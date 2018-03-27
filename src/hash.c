#include "hash.h"
#include "common.h"
#include "memory.h"

struct node {
    uint32_t value;
    void *x;
    struct node *next;
};

struct hash_t {
    int cap;
    hash_func_t *hash;
    hash_free_t *free;
    struct node **table;
};

static uint32_t hash_default_func(void const *key, size_t len)
{
    BUG_ON(NULL == key);

    uint32_t ret = 0;
    char const *s;
    char const *e;
    for (s = key, e = s+len; s < e; s++)
        ret = 31*ret+*s;
    return ret;
}

static int hash_find_prime(int cap)
{
    return cap;
}

static void *hash_new_node(hash_t *t)
{
}

static void hash_free_node(hash_t *t, struct node *node)
{
}

extern hash_t *hash_create(int cap, hash_func_t *hash, hash_free_t *free)
{
    int tbsize = hash_find_prime((int)(cap*1.618));

    hash_t *ret = yhttp_malloc(sizeof(*ret) + tbsize*sizeof(void*));
    if (!ret)
        return NULL;
    ret->cap = cap;
    ret->free = free;
    ret->hash = hash? hash: hash_default_func;

    return ret;
}

extern void hash_destroy(hash_t **t)
{
}

extern uint32_t hash_add(hash_t *t, char const *key, size_t len, void *x)
{
}

extern void *hash_getk(hash_t *t, char const *key, size_t len)
{
    BUG_ON(NULL == t);
    uint32_t hv = t->hash(key, len);
    return hash_getv(t, hv);
}

extern void *hash_getv(hash_t *t, uint32_t vlu)
{
}

extern void *hash_delk(hash_t *t, char const *key, size_t len)
{
}

extern void *hash_delv(hash_t *t, uint32_t vlu)
{
}

