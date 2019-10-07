#include "spath.h"

pthread_barrier_t barrier;

void printarr(int* arr, int len, char* outpath){
	FILE* out;
	if (outpath)
		out = fopen(outpath, "a");
	else
		out = stdout;
	for (int i=0; i<len; i++){
		fprintf(out,"%d ",arr[i]);
	}
	fprintf(out,"\n-----------------\n");
	if (out != stdout)
		fclose(out);
	return;
}

int* fparse(char* filenm){
	FILE* f = fopen(filenm, "r");
	if (f == NULL){
		fprintf(stderr,"file not found\n");
		exit(-1);
	}
	char buff[256];
	//first: size of vertex set
	int i = fscanf(f,"%s",buff);
	if (i == EOF){
		fprintf(stderr, "no vertex set size found\n");
		exit(-1);
	}
	//creation of dist
	int v = atoi(buff);
	int* dist = (int*)malloc(sizeof(int)*(1+v*v));
	dist[0] = v;
	int ind = 1;
	//continue to read file
	while ((i = fscanf(f,"%s",buff)) != EOF){
		dist[ind] = atoi(buff);
		ind++;
	}
	return dist;
}

void* fl_wshl(void* tdata){
	//unpacking args
	thread_data* args = (thread_data*)tdata;
	int* dist = args->daddr;
	int st = args->st;
	int end = args->end;
	int par = args->par;
	//char* outtest = args->outtest;
	int dsize = dist[0];
	//f-w algorithm
	for (int k=0; k<dsize; k++){
		for (int i=st; i<end; i++){
			for (int j=0; j<dsize; j++){
				int interdist = dist[1+i*dsize+k]+dist[1+k*dsize+j];
				if (dist[1+i*dsize+j] > interdist){
					dist[1+i*dsize+j] = interdist;
				}
			}
		}
		if (par == 1){
			int bf = pthread_barrier_wait(&barrier);
			if (bf == PTHREAD_BARRIER_SERIAL_THREAD)
				continue;
				//printarr(dist, dsize*dsize+1, outtest);
		}
		else
			continue;
			//printarr(dist, dsize*dsize+1, outtest);
	}
	return NULL;
}

int minpath(int* dist, int nthreads, char* outtest){
	int dsize = dist[0];
	StopWatch_t swatch;
	startTimer(&swatch);
	if (nthreads != 0){ //parallel
		//array of tdata args for each thread
		thread_data tdata[nthreads];
		for (int i=0; i<nthreads; i++){
			tdata[i].daddr = dist;
			tdata[i].par = 1;
			tdata[i].outtest = outtest;
		}
		//creating thread "joinable" attribute
		pthread_t threads[nthreads];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		//creating barrier
		unsigned count = nthreads;
		if (pthread_barrier_init(&barrier, NULL, count) != 0){
			fprintf(stderr, "Barrier creation fail\n");
			free(dist);
			exit(-1);
		}
		int tdiv = dsize/nthreads;
		//create threads
		for (int t=0; t<nthreads; t++){
			tdata[t].st = t*tdiv;
			if (t == nthreads-1)
				tdata[t].end = dsize;
			else
				tdata[t].end = tdata[t].st+tdiv;
			if (pthread_create(&threads[t], &attr, fl_wshl, &tdata[t]) != 0){
				fprintf(stderr, "Thread creation fail\n");
				free(dist);
				exit(-1);
			}
		}
		//join threads
		for (int t=0; t<nthreads; t++){
			if (pthread_join(threads[t],NULL) != 0){
				fprintf(stderr, "Thread join fail\n");
				free(dist);
				exit(-1);
			}
		}
		//destroy stuff to keep memory free
		pthread_attr_destroy(&attr);
		pthread_barrier_destroy(&barrier);
	}
	else{ //serial
		thread_data tdata;
		tdata.daddr = dist;
		tdata.st = 0;
		tdata.end = dsize;
		tdata.par = 0;
		tdata.outtest = outtest;
		fl_wshl(&tdata);
	}
	//stop and record time
	stopTimer(&swatch);
	FILE *out;
	if (outtest != NULL)
		out = fopen(outtest,"a");
	else
		out = stdout;
	fprintf(out,"f-w runtime: %f\n", getElapsedTime(&swatch));
	if (outtest != NULL)
		fclose(out);
	return 0;
}

int output(int* dist, char* outpath){
	FILE* out;
	//setting output
	if (outpath != NULL){
		out = fopen(outpath, "w");
		if (out == NULL){
			fprintf(stderr,"unable to open file for some reason\n");
			free(dist);
			exit(-1);
		}
	}
	else{
		out = stdout;
	}
	//printing to output
	int rowplace = 0;
	for (int i=0; i<dist[0]*dist[0]+1; i++){
		if (i==0 || rowplace==dist[0]-1){
			fprintf(out, "%d\n", dist[i]);
			rowplace = 0;
		}
		else{
			fprintf(out, "%d ", dist[i]);
			rowplace++;
		}
	}
	if (out != stdout)
		fclose(out);
	return 0;
}

int main(int argc, char* argv[]){
	int nthreads = 0;
	char* srcfile = NULL;
	char* outfile = NULL;
	char* outtest = NULL;
	int c;
	while ((c = getopt(argc, argv, "s:o:p:t:")) != -1){
		switch(c){
			case 'p':
				nthreads = atoi(optarg);
				if (nthreads == 0){
					fprintf(stderr, "Number of threads given is 0 or not a number\n");
					exit(-1);
				}
				break;
			case 's':
				srcfile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 't':
				outtest = optarg;
				break;
			default:
				exit(-1);
				break;
		}
	}
	if (srcfile==NULL){
		fprintf(stderr, "File directory path required\n");
		exit(-1);
	}
	int* dist = fparse(srcfile);
	minpath(dist,nthreads,outtest);
	/*if (outfile)
		printf("%s\n",outfile);*/
	output(dist,outfile);
	free(dist);
	return 0;
}
