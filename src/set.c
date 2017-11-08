#include "set.h"
#include <stdint.h>

#define HASH_MAX   (9973)
/* #define HASH_MAX   (7) */

struct node {
    void *obj;
    unsigned char valid;
    unsigned char _iter_id;
    struct node *next;
};

struct set_t {
    free_t   *free;
    cmp_t    *cmp;
    hashfn_t *hashfn;
    int size;

    unsigned char _iter_id;
    int _iter_hash_idx;
    struct node *_iter_node;
    struct node *_hash[1];
};

static int scmp(void const *x, void const *y)
{
    return (intptr_t)x-(intptr_t)y;
}
static uint32_t shash(void const *obj)
{
    return (uintptr_t)obj & 0xffffffff;
}

extern set_t set_create(cmp_t *cmp, free_t *free, hashfn_t *hashfn)
{
    set_t s = calloc(1, sizeof(struct set_t) + HASH_MAX*sizeof(struct node*));
    if (!s) return NULL;
    s->free = free;
    s->cmp = cmp? cmp: scmp;
    s->hashfn = hashfn? hashfn: shash;
    s->_iter_id = 0;

    return s;
}
extern void set_destory(set_t *s)
{
    if (!s || !*s) return;

    /* remove element and hash table */
    for (int i = 0; i < HASH_MAX; i++) {
        struct node *pnode = (*s)->_hash[i];
        struct node *pnext;
        while (pnode) {
            pnext = pnode->next;
            (pnode->valid && (*s)->free)? (*s)->free(&pnode->obj): 0;
            free(pnode);
            pnode = pnext;
        }
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
extern int set_add(set_t s, void *obj)
{
    if (!s) return -1;
    uint32_t hash = s->hashfn(obj)%HASH_MAX;
    struct node **pnodep = &s->_hash[hash];
    for (; *pnodep && (*pnodep)->valid; pnodep = &(*pnodep)->next) {
        if (0 == s->cmp(obj, (*pnodep)->obj))
            return 1;
    }
    if (!*pnodep) {
        *pnodep = malloc(sizeof(struct node));
        (*pnodep)->next = NULL;
    }
    (*pnodep)->obj = obj;
    (*pnodep)->valid = 1;
    (*pnodep)->_iter_id = s->_iter_id;
    s->size++;
    return 0;
}

extern int set_remove(set_t s, void *hint)
{
    if (!s) return -1;
    uint32_t hash = s->hashfn(hint)%HASH_MAX;
    struct node **pnodep = &s->_hash[hash];
    for (; *pnodep; pnodep = &(*pnodep)->next) {
        if ((*pnodep)->valid && !s->cmp(hint, (*pnodep)->obj)) {
            s->free? s->free(&(*pnodep)->obj): 0;
            (*pnodep)->valid = 0;
            s->size--;

            return 0;
        }
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
    s->_iter_id++;
    s->_iter_hash_idx = 0;
    s->_iter_node = s->_hash[0];

    return 0;
}
/**
 * return: 0: the end iterating
 *        !0: iterate
 */
int set_iterate(set_t s, void **pobj)
{
    if (!s || !pobj) return 0;
    for (;;) {
        for (; s->_iter_node; s->_iter_node = s->_iter_node->next) {
            if (s->_iter_node->valid && (s->_iter_id != s->_iter_node->_iter_id)) {
                *pobj = s->_iter_node->obj;
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

#undef HASH_MAX
