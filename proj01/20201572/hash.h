#ifndef __MYLIB_HASH_H
#define __MYLIB_HASH_H

/* Hash table.

  This is a standard hash table with chaining.  To locate an
   element in the table, we compute a hash function over the
   element's data and use that as an index into an array of
   doubly linked lists, then linearly search the list.

   The chain lists do not use dynamic allocation.  Instead, each
   structure that can potentially be in a hash must embed a
   struct hash_elem member.  All of the hash functions operate on
   these `struct hash_elem's.  The hash_entry macro allows
   conversion from a struct hash_elem back to a structure object
   that contains it.  This is the same technique used in the
   linked list implementation.  Refer to ./list.h for a
   detailed explanation. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Hash element. */
struct hash_elem 
  {
    struct list_elem list_elem;
  };

// works just like list_entry to convert hash element to pointer of hash_item
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)		 \
	((STRUCT* ) ((uint8_t *) &(HASH_ELEM)->list_elem \
				- offsetof(STRUCT, MEMBER.list_elem)))

/* Computes and returns the hash value for hash element E, given
   auxiliary data AUX. */
typedef unsigned hash_hash_func (const struct hash_elem *e, void *aux);

/* Compares the value of two hash elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool hash_less_func (const struct hash_elem *a,
                             const struct hash_elem *b,
                             void *aux);

/* Performs some operation on hash element E, given auxiliary
   data AUX. */
typedef void hash_action_func (struct hash_elem *e, void *aux);

/* Hash table. */
struct hash 
  {
    size_t elem_cnt;            /* Number of elements in table. */
    size_t bucket_cnt;          /* Number of buckets, a power of 2. */
    struct list *buckets;       /* Array of `bucket_cnt' lists. */
    hash_hash_func *hash;       /* Hash function. */
    hash_less_func *less;       /* Comparison function. */
    void *aux;                  /* Auxiliary data for `hash' and `less'. */
  };

/* A hash table iterator. */
struct hash_iterator 
  {
    struct hash *hash;          /* The hash table. */
    struct list *bucket;        /* Current bucket. */
    struct hash_elem *elem;     /* Current hash element in current bucket. */
  };

/* hash table entry with key and value. Both should be the same, and while value may change, key must not change
	while the item is still inside the table. If change is needed it, delete the item and re-insert it */
struct hash_item{
	struct hash_elem elem;
	int key;
	int value;
};

/* Basic life cycle. */
bool hash_init (struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear (struct hash *, hash_action_func *);
void hash_destroy (struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert (struct hash *, struct hash_elem *);
struct hash_elem *hash_replace (struct hash *, struct hash_elem *);
struct hash_elem *hash_find (struct hash *, struct hash_elem *);
struct hash_elem *hash_delete (struct hash *, struct hash_elem *);

/* Iteration. */
void hash_apply (struct hash *, hash_action_func *);
void hash_first (struct hash_iterator *, struct hash *);
struct hash_elem *hash_next (struct hash_iterator *);
struct hash_elem *hash_cur (struct hash_iterator *);

/* Information. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* Sample hash functions. */
unsigned hash_bytes (const void *, size_t);
unsigned hash_string (const char *);
unsigned hash_int (int);

/* proj1 implementations */
unsigned hash_int_2 (int key);
unsigned my_hash_func(struct hash_elem* e, void* aux);
bool cmp_hash(struct hash_elem* a, struct hash_elem* b, void* aux);

// custom hash_action_funcs to perform while iterating
void hash_print(struct hash_elem* e, void* aux);
void hash_square(struct hash_elem* e, void* aux);
void hash_triple(struct hash_elem* e, void* aux);
void hash_free(struct hash_elem* e, void* aux);
#endif /* hash.h */
