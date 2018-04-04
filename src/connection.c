#include <errno.h>

#include "common.h"
#include "memory.h"
#include "connection.h"

extern connection_t *connection_malloc()
{
    connection_t *c = yhttp_malloc(sizeof(connection_t));
    if (!c) return NULL;

    return c;
}

extern void connection_free(connection_t *c)
{
    BUG_ON(NULL == c);

    if (c->rd || c->wr)
        event_free(c->ev);
    yhttp_free(c);
}

extern void connection_pause(connection_t *c, int event)
{
    int s = 0;
    BUG_ON(NULL == c);
    switch (event) {
    case EVENT_READ:
        BUG_ON(!c->rd);
        BUG_ON(!c->rd_enable);
        c->rd_enable = 0;
        s = event_del(c->ev, EVENT_READ);
        if (c->ev->timeout_set)
            event_del_timer(c->ev);
        break;
    case EVENT_WRITE:
        BUG_ON(!c->wr);
        BUG_ON(!c->wr_enable);
        c->wr_enable = 0;
        s = event_del(c->ev, EVENT_WRITE);
        break;
    default:
        BUG_ON("unkown event type");
        break;
    }
    BUG_ON(s != 0);
}

extern void connection_revert(connection_t *c, int event)
{
    int s = 0;
    BUG_ON(NULL == c);
    switch (event) {
    case EVENT_READ:
        BUG_ON(!c->rd);
        BUG_ON(c->rd_enable);
        c->rd_enable = 1;
        s = event_add(c->ev, EVENT_READ);
        break;
    case EVENT_WRITE:
        BUG_ON(!c->wr);
        BUG_ON(c->wr_enable);
        c->wr_enable = 1;
        s = event_add(c->ev, EVENT_WRITE);
        break;
    default:
        BUG_ON("unkown event type");
        break;
    }
    BUG_ON(s != 0);
}

extern void connection_event_add(connection_t *c, int event, event_t *ev)
{
    BUG_ON(NULL == c);
    switch (event) {
    case EVENT_READ:
        BUG_ON(c->rd);
        BUG_ON(c->rd_enable);
        c->rd = 1;
        c->rd_enable = 0;
        c->ev = ev;
        break;
    case EVENT_WRITE:
        BUG_ON(c->wr);
        BUG_ON(c->wr_enable);
        c->wr = 1;
        c->wr_enable = 0;
        c->ev = ev;
        break;
    default:
        BUG_ON("unkown event type");
        break;
    }
}

extern void connection_event_add_now(connection_t *c, int event, event_t *ev)
{
    int s = 0;
    BUG_ON(NULL == c);
    switch (event) {
    case EVENT_READ:
        BUG_ON(c->rd);
        BUG_ON(c->rd_enable);
        c->rd = 1;
        c->rd_enable = 1;
        c->ev = ev;
        s = event_add(c->ev, EVENT_READ);
        break;
    case EVENT_WRITE:
        BUG_ON(c->wr);
        BUG_ON(c->wr_enable);
        c->wr = 1;
        c->wr_enable = 1;
        c->ev = ev;
        s = event_add(c->ev, EVENT_WRITE);
        break;
    default:
        BUG_ON("unkown event type");
        break;
    }
    BUG_ON(s != 0);
}

extern event_t *connection_event_del(connection_t *c, int event)
{
    int s = 0;
    BUG_ON(NULL == c);
    switch (event) {
    case EVENT_READ:
        BUG_ON(!c->rd);
        if (c->rd_enable)
            s = event_del(c->ev, EVENT_READ);
        if (c->ev->timeout_set)
            event_del_timer(c->ev);
        c->rd = 0;
        c->rd_enable = 0;
        break;
    case EVENT_WRITE:
        BUG_ON(!c->wr);
        if (c->wr_enable)
            s = event_del(c->ev, EVENT_WRITE);
        c->wr = 0;
        c->wr_enable = 0;
        break;
    default:
        BUG_ON("unkown event type");
        break;
    }
    BUG_ON(s != 0);
    return c->ev;
}

extern void connection_read_timeout(connection_t *c, msec_t ts)
{
    event_t *ev = c->ev;
    BUG_ON(!c->rd);
    BUG_ON(!ev->read);

    c->tmstamp = ts;
    if (ev->timeout_set)
        event_del_timer(ev);
    event_add_timer(ev);
}

extern void connection_destroy(connection_t *c)
{
    int nedd_free = 0;

    BUG_ON(NULL == c);
    BUG_ON(c->rd && c->wr);
    BUG_ON(c->rd_enable && c->wr_enable);

    if (c->rd_enable) {
        if (c->ev->timeout_set)
            event_del_timer(c->ev);
        event_del(c->ev, EVENT_READ);
        nedd_free = 1;
    } else if (c->wr_enable) {
        event_del(c->ev, EVENT_WRITE);
        nedd_free = 1;
    }
    if (nedd_free)
        event_free(c->ev);
    close(c->fd);
    yhttp_free(c);
}

extern ssize_t connection_read(int fd, char *start, char *end)
{
    if (start >= end)
        return YHTTP_BLOCK;
    int rdn = read(fd, start, end-start);
    if (rdn > 0)
        return rdn;
    if (rdn == -1 && (EAGAIN == errno || EWOULDBLOCK == errno))
        return YHTTP_BLOCK;
    if (rdn == -1 && EINTR == errno)
        return YHTTP_AGAIN;
    return YHTTP_ERROR;
}

extern ssize_t connection_write(int fd, char const *start, char const *end)
{
    if (start >= end)
        return YHTTP_BLOCK;
    int wrn = write(fd, start, end-start);
    if (wrn > 0)
        return wrn;
    if (wrn == -1 && (EAGAIN == errno || EWOULDBLOCK == errno))
        return YHTTP_BLOCK;
    if (wrn == -1 && EINTR == errno)
        return YHTTP_AGAIN;
    return YHTTP_ERROR;
}
