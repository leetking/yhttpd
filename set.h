#ifndef SET_H__
#define SET_H__

#include <stdint.h>
#include <stdlib.h>

/* 简单的集合类型 */
#define T set_t
typedef struct T *T;
/* free(*x); *x = NULL */
typedef void free_t(void **x);
/**
 * if x < y then return -1
 * if x = y then return 0
 * if x > y then return 1
 */
typedef int  cmp_t(void const *x, void const *y);
/**
 * The same data MUST return euqal hash value
 */
typedef uint32_t hashfn_t(void const *data);

/* public function */
/**
 * if cmp == NULL then
 *  cmp via element address
 * if free == NULL then
 *  no free element
 * if hashfn == NULL then
 *  hash via element address
 */
extern T set_create(cmp_t *cmp, free_t *free, hashfn_t *hashfn);
extern void set_destory(T *s);
extern int set_size(T s);
extern int set_isempty(T s);
extern int set_isfull(T s);
/**
 * add `obj' to the set `s'
 * @return: 0: add it successfull
 *          1: `obj' has existed in set `s'
 *         -1: error
 */
extern int set_add(T s, void *obj);
/**
 * remove element from set `s' via `hint',
 * in function `set_remove`, compare element and hint via function `cmp'
 * @return: 0: remove it successfull
 *          1: can't found element via `hint'
 */
extern int set_remove(T s, void *hint);

extern void set_gc(T s);

/* a sample interater */
#define set_foreach(s, obj) \
        for (set_start(s); set_iterate(s, (void**)&obj); )

/* private function, though we can invoke them, but may be rmeoved. */
int set_start(T s);
int set_iterate(T s, void **pobj);

#endif /* SET_H__ */
