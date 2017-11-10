#include <stdlib.h>

#include "memory.h"
#include "buffer.h"

buffer_t *buffer_malloc(size_t size)
{
    buffer_t *ret = yhttp_malloc(sizeof(*ret) + size);
    if (!ret)
        NULL;
    ret->idx = ret->buffer;
    ret->end = ret->buffer+size;
    return ret;
}
