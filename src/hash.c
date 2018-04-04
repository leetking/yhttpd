#include "hash.h"
#include "common.h"
#include "memory.h"

#define NODE_SIZE   (sizeof(struct node))

struct node {
    uint32_t value;
    void *x;
    struct node *hash_next;
    struct node *extr_next;
};

struct hash_t {
    int cap, tbsize;
    hash_func_t *hash;
    hash_free_t *free;
    struct node **table;
    struct node *freed;
    struct node *extra;
    char *pool;
    char *pos;
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
    static const int table[] = {
        7, 23, 107, 317, 719, 1777, 5107, 7879, 10007,
    };
    int l = 0, m, r = ARRSIZE(table);
    while (l < r) {
        m = (l+r)/2;
        if (table[m] < cap)
            l = m+1;
        else
            r = m;
    }
    /* table[r] < table[m] <= table[l] */
    return table[l];
}

static void *hash_new_node(hash_t t)
{
    struct node *new = NULL;
    if (t->freed) {
        new = t->freed;
        t->freed = t->freed->hash_next;
    } else if (t->cap) {
        --t->cap;
        new = (struct node*)t->pos;
        t->pos += NODE_SIZE;
    } else {
        new = yhttp_malloc(NODE_SIZE);
        new->extr_next = t->extra;
        t->extra = new;
    }
    return new;
}

static void hash_free_node(hash_t t, struct node *node)
{
    node->hash_next = t->freed;
    t->freed = node;
}

extern hash_t hash_create(int cap, hash_func_t *hash, hash_free_t *free)
{
    /* cap = 0.618*tbsize */
    int tbsize = hash_find_prime((int)(cap*1.618));

    hash_t ret = yhttp_malloc(sizeof(*ret) + tbsize*sizeof(void*) + cap*NODE_SIZE);
    if (!ret)
        return NULL;
    ret->cap = cap;
    ret->tbsize = tbsize;
    ret->free = free;
    ret->hash = hash? hash: hash_default_func;
    ret->table = (struct node**)((char*)ret+sizeof(*ret));
    ret->freed = NULL;
    ret->extra = NULL;
    ret->pool = (char*)ret->table+tbsize*sizeof(void*);
    ret->pos = ret->pool;

    return ret;
}

extern void hash_destroy(hash_t *t)
{
    if (!t || !*t)
        return;
    struct node *n = (*t)->extra;
    while (n) {
        struct node *d = n;
        n = n->extr_next;
        yhttp_free(d);
    }
    yhttp_free(*t);
    *t = NULL;
}

extern uint32_t hash_add(hash_t t, char const *key, size_t len, void *x)
{
    BUG_ON(NULL == t);

    uint32_t vlu = t->hash(key, len);
    int idx = vlu%t->tbsize;
    struct node *new = hash_new_node(t);
    struct node **slot = &t->table[idx];

    new->value = vlu;
    new->x = x;

    new->hash_next = (*slot);
    (*slot) = new;

    return vlu;
}

extern void *hash_getk(hash_t t, char const *key, size_t len)
{
    BUG_ON(NULL == t);
    uint32_t hv = t->hash(key, len);
    return hash_getv(t, hv);
}

extern void *hash_getv(hash_t t, uint32_t vlu)
{
    int idx = vlu%t->tbsize;
    for (struct node *s = t->table[idx]; s; s = s->hash_next) {
        if (s->value == vlu)
            return s->x;
    }
    return NULL;
}

extern void *hash_delk(hash_t t, char const *key, size_t len)
{
    BUG_ON(NULL == t);
    uint32_t vlu = t->hash(key, len);
    return hash_delv(t, vlu);
}

extern void *hash_delv(hash_t t, uint32_t vlu)
{
    BUG_ON(NULL == t);
    void *x;
    struct node *d;
    int idx = vlu%t->tbsize;
    for (struct node **s = &t->table[idx]; *s; s = &(*s)->hash_next) {
        if (vlu == (*s)->value) {
            x = (*s)->x;
            d = (*s);
            (*s) = (*s)->hash_next;
            hash_free_node(t, d);
            return x;
        }
    }
    return NULL;
}

