/*
 * Here's a vector implementation
 */

#include <stdio.h>
#include <stdlib.h>

#include "vec.h"


/*
 * adds an element to the vector
 */
void v_add(vec v, void *new) {
  if (v->size == v->limit) {
    v->limit *= 2;
    v->data = realloc(v->data, sizeof(void *) * v->limit);
  }
  v->data[v->size] = new;
  v->size++;
}

/*
 * retrieves an item from a vector
 */
void *v_get(vec v, int i) {
  if (i >= v->limit) 
    return;

   return v->data[i];
}

/*
 * sets the element at index i of vector v
 */
void v_set(vec v, int i, void *new) {
  if (i >= v->limit)
    return;

  v->data[i] = new;
}

/*
 * initializes a new vector and returns a pointer to it
 */
vec v_alloc(void) {
  vec v = (vec)calloc(sizeof(vector *), 1);
  v->size = 0;
  v->limit = 10;
  v->data = calloc(sizeof(void *) * v->limit, 1);
  return v;
}

/*
 * frees an existing vector
 */
void v_free(vec v) {
  if (v == 0) 
    return;

  free(v->data);
  free(v);
}

int main() {
  return 0;
}
