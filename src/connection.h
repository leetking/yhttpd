#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <unistd.h>

#include "common.h"
#include "buffer.h"

typedef struct connection_t {
    int fd;
    void *data;
    ssize_t (*read)(int fd, buffer_t *buffer);
    ssize_t (*write)(int fd, buffer_t *buffer);
    msec_t tmstamp;
} connection_t;

extern connection_t *connection_malloc();
extern void connection_free(connection_t *c);

#endif /* CONNECTION_H__ */
