#ifndef PACHAND_H_
#define PACHAND_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include "packhash.h"
#include "../utils/locks.h"
#include "../utils/hashgenerator.h"
#include "../utils/fingerprint.h"

typedef struct{
	volatile int* head;
	volatile int* tail;
	int qdepth;
	HashPacket_t* queue;
}Lamport;

typedef struct{
	int pflag;
	int src;
	int npck; //number of packets per source - to be used only for testing mode
	unsigned long long* chksumsum; //total chksum - correctness testing only
	unsigned long long* opsumsum; //chksumsums for add, remove, remains - correctness testing only
	int nsrc;
	int* indcount;
	void* lock;
	void (*lock_fnc)(void*);
	void (*unlock_fnc)(void*);
	void** all_locks; //awesome strategy only
	float tlim;
	Lamport* all_q; //awesome strategy only
	FILE* out;
	HBox hbox;
}Opargs;

typedef struct{
	int nsrc;
	long pkwk;
	int npck; //testing only
	unsigned long long* chksumsum; //total chksum - correctness testing only
	unsigned long long* opsumsum; //chksumsums for add, remove, remains - correctness testing only
	int pflag;
	int remcorr;
	float tlim;
	float addpor;
	float rempor;
	float hitrate;
	Lamport* lqq;
	FILE* out;
	HBox hbox;
}Dispargs;

int enq(Lamport* q, HashPacket_t* pck);

HashPacket_t deq(Lamport* q);

void* operator(void* opdata);

void* dispatcher(void* args);

int ophandle(Dispargs args, int qdepth, int initsize, int htype);

#endif /* PACHAND_H_ */