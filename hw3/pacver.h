#ifndef PACVER_H_
#define PACVER_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include "locks.h"
#include "../utils/stopwatch.h"
#include "../utils/packetsource.h"
#include "../utils/fingerprint.h"

typedef struct{
	volatile int* head;
	volatile int* tail;
	int qdepth;
	volatile Packet_t* queue;
}Lamport;

typedef struct{
	int src;
	int npck; //number of packets per source - to be used only for testing mode
	unsigned long long* chksumsum; //total chksum - correctness testing only
	int nsrc;
	int opmode;
	int lockmode;
	int* indcount;
	void* lock;
	void (*lock_fnc)(void*);
	void (*unlock_fnc)(void*);
	void** all_locks; //awesome strategy only
	float tlim;
	Lamport* lq;
	Lamport* all_q; //awesome strategy only
	FILE* out;
}Opargs;

typedef struct{
	int nsrc;
	int pkwk;
	int npck; //testing only
	unsigned long long* chksumsum; //total chksum - correctness testing only
	int uflag;
	int pflag;
	short seed;
	float tlim;
	Lamport* lqq;
	FILE* out;
}Dispargs;

int enq(Lamport* q, volatile Packet_t* pck);

volatile Packet_t deq(Lamport* q);

void* operator(void* opdata);

void* dispatcher(void* args);

int ophandle(Dispargs args, int qdepth, int lockmode, int opmode);

#endif /* PACVER_H_ */