#include "packhash.h"

void serial_add(void** args){
	SerialHashTable_t* htable = (SerialHashTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	unsigned long long* addsumsum = (unsigned long long*)args[2];
	unsigned long long* remsumsum = (unsigned long long*)args[4];
	//FILE *out = (FILE*)args[3];
	__sync_fetch_and_add(addsumsum, getFingerprint(pck->body->iterations, pck->body->seed));
	add_ht(htable, pck->key, pck->body, remsumsum);
	//for contains correctness testing only
	//fprintf(out, "Add %d\n", pck->key);
}

bool serial_rem(void** args){
	SerialHashTable_t* htable = (SerialHashTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	unsigned long long* remsumsum = (unsigned long long*)args[2];
	//FILE *out = (FILE*)args[3];
	bool ret = remove_ht(htable, pck->key, remsumsum);
	//for contains correctness testing only
	//fprintf(out, "Remove %d %s\n", pck->key, ret ? "Success":"Fail");
	return ret;
}

bool serial_con(void** args){
	SerialHashTable_t* htable = (SerialHashTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	//FILE *out = (FILE*)args[3];
	bool ret = contains_ht(htable, pck->key);
	//for contains correctness testing only
	//fprintf(out, "Contains %d %s\n", pck->key, ret ? "Success":"Fail");
	return ret;
}

void serial_del(void** args){
	SerialHashTable_t* htable = (SerialHashTable_t*)args[0];
	unsigned long long* stsumsum = (unsigned long long*)args[1];
	for (int i=0; i<htable->size; i++){
		if (htable->table[i] != NULL){
			Item_t* curr = htable->table[i]->head;
			while (curr != NULL){
				Item_t* temp = curr;
				*(stsumsum) += getFingerprint(curr->value->iterations, curr->value->seed);
				free((void*)(curr->value));
				curr = curr->next;
				free(temp);
			}
			free(htable->table[i]);
		}
	}
	free(htable->table);
	free(htable);
}

StripedLockHTable_t* striped_init(int logSize, int maxBucketSize, bool optimistic){
	StripedLockHTable_t* htable = malloc(sizeof(StripedLockHTable_t));
	htable->optimistic = optimistic;
	htable->tableSize = malloc(sizeof(int));
	htable->tableMask = malloc(sizeof(int));
	*(htable->tableSize) = 1<<logSize;
	htable->locksSize = 1<<logSize;
	*(htable->tableMask) = (1<<logSize)-1;
	htable->locksMask = (htable->locksSize)-1;
	htable->maxBucketSize = maxBucketSize;
	htable->table = malloc(sizeof(SerialList_t*)*(1<<logSize));
	htable->locks = malloc(sizeof(pthread_rwlock_t)*(1<<logSize));
	for (int i=0; i<(1<<logSize); i++){
		htable->table[i] = NULL;
		int j = 0;
		if ((j=pthread_rwlock_init(&(htable->locks[i]), NULL)) != 0){
			fprintf(stderr, "rwlock %d initialization error %d\n",i,j);
			exit(-1);
		}
	}
	return htable;
}

void striped_add(void** args){
	StripedLockHTable_t* htable = (StripedLockHTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	unsigned long long* addsumsum = (unsigned long long*)args[2];
	unsigned long long* remsumsum = (unsigned long long*)args[4];
	//FILE *out = (FILE*)args[3];
	__sync_fetch_and_add(addsumsum, getFingerprint(pck->body->iterations, pck->body->seed));
	int key = pck->key;
	if (htable->table[key & *(htable->tableMask)]!=NULL
			&& htable->table[key & *(htable->tableMask)]->size>=htable->maxBucketSize){
		striped_resize(htable, *(htable->tableSize));
	}
	int i = key & htable->locksMask;
	int j = 0;
	if ((j=pthread_rwlock_wrlock(&(htable->locks[i]))) != 0){
		fprintf(stderr, "add: rwlock %d writelock acquisition error %d\n",i,j);
		exit(-1);
	}
	//crit section!
	int index = key & *(htable->tableMask);
	if(htable->table[index] == NULL)
		htable->table[index] = createSerialListWithItem(key,pck->body);
	else
		addNoCheck_list(htable->table[index],key,pck->body,remsumsum);
	//for contains correctness testing only
	//fprintf(out, "Add %d\n", key);
    //end crit section!
    if ((j=pthread_rwlock_unlock(&(htable->locks[i]))) != 0){
		fprintf(stderr, "add: rwlock %d write unlock error %d\n",i,j);
		exit(-1);
	}
}

bool striped_rem(void** args){
	StripedLockHTable_t* htable = (StripedLockHTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	unsigned long long* remsumsum = (unsigned long long*)args[2];
	//FILE *out = (FILE*)args[3];
	int key = pck->key;
	int i = key & htable->locksMask;
	int j = 0;
	if ((j=pthread_rwlock_wrlock(&(htable->locks[i]))) != 0){
		fprintf(stderr, "remove: rwlock %d writelock acquisition error %d\n",i,j);
		exit(-1);
	}
	//crit section!
	bool result = false;
	int index = key & *(htable->tableMask);
	if(htable->table[index] != NULL)
		result = remove_list(htable->table[index],key,remsumsum);
	//for contains correctness testing only
	//fprintf(out, "Remove %d %s\n", pck->key, result ? "Success":"Fail");
	//end crit section!
	if ((j=pthread_rwlock_unlock(&(htable->locks[i]))) != 0){
		fprintf(stderr, "remove: rwlock %d write unlock error %d\n",i,j);
		exit(-1);
	}
	return result;
}

bool striped_con(void** args){
	StripedLockHTable_t* htable = (StripedLockHTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	//FILE *out = (FILE*)args[3];
	int key = pck->key;
	int i = key & htable->locksMask;
	int j = 0;
	if ((j=pthread_rwlock_rdlock(&(htable->locks[i]))) != 0){
		fprintf(stderr, "contains: rwlock %d readlock acquisition error %d\n",i,j);
		exit(-1);
	}
	//crit section!
	bool result = false;
	int index = key & *(htable->tableMask);
	if (htable->table[index] != NULL)
		result = contains_list(htable->table[index],key);
	//for contains correctness testing only
	//fprintf(out, "Contains %d %s\n", pck->key, result ? "Success":"Fail");
	//end crit section!
	if ((j=pthread_rwlock_unlock(&(htable->locks[i]))) != 0){
		fprintf(stderr, "contains: rwlock %d write unlock error %d\n",i,j);
		exit(-1);
	}
	return result;
}

bool striped_optcon(void** args){
	StripedLockHTable_t* htable = (StripedLockHTable_t*)args[0];
	HashPacket_t* pck = (HashPacket_t*)args[1];
	int key = pck->key;
	//no lock
	//crit section!
	bool result = false;
	int index = key & *(htable->tableMask);
	if (htable->table[index] != NULL)
		result = contains_list(htable->table[index],key);
	//end crit section!
	if (!result){
		result = striped_con(args);
	}
	return result;
}

int striped_resize(StripedLockHTable_t* htable, int init_size){
	int j = 0;
	//printf("%d\n", init_size);
	for (int i=0; i<htable->locksSize; i++){
		if ((j=pthread_rwlock_wrlock(&(htable->locks[i]))) != 0){
			fprintf(stderr, "resize: rwlock %d writelock acquisition error %d\n",i,j);
			exit(-1);
		}
		if (*(htable->tableSize) > init_size){
			for (int k=0; k<=i; k++){
				if ((j=pthread_rwlock_unlock(&(htable->locks[i]))) != 0){
					fprintf(stderr, "rwlock %d write unlock error %d\n",i,j);
					exit(-1);
				}
			}
			return 0;
		}
	}
	//critical section!
	int newTableSize = *(htable->tableSize) * 2;
	int newMask = newTableSize - 1;
	SerialList_t** newTable = malloc(sizeof(SerialList_t*)*newTableSize);
	for (int i=0; i<newTableSize; i++){
		newTable[i] = NULL;
	}
	for (int i=0; i<*(htable->tableSize);i++){
		if (htable->table[i] != NULL){
			Item_t* curr = htable->table[i]->head;
			while (curr != NULL){
				Item_t* nextnode = curr->next;
				if (!htable->optimistic){
					if (newTable[curr->key & newMask] == NULL){
						newTable[curr->key & newMask] = createSerialList();
						curr->next = NULL;
						newTable[curr->key & newMask]->head = curr;
					}
					else{
						curr->next = newTable[curr->key & newMask]->head;
						newTable[curr->key & newMask]->head = curr;
					}
				}
				else{
					Item_t* newItem = malloc(sizeof(Item_t));
					if (newTable[curr->key & newMask] == NULL){
						newTable[curr->key & newMask] = createSerialList();
						newItem->next = NULL;
						newItem->key = curr->key;
						newItem->value = curr->value;
						newTable[curr->key & newMask]->head = newItem;
					}
					else{
						newItem->value = curr->value;
						newItem->next = newTable[curr->key & newMask]->head;
						newItem->key = curr->key;
						newTable[curr->key & newMask]->head = newItem;
					}
				}
          		newTable[curr->key & newMask]->size++;
				curr = nextnode;
			}
			if (!htable->optimistic)
				free(htable->table[i]);
		}
	}
	if (!htable->optimistic){
		SerialList_t** temp = htable->table;
		*(htable->tableSize) = newTableSize;
		*(htable->tableMask) = newMask;
		htable->table = newTable;
		free(temp);
	}
	else{
		StripedLockHTable_t temp = *(htable);
		StripedLockHTable_t newH;
		newH.tableSize = malloc(sizeof(int));
		newH.tableMask = malloc(sizeof(int));
		*(newH.tableSize) = newTableSize;
		newH.locksSize = temp.locksSize;
		*(newH.tableMask) = newMask;
		newH.table = newTable;
		newH.locks = temp.locks;
		newH.optimistic = true;
		*(htable) = newH;
		for (int i=0; i<*(temp.tableSize); i++){
			if (temp.table[i] != NULL){
				Item_t* curr = temp.table[i]->head;
				while (curr != NULL){
					Item_t* tempItem = curr;
					curr = curr->next;
					free(tempItem);
				}
				free(temp.table[i]);
			}
		}
		free(temp.table);
		free((void*)(temp.tableSize));
		free((void*)(temp.tableMask));
	}
	//end critical section!
	for (int i=0; i<htable->locksSize; i++){
		if ((j=pthread_rwlock_unlock(&(htable->locks[i]))) != 0){
			fprintf(stderr, "rwlock %d write unlock error %d\n",i,j);
			exit(-1);
		}
	}
	return 0;
}

void striped_del(void** args){
	StripedLockHTable_t* htable = (StripedLockHTable_t*)args[0];
	unsigned long long* stsumsum = (unsigned long long*)args[1];
	for (int i=0; i<*(htable->tableSize); i++){
		if (htable->table[i] != NULL){
			Item_t* curr = htable->table[i]->head;
			while (curr != NULL){
				Item_t* temp = curr;
				*(stsumsum) += getFingerprint(curr->value->iterations,curr->value->seed);
				free((void*)(curr->value));
				curr = curr->next;
				free(temp);
			}
			free(htable->table[i]);
		}
	}
	int j = 0;
	for (int i=0; i<htable->locksSize; i++){
		if ((j=pthread_rwlock_destroy(&(htable->locks[i]))) != 0){
			fprintf(stderr, "rwlock %d destroy error %d\n",i,j);
			exit(-1);
		}
	}
	free(htable->locks);
	free(htable->table);
	free((void*)(htable->tableSize));
	free((void*)(htable->tableMask));
	free(htable);
}

void hash_process(HBox hbox, HashPacket_t* pck, unsigned long long* opsumsum, FILE* out){
	void *args[5];
	args[0] = hbox.htable;
	args[1] = pck;
	args[3] = out;
	switch(pck->type){
		case Add:
			args[2] = &opsumsum[0];
			args[4] = &opsumsum[1];
			(*(hbox.add_op))(args);
			break;
		case Remove:
			args[2] = &opsumsum[1];
			(*(hbox.rem_op))(args);
			break;
		case Contains:
			__sync_fetch_and_add(&opsumsum[3],(*(hbox.con_op))(args));
			break;
	}
}
