#include "set.h"

#define NULL ((void*)0)

struct set {
    int *s;
    int cap;
    int _iter_now;
};

extern set_t *set_create(int cap)
{
    return NULL;
}
extern void set_destory(set_t *s)
{
}
extern int set_isempty(set_t const *s)
{
}
extern int set_isfull(set_t const *s)
{
}
extern int set_add(set_t *s, int ele)
{
}
extern int set_remove(set_t *s, int ele)
{
}

/* private function */
int set_start(set_t *s)
{
}
int set_iterate(set_t *s, int *ele)
{
}
int set_isend(set_t *s)
{
}
