#ifndef MEMOERY_H__
#define MEMOERY_H__

#include <stdlib.h>

extern void *yhttp_malloc(size_t size);
extern void yhttp_free(void *x);


/**
 * @nblk: number of blocks
 * @size: block size
 */
typedef struct yhttp_pool_t yhttp_pool_t;

extern yhttp_pool_t *yhttp_pool_create(size_t nblk, size_t size);
extern void yhttp_pool_destroy(yhttp_pool_t *pool);
extern void *yhttp_pool_alloc(yhttp_pool_t *pool);
extern void yhttp_pool_refund(yhttp_pool_t *pool, void *block);

#endif /* MEMOERY_H__ */
