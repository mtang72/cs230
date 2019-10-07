
#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include "seriallist.h"
#include <stdbool.h>

typedef struct {
	int logSize;
	int mask;
	int maxBucketSize;
	int size;
	long currentFill;
	SerialList_t ** table;
}SerialHashTable_t;

SerialHashTable_t * createSerialHashTable(int logSize, int maxBucketSize);

void resizeIfNecessary_ht(SerialHashTable_t * htable,int key);

void addNoCheck_ht(SerialHashTable_t * htable,int key, volatile Packet_t * x,unsigned long long* remsumsum);

void add_ht(SerialHashTable_t * htable,int key, volatile Packet_t * x,unsigned long long* remsumsum);

bool remove_ht(SerialHashTable_t * htable,int key,unsigned long long* remsumsum);

bool contains_ht(SerialHashTable_t * htable,int key);

void resize_ht(SerialHashTable_t * htable);

void print_ht(SerialHashTable_t * htable, FILE* out);



#endif /* HASHTABLE_H_ */
