#ifndef PACKHASH_H_
#define PACKHASH_H_

#include "../utils/hashtable.h"
#include "../utils/hashgenerator.h"
#include "../utils/locks.h"
#include "../utils/fingerprint.h"
#include <errno.h>

typedef struct{
	void* htable;
	void (*add_op)(void**);
	bool (*rem_op)(void**);
	bool (*con_op)(void**);
	void (*del_op)(void**);
}HBox;

typedef struct{
	bool optimistic;
	volatile int* tableSize;
	int locksSize;
	int maxBucketSize;
	volatile int* tableMask;
	int locksMask;
	SerialList_t** table;
	pthread_rwlock_t* locks;
}StripedLockHTable_t;

//serial
void serial_add(void** args);

bool serial_rem(void** args);

bool serial_con(void** args);

void serial_del(void** args);

//striped
StripedLockHTable_t* striped_init(int logSize, int maxBucketSize, bool optimistic);

void striped_add(void** args);

bool striped_rem(void** args);

bool striped_con(void** args);

bool striped_optcon(void** args);

int striped_resize(StripedLockHTable_t* htable, int init_size);

void striped_del(void** args);

void hash_process(HBox hbox, HashPacket_t* pck, unsigned long long* opsumsum, FILE* out);
#endif /* PACKHASH_H_ */