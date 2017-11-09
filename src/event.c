#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "common.h"
#include "event.h"
#include "set.h"
#include "log.h"

static struct event_t {
    set_t funs;
    fd_set rdset,  wrset;
    fd_set rdset2, wrset2;
    uint8_t quit:1;
} EVENT;

struct callback_fun {
    int fd;
    int event;
    event_fun_t *fun;
    void *data;
};
static int callfun_cmp(struct callback_fun *f1, struct callback_fun *f2)
{
    if (f1->fd == f2->fd)
        return (f1->event - f2->event);
    return (f1->fd - f2->fd);
}
static uint32_t callfun_hash(struct callback_fun *obj)
{
    uint32_t hash = obj->fd;
    hash ^= obj->event;
    return hash;
}
extern int event_init()
{
    FD_ZERO(&EVENT.rdset);
    FD_ZERO(&EVENT.wrset);
    EVENT.funs = set_create((cmp_t*)callfun_cmp, NULL, (hashfn_t*)callfun_hash);
    EVENT.quit = 0;
    if (!EVENT.funs) {
        _M(LOG_ERROR, "Init EVENT.funs error!\n");
        return 1;
    }

    return 0;
}
extern int event_add(int fd, int event, event_fun_t *fun, void *data)
{
    fd_set *fdset;
    switch (event) {
    case EVENT_READ:
        fdset = &EVENT.rdset;
        break;
    case EVENT_WRITE:
        fdset = &EVENT.wrset;
        break;
    default:
        return -1;
    }
    FD_SET(fd, fdset);
    struct callback_fun *callfun = malloc(sizeof(*callfun));
    if (!callfun) {
        _M(LOG_ERROR, "malloc callfun error.\n");
        FD_CLR(fd, fdset);
        return -1;
    }
    callfun->fd    = fd;
    callfun->event = event;
    callfun->fun   = fun;
    callfun->data  = data;

    if (1 == set_add(EVENT.funs, callfun)) {
        _M(LOG_WARN, "%d-%d has been registered.\n", fd, event);
        free(callfun);
        return 1;
    }

    return 0;
}
extern int event_del(int fd, int event)
{
    switch (event) {
    case EVENT_READ:
        FD_CLR(fd, &EVENT.rdset);
        break;
    case EVENT_WRITE:
        FD_CLR(fd, &EVENT.wrset);
        break;
    default:
        return -1;
    }
    struct callback_fun hint;
    hint.fd = fd;
    hint.event = event;

    return set_remove(EVENT.funs, &hint);
}
extern int event_loop()
{
    while (!EVENT.quit) {
        memcpy(&EVENT.rdset2, &EVENT.rdset, sizeof(fd_set));
        memcpy(&EVENT.wrset2, &EVENT.wrset, sizeof(fd_set));
        int maxfd = -1;
        struct callback_fun *fun;
        set_foreach(EVENT.funs, fun) {
            _M(LOG_DEBUG2, "event_loop() fd: %d\n", fun->fd);
            if (fun->fd > maxfd)
                maxfd = fun->fd;
        }
        int ret = select(maxfd+1, &EVENT.rdset2, &EVENT.wrset2, NULL, NULL);
        if (-1 == ret) {
            _M(LOG_DEBUG2, "select: %s\n", strerror(errno));
            event_break();
            return -1;
        }

        set_foreach(EVENT.funs, fun) {
            if (FD_ISSET(fun->fd, &EVENT.rdset2)
                    || FD_ISSET(fun->fd, &EVENT.wrset2)) {
                fun->fun(fun->data);
            }
        }
    }

    return 0;
}

extern void event_quit()
{
    set_destory(&EVENT.funs);
}
extern void event_break()
{
    EVENT.quit = 1;
}
