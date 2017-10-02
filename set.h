#ifndef SET_H__
#define SET_H__

#include <stdlib.h>

/* 简单的集合类型 */
#define T set_t
typedef struct T *T;
/* free(*x); *x = NULL */
typedef void free_t(void **x);
typedef int  cmp_t(void const *x, void const *y);

/* public function */
/**
 * if cmp == NULL then
 *  cmp via element address
 * if free == NULL then
 *  no free element
 */
extern T set_create(size_t elesize, cmp_t *cmp, free_t *free);
extern void set_destory(T *s);
extern int set_size(T s);
extern int set_isempty(T s);
extern int set_isfull(T s);
extern int set_add(T s, void *ele);
extern int set_remove(T s, void const *hint);

extern void set_gc(T s);

/* a sample interater */
#define set_foreach(s, ele) \
        for (set_start(s); set_iterate(s, (void**)&ele); )

/* private function, though we can invoke them, but may be rmeoved. */
int set_start(T s);
int set_iterate(T s, void **ele);

#endif /* SET_H__ */
