// Implementation of a circular queue.
// (c) 2011,2015 duane a. bailey
/*
 * Implementation approach.
 * Our main datatype is a handle (a pointer to a pointer) that references
 * the tail of a circular, singly-linked list. The list carries generic
 * pointers.
 */
#include <stdlib.h>
#include "cirq.h"

cirq cq_alloc(void)
// post: return a circ structure; 0 if not able
{
  // allocate handle
  return (cirq)calloc(1,sizeof(element*));

}

int cq_size(cirq q)
// pre: q is non-zero
// post: compute the size of the queue.
{
  element *p,*tail;
  int size = 0;
  // here is an example of code that might be simplified using dummy nodes:
  if ((tail = p = *q)) {;
    do {
      size++;
      p = p->next;
    } while (p != tail);
  }
  return size;
}

void cq_enq(cirq q,void *value)
// pre: q is non-zero
// post: value is enqueued at the end of the list
{
  // allocate a new element to hold the value
  element *e = (element*)malloc(sizeof(element));
  element *tail;
  e->value = value;
  if ((tail = *q)) { // queue already has an element
    e->next = tail->next; // tail->next == the head
    tail->next = e;
  } else { // queue is empty
    e->next = e;
  }
  *q = e;
}

void *cq_deq(cirq q)
// pre: q is non-zero
// post: returns value at head of the queue
{
  element *tail;
  void *result = 0;
  if ((tail = *q)) {
    element *head = tail->next;
    result = head->value;
    if (head != tail) { // if multiple nodes
      tail->next = head->next;
    } else {
      *q = 0;
    }
    free(head);
  }
  return result;
}

void *cq_peek(cirq q)
// pre: q is non-zero
// post: returns value at head of queue
{
  element *tail;
  void *result = 0;
  if ((tail = *q)) {
    result = tail->next->value;
  }
  return result;
}

void cq_rot(cirq q)
// pre: q is not 0
// post: one value is dequeued and re-queued at tail
{
  element *tail;
  if ((tail = *q)) {
    *q = tail->next;
  }
}

void cq_free(cirq q)
// pre: q is not 0
// post: memory associated with the circular queue is released
{
  while (cq_deq(q));
  if (q) free(q);
}
