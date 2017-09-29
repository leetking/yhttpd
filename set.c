#include "set.h"

struct set {
    int *s;
    int cap;
    int _iter_now;
};

extern set_t set_create(size_t elesize, free_t *free)
{
    return NULL;
}
extern void set_destory(set_t *s)
{
    *s = NULL;
}
extern int set_isempty(set_t s)
{
}
extern int set_isfull(set_t s)
{
}
extern int set_add(set_t s, void const *hint)
{
}
extern int set_remove(set_t s, void const *hint)
{
}

/* private function */
int set_start(set_t s)
{
}
int set_iterate(set_t s, void **ele)
{
}
int set_isend(set_t s)
{
}
