#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <errno.h>

#include <unistd.h>

#include "event.h"
#include "common.h"
#include "buffer.h"

typedef struct connection_t {
    int fd;
    void *data;
    event_t *ev;            /* use by read and write event as a union */
    msec_t tmstamp;
    unsigned rd:1;
    unsigned rd_eof:1;
    unsigned rd_enable:1;
    unsigned wr:1;
    unsigned wr_eof:1;
    unsigned wr_enable:1;
} connection_t;

extern connection_t *connection_malloc();
extern void connection_free(connection_t *c);

extern void connection_revert(connection_t *c, int event);
extern void connection_pause(connection_t *c, int event);
/* add but pause */
extern void connection_event_add(connection_t *c, int event, event_t *ev);
/* add and active */
extern void connection_event_add_now(connection_t *c, int event, event_t *ev);
extern event_t *connection_event_del(connection_t *c, int event);
extern void connection_read_timeout(connection_t *c, msec_t ts);
extern void connection_destroy(connection_t *c);

/**
 * Return: YHTTP_ERROR
 *         YHTTP_AGAIN
 *         nbytes
 */
extern ssize_t connection_read(int fd, char *start, char *end);
//extern ssize_t connection_write(int fd, char const *start, char const *end);

static inline ssize_t connection_write(int fd, char const *start, char const *end)
{
    if (unlikely(start >= end))
        return YHTTP_BLOCK;
    int wrn = write(fd, start, end-start);
    if (likely(wrn > 0))
        return wrn;
    if (wrn == -1 && (EAGAIN == errno || EWOULDBLOCK == errno))
        return YHTTP_BLOCK;
    if (wrn == -1 && EINTR == errno)
        return YHTTP_AGAIN;
    return YHTTP_ERROR;
}

#endif /* CONNECTION_H__ */
