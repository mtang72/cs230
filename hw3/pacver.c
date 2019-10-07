#include "pacver.h"

int enq(Lamport* q, volatile Packet_t* pck){
	int qd = q->qdepth;
	volatile int* tl = q->tail;
	//printf("enq %ld %d %d\n", (long)q, q->head, q->tail);
	if (*tl - *(q->head) == qd)
		return -1;
	volatile Packet_t* pq = q->queue;
	pq[*tl % qd] = *pck;
	//fprintf(out, "source: %d chksum: %ld\n", src, getFingerprint(pck->iterations, pck->seed));
	(*tl)++;
	__sync_synchronize();
	return 0;
}

volatile Packet_t deq(Lamport* q){
	volatile int* hd = q->head;
	volatile Packet_t pck;
	if (*(q->tail) - *hd == 0){
		pck.iterations = -1;
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
	int lockmode = args->lockmode;
	int opmode = args->opmode;
	float tlim = args->tlim;
	Lamport* lq = args->lq;
	unsigned long long* sumsum = args->chksumsum;
	//FILE* out = args->out;
	void* lock = args->lock;
	void** locks = args->all_locks;
	Lamport* lqq = args->all_q;
	void (*lock_fnc)(void*) = args->lock_fnc;
	void (*unlock_fnc)(void*) = args->unlock_fnc;
	volatile Packet_t* pck = malloc(sizeof(Packet_t));
	pck->iterations = -1;
	time_t starttime = time(NULL);
	int indcount = 0;
	int acquired = -1;
	unsigned long long mySum = 0;
	while ((npck>=0 && indcount<npck) || (float)difftime(time(NULL), starttime)<tlim){
		if (opmode > 0){
			if (opmode == 1) //homequeue: spin in place until you can grab the lock or until time runs out
				while ((npck>=0 || (float)difftime(time(NULL), starttime)<tlim) && (acquired=tryLock(lock_fnc,lock,lockmode))<0);
			else{ //awesome: starting with initialized src num, rotate over all locks until you can grab one or time runs out
				//if the queue is empty, keep rotating, else move on to "crit section" ops
				while ((npck>=0 || (float)difftime(time(NULL), starttime)<tlim) && src<nsrc){
					if ((acquired=tryLock(lock_fnc,locks[src],lockmode)) == 0){
						if ((*pck=deq(&lqq[src])).iterations >= 0)
							break;
						else
							(*unlock_fnc)(locks[src]);
					}
					src++;
					if (src == nsrc)
						src = 0;
				} 
			}
		}
		//critical section if not awesome strategy!
		if (opmode < 2){
			while((npck>=0 || (float)difftime(time(NULL), starttime)<tlim) && (*pck=deq(lq)).iterations<0);
			if (pck->iterations>=0){
				mySum += getFingerprint(pck->iterations, pck->seed);
			}
		}
		//end critical section!! 8498257269129 8369681655855
		//critical section of awesome strat!
		else{
			if (acquired == 0){
				(*unlock_fnc)(locks[src]);
				acquired = -1;
			}
			if (pck->iterations>=0)
				mySum += getFingerprint(pck->iterations, pck->seed);
		}
		indcount++;
		//only free if we actually got a lock in the first place
		if (acquired==0 && opmode==1){
			(*unlock_fnc)(lock);
			acquired = -1;
		}
		pck->iterations = -1;
	}
	free((void*)pck);
	*(args->indcount) = indcount;
	*(sumsum) += mySum;
	return NULL;
}

void* dispatcher(void* ddata){
	Dispargs* args = (Dispargs*)ddata;
	int nsrc = args->nsrc;
	double tlim = args->tlim;
	int pkwk = args->pkwk;
	int uflag = args->uflag;
	int pflag = args->pflag;
	unsigned long long* sumsum = args->chksumsum;
	unsigned long long mySum = 0;
	short seed = args->seed;
	Lamport* lqq = args->lqq;
	FILE* out = args->out;
	int npck = args->npck;
	//packet source init
	PacketSource_t* psource = createPacketSource((long)pkwk, nsrc, seed);
	int src = 0;
	//dispatch
	time_t starttime = time(NULL);
	//count number of packets
	long totct = 0;
	while((npck>=0 && totct<(npck*nsrc)) || (float)difftime(time(NULL), starttime) < tlim){
		/*if (cts[src]>=npck){
			src++;
			continue;
		}*/
		//packet retrieval
		volatile Packet_t* pck = NULL;
		switch(uflag){
			case 0:
				pck = getExponentialPacket(psource, src);
				break;
			case 1:
				pck = getUniformPacket(psource, src);
				break;
		}
		if (pflag == 0){ //serial, checksum here
			mySum += getFingerprint(pck->iterations, pck->seed);
		}
		else { //parallel
			while((npck>=0 || (float)difftime(time(NULL), starttime)<tlim) && enq(&lqq[src], pck)<0);
		}
		free((void*)pck);
		totct++;
		if (src == nsrc-1)
			src = 0;
		else
			src++;
	}
	deletePacketSource(psource);
	fprintf(out, "Total number of packets dispatched: %ld\n", totct);
	if (pflag == 0){
		fprintf(out, "Total number of packets processed: %ld\n", totct);
		*(sumsum) += mySum;
	}
	return NULL;
}

int ophandle(Dispargs args, int qdepth, int lockmode, int opmode){
	/*StopWatch_t swatch;
	startTimer(&swatch);*/
	//for testing; matching checksums
	unsigned long long* sumsum = NULL;
	if (args.pflag == 0){ //serial
		sumsum = malloc(sizeof(unsigned long long));
		*(sumsum) = 0;
		args.chksumsum = sumsum;
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
			args.lqq[i].queue = malloc(sizeof(Packet_t)*qdepth);
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
		if (opmode > 0)
			locks = malloc(sizeof(void*)*args.nsrc);
		//for CLHLock creation
		CLHLock* CLock[args.nsrc];
		//for myCLH handling under awesome mode
		myCLH* myCs[args.nsrc][args.nsrc];
		//sumsum changes to have a slot for every node
		sumsum = malloc(sizeof(unsigned long long)*args.nsrc);
		for (int t=1; t<args.nsrc+1; t++){
			opargs[t-1].tlim = args.tlim;
			opargs[t-1].src = t-1;
			opargs[t-1].lockmode = lockmode;
			opargs[t-1].npck = args.npck;
			opargs[t-1].chksumsum = &sumsum[t-1];
			opargs[t-1].opmode = opmode;
			indcount[t-1] = 0;
			opargs[t-1].indcount = &indcount[t-1];
			opargs[t-1].lq = &args.lqq[t-1];
			opargs[t-1].nsrc = args.nsrc;
			opargs[t-1].out = args.out;
			if (opmode > 0){
				switch(lockmode){
					case 0: //TASLock
						opargs[t-1].lock_fnc = &TAS_lock;
						opargs[t-1].unlock_fnc = &TAS_unlock;
						locks[t-1] = (void*)TAS();
						break;
					case 1: //pthread_mutex
						opargs[t-1].unlock_fnc = &ptm_unlock;
						locks[t-1] = malloc(sizeof(pthread_mutex_t));
						pthread_mutex_init((pthread_mutex_t*)(locks[t-1]), NULL);
						break;
					case 2: //ALock
						opargs[t-1].lock_fnc = &A_lock;
						opargs[t-1].unlock_fnc = &A_unlock;
						locks[t-1] = (void*)Anderson(args.nsrc);
						break;
					case 3: //CLHLock
						opargs[t-1].lock_fnc = &CLH_lock;
						opargs[t-1].unlock_fnc = &CLH_unlock;
						CLock[t-1] = CLH(args.nsrc);
						if (opmode == 1){
							locks[t-1] = malloc(sizeof(myCLH));
							((myCLH*)(locks[t-1]))->myNode = &(((CLock[t-1])->nodes)[t]);
							((myCLH*)(locks[t-1]))->lock = CLock[t-1];
						}
						break;
				}
				if (opmode == 1)
					opargs[t-1].lock = locks[t-1];
				else{
					opargs[t-1].all_q = args.lqq;
					if (lockmode != 3)
						opargs[t-1].all_locks = locks;
					else{
						opargs[t-1].all_locks = (void**)(myCs[t-1]);
					}
				}
			}
		}
		if (opmode==2 && lockmode==3){
			for (int t=1; t<args.nsrc+1; t++){
				for (int i=0; i<args.nsrc; i++){
					myCs[t-1][i] = malloc(sizeof(myCLH));
					(myCs[t-1][i])->myNode = &(((CLock[i])->nodes)[t]);
					(myCs[t-1][i])->lock = CLock[i];
				}
			}
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
		if (opmode > 0){
			for (int t=0; t<args.nsrc; t++){
				switch(lockmode){
					case 0:
						TAS_destroy(locks[t]);
						break;
					case 1:
						pthread_mutex_destroy((pthread_mutex_t*)locks[t]);
						free(locks[t]);
						break;
					case 2:
						A_destroy(locks[t]);
						break;
					case 3:
						CLH_destroy((void*)CLock[t]);
						if (opmode == 1)
							free(locks[t]);
						else{
							for (int i=0; i<args.nsrc; i++){
								free(myCs[t][i]);
							}
						}
						break;
				}
			}
			free(locks);
		}
		//summing up the sumsum
		for (int t=1; t<args.nsrc; t++){
			sumsum[0] += sumsum[t];
		}
	}
	//stopTimer(&swatch);
	fprintf(args.out, "Checksum Total: %llu\n", sumsum[0]);
	free(sumsum);
	//fprintf(args.out, "Total program runtime: %f\n", getElapsedTime(&swatch));
	return 0;
}

int main(int argc, char* argv[]){
	//argparsing
	int nsrc = -1;
	int timelim = -1;
	int pkwk = -1;
	int rngseed = INT_MIN;
	int lockmode = -1;
	int pflag = 0;
	int qdepth = 8;
	int uflag = 1;
	int opmode = -1;
	int npck = -1;
	char* outdir = NULL;
	char c;
	FILE* out = NULL;
	while ((c = getopt(argc, argv, "n:M:D:W:u:S:L:o:x:p:T:")) != -1){
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
			case 'D':
				qdepth = atoi(optarg);
				break;
			case 'W':
				pkwk = atoi(optarg);
				break;
			case 'u':
				uflag = atoi(optarg);
				if (uflag!=0 && uflag!=1){
					fprintf(stderr, "invalid -u, only take 0 or 1\n");
					exit(-1);
				}
				break;
			case 'S':
				opmode = atoi(optarg);
				if (opmode < 0 || opmode > 2){
					fprintf(stderr, "invalid opmode, only take 0-2\n");
					exit(-1);
				}
				break;
			case 'L':
				lockmode = atoi(optarg);
				if (lockmode < 0 || lockmode > 3){
					fprintf(stderr, "invalid lockmode, only take 0-3\n");
					exit(-1);
				}
				break;
			case 'o':
				outdir = optarg;
				break;
			case 'x':
				rngseed = atoi(optarg);
				break;
			case 'p':
				pflag = atoi(optarg);
				if (pflag!=0 && pflag!=1){
					fprintf(stderr,"invalid -p, only take 0 or 1\n");
					exit(-1);
				}
				break;
		}
	}
	if (nsrc<0 || (timelim*npck)>0 || pkwk<0 || rngseed==INT_MIN){
		fprintf(stderr,"number of sources -n, mean packet work -W, random seed -x all required\n");
		fprintf(stderr,"timelim -M XOR npck -T required\n");
		exit(-1);
	}
	if (pflag && ((opmode>0&&lockmode<0) || opmode<0)){
		fprintf(stderr,"in parallel mode, strategy -S required and locktype -L required if -S != 0\n");
		exit(-1);
	}
	Dispargs args;
	args.nsrc = nsrc;
	args.pkwk = pkwk;
	args.npck = npck;
	args.seed = (short)rngseed;
	args.tlim = (float)timelim/1000;
	args.uflag = uflag;
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
	fprintf(args.out, "nsrc: %d, pkwk: %d, npck: %d, seed: %hd, tlim: %f\nuflag: %d, pflag: %d, "\
		"qdepth: %d, lockmode: %d, opmode: %d\n",args.nsrc,args.pkwk,args.npck,args.seed,args.tlim,args.uflag,\
		args.pflag,qdepth,lockmode,opmode);
	fprintf(args.out, "-------begin session-------\n");
	ophandle(args,qdepth,lockmode,opmode);
	fprintf(args.out, "-------end session-------\n\n");
	if (out != NULL)
		fclose(out);
	return 0;
}
