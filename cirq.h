// Definitions needed for a circular queue, circ.
// (c) 2011,2015 duane a. bailey
#ifndef CIRC_H
#define CIRC_H

typedef struct element element;

struct element {
  void *value;
  struct element *next;
};

typedef element **cirq;
// public interface:
extern cirq cq_alloc(void);
extern int cq_size(cirq q);
extern void cq_enq(cirq q, void *value);
extern void *cq_deq(cirq q);
extern void *cq_peek(cirq q);
extern void cq_rot(cirq q);
extern void cq_free(cirq q);
#endif
