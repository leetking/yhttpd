#ifndef BUFFER_H__
#define BUFFER_H__

#include <stddef.h> /* for size_t */

typedef struct buffer_t {
    void *idx;              /* point to position where next write */
    void *end;
    char buffer[1];
} buffer_t;

buffer_t *buffer_alloc(size_t size);

#endif
