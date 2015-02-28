// An implementation of a simple hash table.
// (c) 2011,2015 duane a. bailey

#define _BSD_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

// holder for key-value pair, key is always a string, value is non-null
typedef struct pair pair;

struct pair {
  char *key;
  void *value;
};

static int hashString(char *s)
// pre: s is non-null
// post: hash associated with s
{
  int result = 0;
  while (*s) { result *= 31; result ^= *s++; }
  if (result < 0) result = -result;
  return result;
}

hash ht_alloc(int buckets)
// pre: buckets >= 1
// post: empty hash table is allocated
{
  hash result = (hash)malloc(sizeof(struct hashTable));
  result->cells = buckets;
  result->entries = 0;
  // allocate the buckets: cirqs must be allocated before first use
  result->bucket = (cirq*)calloc(buckets,buckets*sizeof(cirq));

  return result;
}

void ht_put(hash h, char *key, void *value)
// pre: h is not null, key is not null
// post: adds (or updates) key-value pair in the table
{
  int i = hashString(key)%h->cells;
  cirq q = h->bucket[i];
  pair *p;
  if (!q) {
    q = h->bucket[i] = cq_alloc();
  } else {
    int qs = cq_size(q);
    int j;
    for (j = 0; j < qs; j++) {
      pair *p = cq_peek(q);
      if (0 == strcmp(p->key,key)) {
	p->value = value;
	return;
      }
      cq_rot(q);
    }
  }
  p = (pair*)malloc(sizeof(pair));
  p->key = strdup(key); // we strdup to ensure it won’t be shared or modified.
  p->value = value;
  h->entries++;
  cq_enq(q,p);
}

void *ht_get(hash h, char *key)
// pre: h is non-null, key is non-null
// post: retrieves the value associated with key, or returns 0
{
  int i;
  cirq q;
  i = hashString(key)%(h->cells);
  q = h->bucket[i];
  if (q) {
    int qs = cq_size(q);
    int j;
    for (j = 0; j < qs; j++) {
      pair *p = cq_peek(q);
      if (0 == strcmp(p->key,key)) return p->value;
      cq_rot(q);
    }
  }
  return 0;
}

void ht_free(hash h)

// pre: h is not free

// post: frees resources allocated by hash

{
  int i;
  for (i = 0; i < h->cells; i++) {
    if (h->bucket[i]) {
      cirq q = h->bucket[i];
      pair *p;
      while ((p = cq_deq(q))) {
	free(p->key); // because we strdup’d
	free(p);
      }
      // bucket is now empty
      cq_free(h->bucket[i]);
    }
  }
  free(h->bucket);
  free(h);
}
