#include "config.h"

#ifdef EVENT_USE_SELECT

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/select.h>
#include <fcntl.h>
#include <semaphore.h>

#include "http_time.h"
#include "common.h"
#include "event.h"
#include "log.h"
#include "connection.h"
#include "setting.h"

#define ENVS_ACPT_MAX   (4)
#define ENVS_MAX        (512)

#define INVALID_INDEX   (-1)

static struct {
    event_t *events_acpt[ENVS_ACPT_MAX];
    event_t *events[ENVS_MAX];
    int event_acpt_n, event_n;
    fd_set rdset,  wrset;
    fd_set rdset2, wrset2;
    sem_t *lock;
    int maxfd;
} ENV;

extern int event_init()
{
    FD_ZERO(&ENV.rdset);
    FD_ZERO(&ENV.wrset);
    ENV.event_acpt_n = 0;
    ENV.event_n = 0;
    ENV.maxfd = -1;
    ENV.lock = sem_open(ACCEPT_LOCK, O_EXCL);
    if (ENV.lock == SEM_FAILED) {
        yhttp_error("init semaphore error: %s\n", strerror(errno));
        return -1;
    }
    http_update_time();

    return 0;
}

int event_add(event_t *ev, int event)
{
    BUG_ON(NULL == ev);
    BUG_ON(NULL == ev->data);
    BUG_ON(event != EVENT_READ && event != EVENT_WRITE);

    connection_t *c = ev->data;
    fd_set *fdset;
    event_t **events = ENV.events;
    int *event_n = &ENV.event_n;
    if (ev->accept) {
        events = ENV.events_acpt;
        event_n = &ENV.event_acpt_n;
    }
    switch (event) {
    case EVENT_READ:
        fdset = &ENV.rdset;
        ev->read = 1;
        break;
    case EVENT_WRITE:
        fdset = &ENV.wrset;
        ev->write = 1;
        break;
    default:
        return -1;
    }
    if (c->fd > ENV.maxfd)
        ENV.maxfd = c->fd;
    events[*event_n] = ev;
    ev->index = *event_n;
    (*event_n)++;
    FD_SET(c->fd, fdset);

    return 0;
}

extern int event_del(event_t *ev, int event)
{
    BUG_ON(NULL == ev);
    BUG_ON(NULL == ev->data);
    BUG_ON(event != EVENT_READ && event != EVENT_WRITE);

    connection_t *c = ev->data;
    event_t **events = ENV.events;
    int *event_n = &ENV.event_n;
    if (ev->accept) {
        events = ENV.events_acpt;
        event_n = &ENV.event_acpt_n;
    }
    if (INVALID_INDEX == ev->index)
        return 0;
    switch (event) {
    case EVENT_READ:
        FD_CLR(c->fd, &ENV.rdset);
        ev->read = 0;
        break;
    case EVENT_WRITE:
        FD_CLR(c->fd, &ENV.wrset);
        ev->write = 0;
        break;
    default:
        return -1;
    }
    if (c->fd == ENV.maxfd)
        ENV.maxfd = -1;
    if (ev->index < --(*event_n)) {
        event_t *e = events[*event_n];
        events[ev->index] = e;
        e->index = ev->index;
    }
    ev->index = INVALID_INDEX;
    return 0;
}

/**
 * @ms: timeout of event_process()
 * Return:
 */
extern int event_process(msec_t ms)
{
    int locked = 0;
    int fresh_maxfd = (-1 == ENV.maxfd);
    int err;
    int readyn;
    int st;
    struct timeval tm;
    struct timeval *ptm;

    tm.tv_sec  = ms/1000;
    tm.tv_usec = ms%1000*1000;
    ptm = (ms == EVENT_TIMER_INF)? NULL: &tm;
    ENV.rdset2 = ENV.rdset;
    ENV.wrset2 = ENV.wrset;

    if (fresh_maxfd) {
        for (int i = 0; i < ENV.event_n; i++) {
            connection_t *c = ENV.events[i]->data;
            if (c->fd > ENV.maxfd)
                ENV.maxfd = c->fd;
        }
    }

    st = sem_trywait(ENV.lock);
    if (0 == st) {
        locked = 1;
    } else if (-1 == st) {
        yhttp_debug2("sem_trywait can lock: %s\n", strerror(errno));
    }

    if (locked && fresh_maxfd) {
        for (int i = 0; i < ENV.event_acpt_n; i++) {
            connection_t *c = ENV.events_acpt[i]->data;
            if (c->fd > ENV.maxfd)
                ENV.maxfd = c->fd;
        }
    }
    /* remove server fd from rdset2 and wrset2 */
    if (!locked) {
        for (int i = 0; i < ENV.event_acpt_n; i++) {
            connection_t *c = ENV.events_acpt[i]->data;
            FD_CLR(c->fd, &ENV.rdset2);
        }
        /* no client and server fd need to listen */
        if (0 == ENV.event_n) {
            tm.tv_sec  = SETTING.vars.event_interval/1000;
            tm.tv_usec = SETTING.vars.event_interval%1000*1000;
            ptm = &tm;
        }
    }

    readyn = select(ENV.maxfd+1, &ENV.rdset2, &ENV.wrset2, NULL, ptm);
    err = (readyn == -1);

    /* error:   readyn == -1 && errno != EINTR, readyn == 0 && ptm == NULL */
    /* inter:   readyn == -1 && errno == ETNTR */
    /* timeout: readyn == 0 */
    /* sucess */

    if (err || readyn == 0) {
        if (locked)
            sem_post(ENV.lock);
        /* if (err && errno != EINTR) */
        if (err && EINTR != errno)
            yhttp_warn("select error: %s\n", strerror(errno));
        return err;
    }

    if (locked) {
        for (int i = 0; i < ENV.event_acpt_n && readyn; i++) {
            event_t *ev = ENV.events_acpt[i];
            connection_t *c = ev->data;
            /* accpet new request now */
            if (ev->read && FD_ISSET(c->fd, &ENV.rdset2)) {
                readyn--;
                event_post_event(&accept_events, ev);
            }
        }
        sem_post(ENV.lock);
    }

    /* post event to ``posted_events'' */
    for (int i = 0; i < ENV.event_n && readyn; i++) {
        event_t *e = ENV.events[i];
        connection_t *c = e->data;
        if ((e->read && FD_ISSET(c->fd, &ENV.rdset2))
                || (e->write && FD_ISSET(c->fd, &ENV.wrset2))) {
            readyn--;
            event_post_event(&posted_events, e);
        }
    }

    return 0;
}

extern void event_quit()
{
    if (-1 == sem_close(ENV.lock)) {
        yhttp_warn("close semaphore error: %s\n", strerror(errno));
    }
}

#endif /* EVENT_USE_SELECT */
