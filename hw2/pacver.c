#include "pacver.h"

int enq(Lamport* q, volatile Packet_t* pck, int src, FILE* out){
	int qd = q->qdepth;
	volatile int tl = q->tail;
	//printf("enq %ld %d %d\n", (long)q, q->head, q->tail);
	while (tl - q->head == qd);
	volatile Packet_t* pq = q->queue;
	pq[tl % qd] = *pck;
	//fprintf(out, "source: %d chksum: %ld\n", src, getFingerprint(pck->iterations, pck->seed));
	q->tail++;
	__sync_synchronize();
	return 0;
}

volatile Packet_t deq(Lamport* q){
	volatile int hd = q->head;
	while (q->tail - hd == 0);
	int qd = q->qdepth;
	volatile Packet_t pck = (q->queue)[hd % qd];
	q->head++;
	__sync_synchronize();
	return pck;
}

void* operator(void* opdata){
	Opargs* args = (Opargs*)opdata;
	//int src = args->src;
	int npck = args->npck;
	Lamport* lq = args->lq;
	//FILE* out = args->out;
	volatile Packet_t pck;
	//long finger;
	for (int i=0; i<npck; i++){
		pck = deq(lq);
		//finger = 
		getFingerprint(pck.iterations, pck.seed);
		//fprintf(out, "source: %d chksum: %ld\n", src, finger);
	}
	return NULL;
}

void* dispatcher(void* ddata){
	Dispargs* args = (Dispargs*)ddata;
	int nsrc = args->nsrc;
	int npck = args->npck;
	int pkwk = args->pkwk;
	int dstr = args->dstr;
	int opmode = args->opmode;
	Lamport* lqq = args->lqq;
	FILE* out = args->out;

	//packet source init
	PacketSource_t* psource = createPacketSource((long)pkwk, nsrc, (short)23);
	int src = 0;
	//to keep track of number of packets processed for each source
	int cts[nsrc];
	for (int i=0; i<nsrc; i++){
		cts[i] = 0;
	}
	//dispatch
	while(src < nsrc){
		if (cts[src]>=npck){
			src++;
			continue;
		}
		//packet retrieval
		volatile Packet_t* pck;
		switch(dstr){
			case 0:
				pck = getConstantPacket(psource, src);
				break;
			case 1:
				pck = getUniformPacket(psource, src);
				break;
			case 2:
				pck = getExponentialPacket(psource, src);
				break;
		}
		if (opmode == 0){ //serial, checksum here
			//long finger=
			getFingerprint(pck->iterations, pck->seed);
			//fprintf(out, "source: %d chksum: %ld\n", src, finger);
		}
		else { //parallel and serial-queue, push to lamport queue for operator to checksum
			//push to operator queue, spin if unsuccessful to wait for queue empty
			enq(&lqq[src], pck, src, out);
			/*serial-queue: call operator to run checksum on queue
				to make this faster: enable the dispatcher to fill up the queue first then empty it
				but then you have to be able to deq remaining packets at the end if npck%qdepth != 0*/
			if (opmode == 1){
				Opargs opargs;
				opargs.npck = 1;
				opargs.lq = &lqq[src];
				opargs.src = src;
				opargs.out = out;
				operator(&opargs);
			}
		}
		free((void*)pck);
		cts[src]++;
		if (src == nsrc-1)
			src = 0;
		else
			src++;
	}
	deletePacketSource(psource);
	return NULL;
}

int ophandle(Dispargs args, int qdepth){
	StopWatch_t swatch;
	startTimer(&swatch);
	if (args.opmode == 0){ //serial
		dispatcher(&args);
	}
	else{ //serial-queue & parallel
		//malloc'ing array of Lamport queues
		args.lqq = malloc(sizeof(Lamport)*args.nsrc);
		for (int i=0; i<args.nsrc; i++){
			args.lqq[i].head = 0;
			args.lqq[i].tail = 0;
			args.lqq[i].queue = malloc(sizeof(Packet_t)*qdepth);
			args.lqq[i].qdepth = qdepth;
		}
		//creating threads array along with "joinable" attribute
		pthread_t threads[args.nsrc+1];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		//first, dispatcher thread
		if (pthread_create(&threads[0], &attr, dispatcher, &args) != 0){
			fprintf(stderr, "Dispatcher thread creation fail\n");
			for (int i=0; i<args.nsrc; i++){
				free((void*)args.lqq[i].queue);
			}
			free(args.lqq);
			exit(-1);
		}
		//next, operator threads - parallel mode only
		if (args.opmode == 2){
			Opargs opargs[args.nsrc];
			for (int t=1; t<args.nsrc+1; t++){
				opargs[t-1].npck = args.npck;
				opargs[t-1].src = t-1;
				opargs[t-1].lq = &args.lqq[t-1];
				opargs[t-1].out = args.out;
				if (pthread_create(&threads[t], &attr, operator, &opargs[t-1]) != 0){
					fprintf(stderr, "Operator thread creation fail\n");
					for (int i=0; i<args.nsrc; i++){
						free((void*)args.lqq[i].queue);
					}
					free(args.lqq);
					exit(-1);
				}
			}
			//join threads
			for (int t=0; t<args.nsrc; t++){
				if (pthread_join(threads[t],NULL) != 0){
					fprintf(stderr, "Thread %d join fail\n", t);
				}
			}
		}
		//join dispatcher thread (serial-queue)
		else{
			if (pthread_join(threads[0],NULL) != 0){
				fprintf(stderr, "Thread 0 join fail\n");
			}
		}
		for (int i=0; i<args.nsrc; i++){
			free((void*)args.lqq[i].queue);
		}
		free(args.lqq);
		pthread_attr_destroy(&attr);
	}
	stopTimer(&swatch);
	fprintf(args.out, "runtime: %f\n", getElapsedTime(&swatch));
	return 0;
}

int main(int argc, char* argv[]){
	//argparsing
	int nsrc, npck, pkwk;
	int qdepth = 32;
	int dstr = 0;
	int opmode = 0;
	char* outdir = NULL;
	char c;
	FILE* out = NULL;
	while ((c = getopt(argc, argv, "n:T:D:W:s:m:o:")) != -1){
		switch(c){
			case 'n':
				nsrc = atoi(optarg);
				if (nsrc <= 0){
					fprintf(stderr,"cannot have less than or equal to 0 sources\n");
					exit(-1);
				}
				break;
			case 'T':
				npck = atoi(optarg);
				break;
			case 'D':
				qdepth = atoi(optarg);
				break;
			case 'W':
				pkwk = atoi(optarg);
				break;
			case 's':
				dstr = atoi(optarg);
				if (dstr < 0 || dstr > 2){
					fprintf(stderr, "invalid dstr, only take 0-2\n");
					exit(-1);
				}
				break;
			case 'm':
				opmode = atoi(optarg);
				if (opmode < 0 || opmode > 2){
					fprintf(stderr, "invalid opmode, only take 0-2\n");
					exit(-1);
				}
				break;
			case 'o':
				outdir = optarg;
				break;
			default:
				exit(-1);
				break;
		}
	}
	if (!nsrc || !npck || !pkwk){
		fprintf(stderr,"number of sources -n, number of packets per source -T, packet work -W all required\n");
		exit(-1);
	}
	Dispargs args;
	args.nsrc = nsrc;
	args.pkwk = pkwk;
	args.npck = npck;
	args.dstr = dstr;
	args.opmode = opmode;
	//open output directory
	if (outdir != NULL){
		FILE* out = fopen(outdir,"w");
		if (out == NULL){
			fprintf(stderr, "invalid file directory path\n");
			exit(-1);
		}
		args.out = out;
	}
	else
		args.out = stdout;
	ophandle(args,qdepth);
	if (out != NULL)
		fclose(out);
	return 0;
}
