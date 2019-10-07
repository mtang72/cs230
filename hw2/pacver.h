#ifndef PACVER_H_
#define PACVER_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "../utils/stopwatch.h"
#include "../utils/packetsource.h"
#include "../utils/fingerprint.h"

typedef struct{
	volatile int head;
	volatile int tail;
	int qdepth;
	volatile Packet_t* queue;
}Lamport;

typedef struct{
	int npck;
	int src;
	Lamport* lq;
	FILE* out;
}Opargs;

typedef struct{
	int nsrc;
	int npck;
	int pkwk;
	int dstr;
	int opmode;
	Lamport* lqq;
	FILE* out;
}Dispargs;

int enq(Lamport* q, volatile Packet_t* pck, int src, FILE* out);

volatile Packet_t deq(Lamport* q);

void* operator(void* opdata);

void* dispatcher(void* args);

int ophandle(Dispargs args, int qdepth);

#endif /* PACVER_H_ */