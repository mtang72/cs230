#include "locks.h"

int main(int argc, char* argv[]){
	int nthreads = 0;
	int fair = 1;
	int lock = -1;
	char* outpath = NULL;
	char c;
	while ((c = getopt(argc, argv, "n:L:o:f:")) != -1){
		switch(c){
			case 'n':
				nthreads = atoi(optarg);
				if (nthreads < 0 || (nthreads!=0 && BIG%nthreads)){
					fprintf(stderr, "nthreads must be greater than 0 and divide %d cleanly\n", BIG);
					exit(-1);
				}
				break;
			case 'L':
				lock = atoi(optarg);
				if (lock < 0 || lock > 3){
					fprintf(stderr, "-L can only be 0 to 3, for the lock modes\n");
					exit(-1);
				}
				break;
			case 'o':
				outpath = optarg;
				break;
			case 'f':
				fair = atoi(optarg);
				if (fair!=0 && fair!=1){
					fprintf(stderr, "-f can only be 0 or 1, for fairness\n");
					exit(-1);
				}
		}
	}
	if (lock==-1 && nthreads>0){
		fprintf(stderr, "-L specification required in parallel mode\n");
		exit(-1);
	}
	FILE* out = NULL;
	if (outpath){
		out = fopen(outpath, "a");
		if (out == NULL){
			fprintf(stderr, "invalid file directory path\n");
			exit(-1);
		}
	}
	else
		out = stdout;
	fprintf(out, "nthreads: %d lockmode: %d, fairness: %d\n",nthreads,lock,fair);
	fprintf(out, "--begin--\n");
	opControl(nthreads, lock, out, fair);
	if (out != NULL)
		fclose(out);
	return 0;
}
