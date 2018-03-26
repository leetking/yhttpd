#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <unistd.h>

#include "event.h"
#include "common.h"
#include "buffer.h"

typedef struct connection_t {
    int fd;
    void *data;
    ssize_t (*read)(int fd, char *start, char *end);
    ssize_t (*write)(int fd, char const *start, char const *end);
    event_t *rev;
    event_t *wev;
    msec_t tmstamp;
    unsigned rd_eof:1;
    unsigned wr_eof:1;
} connection_t;

extern connection_t *connection_malloc();
extern void connection_free(connection_t *c);

/**
 * Return: YHTTP_ERROR
 *         YHTTP_AGAIN
 *         nbytes
 */
extern ssize_t connection_read(int fd, char *start, char *end);
extern ssize_t connection_write(int fd, char const *start, char const *end);

#endif /* CONNECTION_H__ */
