#ifndef SET_H__
#define SET_H__

#include <stdlib.h>

/* 简单的集合类型 */
typedef struct set *set_t;
#define T set_t
/* free(*x); *x = NULL */
typedef void free_t(void **x);

/* public function */
extern T set_create(size_t elesize, free_t *free);
extern void set_destory(T *s);
extern int set_isempty(T s);
extern int set_isfull(T s);
extern int set_add(T s, void const *hint);
extern int set_remove(T s, void const *hint);

/* a sample interater */
#define set_foreach(s, ele) \
        for (set_start(s); set_isend(s); set_iterate(s, (void**)&ele))

/* private function, though we can invoke them, but may be rmeoved. */
int set_start(T s);
int set_iterate(T s, void **ele);
int set_isend(T s);

#endif /* SET_H__ */
