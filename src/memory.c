#include "memory.h"

extern void *yhttp_malloc(size_t size)
{
    return calloc(1, size);
}

extern void yhttp_free(void *x)
{
    free(x);
}

struct yhttp_pool_t {
    size_t nmem, size;
    char *map;
    void *base;
};

extern yhttp_pool_t *yhttp_pool_create(size_t nblk, size_t size)
{
    return NULL;
}

extern void yhttp_pool_destroy(yhttp_pool_t *pool)
{
}

extern void *yhttp_pool_alloc(yhttp_pool_t *pool)
{
    return NULL;
}

extern void yhttp_pool_refund(yhttp_pool_t *pool, void *block)
{
}
