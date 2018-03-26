#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "buffer.h"
#include "common.h"

extern buffer_t *buffer_malloc(size_t size)
{
    buffer_t *ret = yhttp_malloc(sizeof(*ret) + size);
    if (!ret)
        NULL;
    buffer_init(ret);
    ret->end = ret->base+size;
    return ret;
}

extern void buffer_free(buffer_t *b)
{
    yhttp_free(b);
}

extern buffer_t *buffer_copy(buffer_t *to, buffer_t const *from)
{
    BUG_ON(NULL == from);
    BUG_ON(NULL == to);
    BUG_ON(from->last < from->pos);
    BUG_ON((to->end - to->last) < (from->last - from->pos));

    memcpy(to->last, from->pos, from->last - from->pos);
    to->last += (from->last - from->pos);

    return to;
}
