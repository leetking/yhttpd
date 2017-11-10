#include "memory.h"

extern void *yhttp_malloc(size_t size)
{
    return malloc(size);
}

extern void yhttp_free(void *x)
{
    free(x);
}

