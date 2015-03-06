/*
 * An implementation of a vector.
 * (c) 2015 Tony Liu and Reid Pryzant.
 */
#ifndef VEC_H
#define VEC_H

typedef struct vector vector;

struct vector {
  void **data;
  int  size;
  int  limit;
};

typedef vector *vec;

extern void v_add(vec, void *);
extern void *v_get(vec, int);
extern void v_set(vec, int, void *);
extern vec v_alloc(void);
extern void v_free(vec);

#endif
