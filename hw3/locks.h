#ifndef LOCKS_H_
#define LOCKS_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "../utils/stopwatch.h"
#include "../utils/paddedprim.h"

//macro for counter goal
#define BIG 112//2725408
//remember: to change this, delete .o files first!

typedef struct{
	volatile int* counter;
	void (*lock_fnc)(void*);
	void (*unlock_fnc)(void*);
	void* lock;
	int fair;
	int nthreads;
	int lockmode;
	int* ind_count;
	int* counter_count; //to check mutex
}lockOpArgs;

typedef struct{
	int size;
	int slot;
	volatile int* tail;
	PaddedPrimBool_t* q;
}ALock;

typedef struct{
	PaddedPrimBool_t* tail;
	PaddedPrimBool_t* nodes;
}CLHLock;

typedef struct{
	PaddedPrimBool_t* myNode;
	PaddedPrimBool_t* pred;
	CLHLock* lock;
}myCLH;

void increment(volatile int* counter);

int tryLock(void (*lock_fnc)(void*), void* lock_struct, int lockmode);

/*                   */
/*      TASLock      */
/*                   */

volatile int* TAS();

void TAS_lock(void* arg);

void TAS_unlock(void* arg);

void TAS_destroy(void* arg);

/*                   */
/*   pthreads_mutex  */
/*                   */
void ptm_unlock(void* arg);

/*                   */
/*       ALock       */
/*                   */
ALock* Anderson(int nthreads);

void A_lock(void* arg);

void A_unlock(void* arg);

void A_destroy(void* arg);

/*                   */
/*      CLHLock      */
/*                   */
CLHLock* CLH(int nthreads);

void CLH_lock(void* arg);

void CLH_unlock(void* arg);

void CLH_destroy(void* arg);


void* lockOperator(void* opargs);

int opControl(int nthreads, int lockmode, FILE* out, int fair);

#endif /* LOCKS_H_ */
