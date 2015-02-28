// The interface for a simple hash table.
// (c) 2011,2015 duane a. bailey

#ifndef HASH_H
#define HASH_H
#include "cirq.h"
// hash structure is non-trivial

struct hashTable {
  int cells; // number of buckets in table
  int entries; // number of (key,value) pairs
  cirq *bucket; // array of buckets
};

typedef struct hashTable *hash;
extern hash ht_alloc(int buckets);
extern void ht_put(hash h, char *key, void *value);
extern void *ht_get(hash h, char *key);
extern void ht_free(hash);
#endif
