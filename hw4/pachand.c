#include "pachand.h"

int enq(Lamport* q, HashPacket_t* pck){
	int qd = q->qdepth;
	volatile int* tl = q->tail;
	//printf("enq %ld %d %d\n", (long)q, q->head, q->tail);
	if (*tl - *(q->head) == qd)
		return -1;
	HashPacket_t* pq = q->queue;
	pq[*tl % qd] = *pck;
	//fprintf(out, "source: %d chksum: %ld\n", src, getFingerprint(pck->iterations, pck->seed));
	(*tl)++;
	__sync_synchronize();
	return 0;
}

HashPacket_t deq(Lamport* q){
	volatile int* hd = q->head;
	HashPacket_t pck;
	if (*(q->tail) - *hd == 0){
		pck.key = -1;
		return pck;
	}
	int qd = q->qdepth;
	pck = (q->queue)[(*hd) % qd];
	(*hd)++;
	__sync_synchronize();
	return pck;
}

void* operator(void* opdata){
	Opargs* args = (Opargs*)opdata;
	int src = args->src;
	int nsrc = args->nsrc;
	int npck = args->npck;
	int pflag = args->pflag;
	float tlim = args->tlim;
	FILE* out = args->out;
	void* lock = args->lock;
	void** locks = args->all_locks;
	Lamport* lqq = args->all_q;
	void (*lock_fnc)(void*) = args->lock_fnc;
	void (*unlock_fnc)(void*) = args->unlock_fnc;
	HashPacket_t* pck = malloc(sizeof(HashPacket_t));
	pck->key = -1;
	time_t starttime = time(NULL);
	int indcount = 0;
	int acquired = -1;
	unsigned long long mySum = 0;
	while ((npck>=0 && indcount<npck) || (float)difftime(time(NULL), starttime)<tlim){
		//awesome: starting with initialized src num, rotate over all locks until you can grab one or time runs out
		//if the queue is empty, keep rotating, else move on to "crit section" ops
		while ((npck>=0 || (float)difftime(time(NULL), starttime)<tlim) && src<nsrc){
			if ((acquired=tryLock(lock_fnc,locks[src],0)) == 0){
				if ((*pck=deq(&lqq[src])).key >= 0)
					break;
				else
					(*unlock_fnc)(locks[src]);
			}
			src++;
			if (src == nsrc)
				src = 0; 
		}
		//critical section of awesome strat!
		if (acquired == 0){
			(*unlock_fnc)(locks[src]);
			acquired = -1;
		}
		//for ParNoLoad correctness only
		//mySum += getFingerprint(pck->body->iterations, pck->body->seed);

		if (pck->key>=0 && pflag==1){
			mySum += getFingerprint(pck->body->iterations, pck->body->seed);
			if (pck->type!=Add)
				free((void*)(pck->body));
			hash_process(args->hbox, pck, args->opsumsum, out);
		}
		indcount++;
		//end critical section!

		//only free if we actually got a lock in the first place
		if (acquired==0){
			(*unlock_fnc)(lock);
			acquired = -1;
		}
		pck->key = -1;
	}
	free(pck);
	__sync_fetch_and_add(args->chksumsum, mySum);
	*(args->indcount) = indcount;
	return NULL;
}

void* dispatcher(void* ddata){
	Dispargs* args = (Dispargs*)ddata;
	int nsrc = args->nsrc;
	double tlim = args->tlim;
	long pkwk = args->pkwk;
	int pflag = args->pflag;
	float addpor = args->addpor;
	float rempor = args->rempor;
	float hitrate = args->hitrate;
	unsigned long long mySum = 0;
	Lamport* lqq = args->lqq;
	FILE* out = args->out;
	int npck = args->npck;
	int remcorr = args->remcorr;
	//packet source init
	HashPacketGenerator_t* psource = createHashPacketGenerator(addpor, rempor, hitrate, pkwk);
	int src = 0;
	//dispatch
	time_t starttime = time(NULL);
	//count number of packets
	long totct = 0;
	int limit = 0;
	int addct = 0;
	if (remcorr && npck!=-1)
		limit = npck*nsrc;
	while((npck>=0 && totct<(npck*nsrc)) || (float)difftime(time(NULL), starttime) < tlim){
		/*if (cts[src]>=npck){
			src++;
			continue;
		}*/
		//packet retrieval
		HashPacket_t* pck = NULL;
		if (remcorr){
			if (addct < limit/2){
				pck = getAddPacket(psource);
				addct++;
			}
			else{
				pck = getRemovePacket(psource);
			}
		}
		else
			pck = getRandomPacket(psource);

		if (pflag == 0){ //serial, checksum here
			mySum += getFingerprint(pck->body->iterations, pck->body->seed);
			if (pck->type!=Add){
				free((void*)(pck->body));
			}
			hash_process(args->hbox,pck,args->opsumsum,out);
		}
		else { //parallel
			while((npck>=0 || (float)difftime(time(NULL), starttime)<tlim) && enq(&lqq[src], pck)<0);
		}
		free(pck);
		totct++;
		if (src == nsrc-1)
			src = 0;
		else
			src++;
	}
	deleteHashPacketSource(psource);
	fprintf(out, "Total number of packets dispatched: %ld\n", totct);
	if (pflag == 0){
		fprintf(out, "Total number of packets processed: %ld\n", totct);
		*(args->chksumsum) += mySum;
	}
	return NULL;
}

int ophandle(Dispargs args, int qdepth, int initsize, int htype){
	//for testing; matching checksums
	unsigned long long* sumsum = malloc(sizeof(unsigned long long));
	*(sumsum) = 0;
	//chksums for add, remove, remains, number of calls for contains, contains-hits - indices 0, 1, 2, 3, 4 respectively
	unsigned long long* opsumsum = malloc(4*sizeof(unsigned long long));
	for (int i=0; i<4; i++){
		opsumsum[i] = 0;
	}
	//hash toolbox, table setup
	int logSize = 0;
	while ((1<<logSize) < args.nsrc){
		logSize++;
	}
	HBox hbox;
	switch(htype){
		case 0: //serial
			hbox.htable = (void*)createSerialHashTable(logSize, 3);
			hbox.add_op = &serial_add;
			hbox.rem_op = &serial_rem;
			hbox.con_op = &serial_con;
			hbox.del_op = &serial_del;
			break;
		case 1: //striped
			hbox.htable = (void*)striped_init(logSize,3,false);
			hbox.add_op = &striped_add;
			hbox.rem_op = &striped_rem;
			hbox.con_op = &striped_con;
			hbox.del_op = &striped_del;
			break;
		case 2: //optimistic
			hbox.htable = (void*)striped_init(logSize,3,true);
			hbox.add_op = &striped_add;
			hbox.rem_op = &striped_rem;
			hbox.con_op = &striped_optcon; //for optimistic over striped overhead testing: &striped_con;
			hbox.del_op = &striped_del;
			break;
	}
	if (args.pflag == 0){ //serial
		args.chksumsum = sumsum;
		args.opsumsum = opsumsum;
		args.hbox = hbox;
		dispatcher(&args);
	}
	else{ //parallel
		//malloc'ing array of Lamport queues
		args.lqq = malloc(sizeof(Lamport)*args.nsrc);
		for (int i=0; i<args.nsrc; i++){
			args.lqq[i].head = malloc(sizeof(int));
			args.lqq[i].tail = malloc(sizeof(int));
			*(args.lqq[i].head) = 0;
			*(args.lqq[i].tail) = 0;
			args.lqq[i].queue = malloc(sizeof(HashPacket_t)*qdepth);
			args.lqq[i].qdepth = qdepth;
		}
		//creating threads array along with "joinable" attribute
		pthread_t threads[args.nsrc+1];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		//operator thread setup
		Opargs opargs[args.nsrc];
		int* indcount = malloc(sizeof(int)*args.nsrc);
		//to hold all the locks for all da queues
		void** locks = NULL;
		locks = malloc(sizeof(void*)*args.nsrc);

		for (int t=1; t<args.nsrc+1; t++){
			opargs[t-1].tlim = args.tlim;
			opargs[t-1].src = t-1;
			opargs[t-1].npck = args.npck;
			opargs[t-1].pflag = args.pflag;
			opargs[t-1].chksumsum = sumsum;
			opargs[t-1].opsumsum = opsumsum;
			indcount[t-1] = 0;
			opargs[t-1].indcount = &indcount[t-1];
			opargs[t-1].nsrc = args.nsrc;
			opargs[t-1].out = args.out;
			//lock stuff; hopefully this is right
			opargs[t-1].lock_fnc = &TAS_lock;
			opargs[t-1].unlock_fnc = &TAS_unlock;
			locks[t-1] = (void*)TAS();
			opargs[t-1].all_q = args.lqq;
			opargs[t-1].all_locks = locks;
			opargs[t-1].hbox = hbox;
		}
		//dispatcher thread launch
		if (pthread_create(&threads[0], &attr, dispatcher, &args) != 0){
			fprintf(stderr, "Dispatcher thread creation fail\n");
			for (int i=0; i<args.nsrc; i++){
				free((void*)args.lqq[i].queue);
				free((void*)args.lqq[i].head);
				free((void*)args.lqq[i].tail);
			}
			free(args.lqq);
			exit(-1);
		}
		//operator thread launch
		for (int t=1; t<args.nsrc+1; t++){
			if (pthread_create(&threads[t], &attr, operator, &opargs[t-1]) != 0){
				fprintf(stderr, "Thread creation fail\n");
				for (int i=0; i<args.nsrc; i++){
					free((void*)args.lqq[i].queue);
					free((void*)args.lqq[i].head);
					free((void*)args.lqq[i].tail);
				}
				free(args.lqq);
				exit(-1);
			}
		}
		//join threads
		for (int t=0; t<args.nsrc+1; t++){
			if (pthread_join(threads[t],NULL) != 0){
				fprintf(stderr, "Thread %d join fail\n", t);
			}
		}
		long total_pax = 0;
		//print individual threads packets processed
		for (int t=0; t<args.nsrc; t++){
			fprintf(args.out, "Thread %d Processed: %d packets\n", t, indcount[t]);
			total_pax += (long)(indcount[t]);
		}
		fprintf(args.out, "Total number of packets processed: %ld\n", total_pax);
		free(indcount);
		for (int i=0; i<args.nsrc; i++){
			free((void*)args.lqq[i].queue);
			free((void*)args.lqq[i].head);
			free((void*)args.lqq[i].tail);
		}
		free(args.lqq);
		pthread_attr_destroy(&attr);

		//free lock structures
		for (int t=0; t<args.nsrc; t++){
			TAS_destroy(locks[t]);
		}
		free(locks);
	}
	int tbsize = 0;
	if (htype == 0)
		tbsize = ((SerialHashTable_t*)hbox.htable)->size;
	else
		tbsize = *(((StripedLockHTable_t*)hbox.htable)->tableSize);
	fprintf(args.out, "Final Table Size: %d\n", tbsize);
	fprintf(args.out, "Checksum Total: %llu\n", *sumsum);
	void* del_args[2];
	del_args[0] = hbox.htable;
	del_args[1] = &opsumsum[2];
	(*(hbox.del_op))(del_args);
	if (args.pflag != 2){
		fprintf(args.out, "Add Checksum Total: %llu\nRemove Checksum Total: %llu\n"
			"Remains Checksum Total: %llu\n",opsumsum[0],opsumsum[1],opsumsum[2]);
		fprintf(args.out, "Add equals Remove + Remains? %s\n", 
			(opsumsum[0]==opsumsum[1]+opsumsum[2]) ? "Yes":"No");
		fprintf(args.out, "Total Contains Hits: %llu\n", opsumsum[3]);
	}
	free(sumsum);
	free(opsumsum);
	return 0;
}

int main(int argc, char* argv[]){
	//argparsing
	int nsrc = -1;
	int timelim = -1;
	long pkwk = -1;
	float addpor = -1.0;
	float rempor = -1.0;
	float hitrate = -1.0;
	int pflag = 0;
	int htype = 0;
	int qdepth = 8;
	int npck = -1;
	int initsize = 0;
	int remcorr = 0;
	char* outdir = NULL;
	char c;
	FILE* out = NULL;
	while ((c = getopt(argc, argv, "n:M:T:D:R:A:W:s:o:h:H:p:d:")) != -1){
		switch(c){
			case 'n':
				nsrc = atoi(optarg);
				if (nsrc <= 0){
					fprintf(stderr,"cannot have less than or equal to 0 sources\n");
					exit(-1);
				}
				break;
			case 'M':
				timelim = atoi(optarg);
				break;
			case 'T':
				npck = atoi(optarg);
				if (npck < 0){
					fprintf(stderr,"cannot have less than 0 packets per source\n");
					exit(-1);
				}
				break;
			case 'D':
				qdepth = atoi(optarg);
				break;
			case 'R':
				rempor = atof(optarg);
				break;
			case 'A':
				addpor = atof(optarg);
				break;
			case 'W':
				pkwk = atol(optarg);
				break;
			case 's':
				initsize = atoi(optarg);
				if (initsize < 0){
					fprintf(stderr,"initsize cannot be less than 0\n");
					exit(-1);
				}
				break;
			case 'o':
				outdir = optarg;
				break;
			case 'h':
				hitrate = atof(optarg);
				break;
			case 'H':
				htype = atoi(optarg);
				break;
			case 'p':
				pflag = atoi(optarg);
				if (pflag<0 || pflag>2){
					fprintf(stderr,"invalid -p, only take 0 for serial, 1 for parallel, 2 for parallel-no-load\n");
					exit(-1);
				}
				break;
			case 'd':
				remcorr = atoi(optarg);
				break;
		}
	}
	if (nsrc<0 || (timelim*npck)>0 || pkwk<0){
		fprintf(stderr,"number of sources -n, mean packet work -W\n");
		fprintf(stderr,"timelim -M XOR npck -T required\n");
		exit(-1);
	}
	if (hitrate<=0 || addpor<0 || rempor<0 || addpor+rempor>1){
		fprintf(stderr,"hitrate, probability of add/remove need to be positive values\n");
		fprintf(stderr,"add/remove probability must add to at most 1\n");
		exit(-1);
	}
	if (htype<0 || htype>2 || (pflag==1 && htype==0) || (pflag==0 && htype!=0)){
		fprintf(stderr,"htype must be 0 (serial), 1 (striped lock), 2 (lock-free), must be 0 if pflag=0 and cannot be 0 otherwise\n");
		exit(-1);
	}
	Dispargs args;
	args.nsrc = nsrc;
	args.pkwk = pkwk;
	args.npck = npck;
	args.addpor = addpor;
	args.rempor = rempor;
	args.remcorr = remcorr;
	args.hitrate = hitrate;
	args.tlim = (float)timelim/1000;
	args.pflag = pflag;
	//open output directory
	if (outdir != NULL){
		FILE* out = fopen(outdir,"a");
		if (out == NULL){
			fprintf(stderr, "invalid file directory path\n");
			exit(-1);
		}
		args.out = out;
	}
	else
		args.out = stdout;
	fprintf(args.out, "nsrc: %d, pkwk: %ld, npck: %d, tlim: %f\naddpor: %f, rempor: %f, hitrate: %f\npflag: %d, "\
		"qdepth: %d, htype: %d\n",args.nsrc,args.pkwk,args.npck,args.tlim,args.addpor,args.rempor,args.hitrate,\
		args.pflag,qdepth,htype);
	fprintf(args.out, "-------begin session-------\n");
	ophandle(args,qdepth,initsize,htype);
	fprintf(args.out, "-------end session-------\n\n");
	if (out != NULL)
		fclose(out);
	return 0;
}
