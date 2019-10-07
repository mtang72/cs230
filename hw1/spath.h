#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "../utils/stopwatch.h"

//for passing arguments to fl_whsl()
typedef struct{
	int* daddr;
	int st;
	int end;
	int par;
	char* outtest;
}thread_data;

//for printing out the adjacency matrix for debugging purposes
void printarr(int* arr, int len, char* outpath);

//for parsing the input file
int* fparse(char* filenm);

//Floyd-Warshall
void* fl_wshl(void* tdata);

//for handling parallel vs. serial operation
int minpath(int* dist, int nthreads, char* outtest);

//for outputting the final answer
int output(int* dist, char* outpath);