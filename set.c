#include "set.h"
#include <stdint.h>

#define HASH_MAX   (9971)

struct node {
    void *ele;
    struct node *next;
};

struct set_t {
    int elesize;
    free_t *free;
    cmp_t  *cmp;
    int size;

    int _iter_hash_idx;
    struct node *_iter_node;
    struct node *_hash[1];
};

static int scmp(void const *x, void const *y)
{
    return (intptr_t)x-(intptr_t)y;
}

extern set_t set_create(size_t elesize, cmp_t *cmp, free_t *free)
{
    set_t s = calloc(1, sizeof(struct set_t) + HASH_MAX*sizeof(struct node));
    if (!s) return NULL;
    s->elesize = elesize;
    s->free = free;
    s->cmp = cmp? cmp: scmp;

    return s;
}
extern void set_destory(set_t *s)
{
    if (!s || !*s) return;

    /* remove element */
    void *ele;
    set_foreach(*s, ele) {
        set_remove(*s, ele);
    }

    /* free structure */
    free(*s);
    *s = NULL;
}
extern int set_isempty(set_t s)
{
    return (0 == s->size);
}
extern int set_isfull(set_t s)
{
    (void)s;
    return 0;
}
extern int set_size(set_t s)
{
    return s->size;
}
extern int set_add(set_t s, void *ele)
{
    if (!s) return -1;
    unsigned int hash = (unsigned int)ele%HASH_MAX;
    struct node **pnodep = &s->_hash[hash];
    while (*pnodep && (*pnodep)->ele) {
        if (0 == s->cmp(ele, (*pnodep)->ele))
            return 0;
        pnodep = &(*pnodep)->next;
    }
    if (!*pnodep) {
        *pnodep = malloc(sizeof(struct node));
        (*pnodep)->next = NULL;
    }
    (*pnodep)->ele = ele;
    s->size++;
    return 0;
}

extern int set_remove(set_t s, void const *hint)
{
    if (!s) return -1;
    unsigned int hash = (unsigned int)hint%HASH_MAX;
    struct node **pnodep = &s->_hash[hash];
    while (*pnodep) {
        if ((*pnodep)->ele && !s->cmp(hint, (*pnodep)->ele)) {
            s->free? s->free(&(*pnodep)->ele): 0;
            (*pnodep)->ele = NULL;
            s->size--;

            return 0;
        }
        pnodep = &(*pnodep)->next;
    }

    /* not found */
    return 1;
}

extern void set_gc(T s)
{
    (void)s;
}

/* private function */
int set_start(set_t s)
{
    if (!s) return -1;
    s->_iter_hash_idx = 0;
    s->_iter_node = s->_hash[0];

    return 0;
}
/**
 * return: 0: the end iterating
 *        !0: iterate
 */
int set_iterate(set_t s, void **ele)
{
    if (!s) return 0;
    for (;;) {
        for (; s->_iter_node; s->_iter_node = s->_iter_node->next) {
            if (s->_iter_node->ele) {
                *ele = s->_iter_node->ele;
                s->_iter_node = s->_iter_node->next;
                return 1;
            }
        }
        s->_iter_hash_idx++;
        if (s->_iter_hash_idx >= HASH_MAX)
            return 0;
        s->_iter_node = s->_hash[s->_iter_hash_idx];
    }

    return 0;
}

#undef STACK_MAX
