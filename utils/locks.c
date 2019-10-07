#include "locks.h"

void increment(volatile int* counter){
	(*counter)++;
}

int tryLock(void (*lock_fnc)(void*), void* lock_struct, int lockmode){
	//first check if lock is busy
	switch(lockmode){
		case 0:
			if (*((volatile int*)lock_struct) == 1)
				return -1;
			break;
		case 1:
			if (pthread_mutex_trylock((pthread_mutex_t*)lock_struct) == 0)
				return 0;
			else
				return -1;
			break;
		//lord forgive me for my one-liners
		case 2:
			if (!((((ALock*)lock_struct)->q)[*(((ALock*)lock_struct)->tail) & (((ALock*)lock_struct)->size-1)].value))
				return -1;
			break;
		case 3:
			if (((((myCLH*)lock_struct)->lock)->tail)->value){
				return -1;
			}
			break;
	}
	(*lock_fnc)(lock_struct);
	return 0;
}

/*                   */
/*      TASLock      */
/*                   */

volatile int* TAS(){
	volatile int* lock = malloc(sizeof(int));
	*lock = 0;
	return lock;
}

void TAS_lock(void* arg){
	volatile int* lock = (int*)arg;
	while (__sync_lock_test_and_set(lock,1));
}

void TAS_unlock(void* arg){
	volatile int* lock = (int*)arg;
	*lock = 0;
}

void TAS_destroy(void* arg){
	free(arg);
}

/*                   */
/*   pthreads_mutex  */
/*                   */
//wrapper to satisfy typecasting
void ptm_unlock(void* arg){
	pthread_mutex_unlock((pthread_mutex_t*)arg);
}


/*                   */
/*       ALock       */
/*                   */
ALock* Anderson(int nthreads){
	ALock* lock = malloc(sizeof(ALock));
	int mult = 1;
	while (1){ //dwai
		if (mult >= nthreads)
			break;
		mult<<=1;
	}
	lock->size = mult;
	lock->tail = malloc(sizeof(int));
	*(lock->tail) = 0;
	PaddedPrimBool_t* q = malloc(sizeof(PaddedPrimBool_t)*mult);
	q[0].value = true;
	for (int i=1; i<mult; i++){
		q[i].value = false;
	}
	lock->q = q;
	return lock;
}

void A_lock(void* arg){
	ALock* lock = (ALock*)arg;
	int slot = __sync_fetch_and_add(lock->tail,1) & (lock->size - 1);
	PaddedPrimBool_t* q = lock->q;
	while (!(q[slot].value));
	lock->slot = slot;
}

void A_unlock(void* arg){
	ALock* lock = (ALock*)arg;
	PaddedPrimBool_t* q = lock->q;
	int slot = lock->slot;
	q[slot].value = false;
	q[(slot+1)&(lock->size-1)].value = true;
}

void A_destroy(void* arg){
	ALock* lock = (ALock*)arg;
	free((void*)lock->tail);
	free(lock->q);
	free(lock);
}

/*                   */
/*      CLHLock      */
/*                   */
//goddam finally

CLHLock* CLH(int nthreads){
	CLHLock* lock = malloc(sizeof(CLHLock));
	lock->nodes = malloc(sizeof(PaddedPrimBool_t)*(nthreads+1));
	for (int i=0; i<nthreads; i++){
		(lock->nodes)[i].value = false;
	}
	lock->tail = lock->nodes;
	return lock;
}

void CLH_lock(void* arg){
	myCLH* me = (myCLH*)arg;
	PaddedPrimBool_t* myNode = me->myNode;
	CLHLock* lock = me->lock;
	myNode->value = true;
	me->pred = __sync_lock_test_and_set(&(lock->tail), myNode);
	while ((me->pred)->value);
}

void CLH_unlock(void* arg){
	myCLH* me = (myCLH*)arg;
	(me->myNode)->value = false;
	me->myNode = me->pred;
}

void CLH_destroy(void* arg){
	CLHLock* lock = (CLHLock*)arg;
	free(lock->nodes);
	free(lock);
}


void* lockOperator(void* opargs){
	lockOpArgs* args = (lockOpArgs*)opargs;
	volatile int* counter = args->counter;
	void (*lock_fnc)(void*) = args->lock_fnc;
	void (*unlock_fnc)(void*) = args->unlock_fnc;
	void* lock = args->lock;
	int fair = args->fair;
	int nthreads = args->nthreads;
	int* counter_count = args->counter_count;
	int lockmode = args->lockmode;
	int acquired, personal_counter = 0;
	int portion = BIG/nthreads;
	//in fair mode, run till portion done, in unfair mode, run till counter reaches BIG
	while ((fair && personal_counter<portion) || (!fair && *counter<BIG)){
		while((acquired=tryLock(lock_fnc, lock, lockmode)) < 0);
		//!!critical section!!
		sleep(1);
		counter_count[*counter]++;
		if (fair || (!fair && *counter<BIG))
			increment(counter);
		//!!end critical section ! !
		(*unlock_fnc)(lock);
		personal_counter++;
	}
	//*(args->ind_count) = personal_counter;
	return NULL;
}

int opControl(int nthreads, int lockmode, FILE* out, int fair){
	volatile int* counter = malloc(sizeof(int));
	*counter = 0;
	//StopWatch_t swatch;
	//startTimer(&swatch);
	if (nthreads == 0){//serial mode
		while (*counter < BIG){
			increment(counter);
		}
	}
	else { //parallel
		//creating opargs, creating lock and setting lock() and unlock() functions given lockmode
		lockOpArgs masterarg; //just so i don't have to respecify lock & lockfncs for creating individual opargs
		masterarg.lock = NULL;
		masterarg.lock_fnc = NULL;
		masterarg.unlock_fnc = NULL;
		masterarg.counter_count = malloc(sizeof(int)*BIG);
		for (int i=0; i<BIG; i++){
			masterarg.counter_count[i] = 0;
		}
		switch(lockmode){
			case 0: //TASLock
				masterarg.lock_fnc = &TAS_lock;
				masterarg.unlock_fnc = &TAS_unlock;
				masterarg.lock = (void*)TAS();
				break;
			case 1: //pthread_mutex
				masterarg.unlock_fnc = &ptm_unlock;
				masterarg.lock = malloc(sizeof(pthread_mutex_t));
				pthread_mutex_init((pthread_mutex_t*)masterarg.lock, NULL);
				break;
			case 2: //ALock
				masterarg.lock_fnc = &A_lock;
				masterarg.unlock_fnc = &A_unlock;
				masterarg.lock = (void*)Anderson(nthreads);
				break;
			case 3: //CLHLock
				masterarg.lock_fnc = &CLH_lock;
				masterarg.unlock_fnc = &CLH_unlock;
				masterarg.lock = (void*)CLH(nthreads);
				break;
		}
		//thread opargs as well as thread individual counters
		lockOpArgs targs[nthreads];

		//creating threads array along with "joinable" attribute
		pthread_t threads[nthreads];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		for (int t=0; t<nthreads; t++){
			targs[t].counter = counter;
			targs[t].fair = fair;
			targs[t].nthreads = nthreads;
			targs[t].lockmode = lockmode;
			//targs[t].ind_count = malloc(sizeof(int));
			targs[t].counter_count = masterarg.counter_count;
			if (lockmode == 3){
				targs[t].lock = malloc(sizeof(myCLH));
				((myCLH*)(targs[t].lock))->myNode = &((((CLHLock*)(masterarg.lock))->nodes)[t+1]);
				((myCLH*)(targs[t].lock))->lock = (CLHLock*)(masterarg.lock);
			}
			else
				targs[t].lock = masterarg.lock;
			targs[t].lock_fnc = masterarg.lock_fnc;
			targs[t].unlock_fnc = masterarg.unlock_fnc;
			if (pthread_create(&threads[t], &attr, lockOperator, &targs[t]) != 0){
				fprintf(stderr, "Thread %d creation fail\n", t);
				exit(-1);
			}
		}
		//thread join, and have to destroy attr apparently
		for (int t=0; t<nthreads; t++){
			if (pthread_join(threads[t],NULL) != 0){
				fprintf(stderr, "Thread %d join fail\n", t);
			}
		}
		pthread_attr_destroy(&attr);
		
		if (masterarg.lock != NULL){
			switch(lockmode){
				case 0:
					TAS_destroy(masterarg.lock);
					break;
				case 1:
					pthread_mutex_destroy((pthread_mutex_t*)masterarg.lock);
					break;
				case 2:
					A_destroy(masterarg.lock);
					break;
				case 3:
					CLH_destroy(masterarg.lock);
					for (int t=0; t<nthreads; t++){
						free(targs[t].lock);
					}
					break;
			}
		}
		/*int ind, mean = BIG/nthreads;
		unsigned long long varsum = 0;
		for (int t=0; t<nthreads; t++){
			ind = *(targs[t].ind_count);
			//fprintf(out, "Thread %d contributed increments: %d\n", t, ind);
			varsum += (ind-mean)*(ind-mean);
			free(targs[t].ind_count);
		}
		fprintf(out, "Variance of thread work: %llu\n",varsum/((unsigned long long)nthreads));*/
		//use counter_count to check if any two threads accessed the same counter
		for (int i=0; i<BIG; i++){
			if (masterarg.counter_count[i] > 1)
				fprintf(out,"Mutex failure at counter: %d. Number of threads that reached CS: %d\n",i,masterarg.counter_count[i]);
		}
		free(masterarg.counter_count);
	}
	//stopTimer(&swatch);
	fprintf(out, "Counter Final: %d\n", *counter);
	//fprintf(out, "Runtime: %f\n", getElapsedTime(&swatch));
	fprintf(out, "----------------------------\n\n");
	free((void*)counter);
	return 0;
}
