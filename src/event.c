#include <stdio.h>

#include <sys/time.h>

#include "memory.h"
#include "event.h"
#include "connection.h"
#include "http_time.h"

LIST_NEW(accept_events);
LIST_NEW(posted_events);
LIST_NEW(timeout_list);

extern event_t *event_malloc(void)
{
    event_t *e = yhttp_malloc(sizeof(event_t));
    if (!e)
        return NULL;
    e->timeout = 0;
    e->timeout_set = 0;
    return e;
}

extern void event_free(event_t *ev)
{
    yhttp_free(ev);
}

static void event_process_accept(list_t *queue)
{
    event_t *ev;
    list_t *l;
    list_foreach(queue, l) {
        ev = list_entry(l, event_t, posted);
        ev->handle(ev);
    }
    LIST_INIT(queue);
}
extern int event_process_posted(list_t *queue)
{
    BUG_ON(NULL == queue);

    event_t *ev;
    list_t *l, *n;
    /* @ev may be freed via event_del(ev, XXXX) */
    list_foreach_safe(queue, l, n) {
        ev = list_entry(l, event_t, posted);
        ev->handle(ev);
    }
    LIST_INIT(queue);

    return 0;
}

extern void process_all_events()
{
    msec_t timer = event_min_timer();

    event_process(timer);
    /* TODO Finish process_all_events(), consider the timer and return value */

    http_update_time();

    event_process_expire();

    event_process_accept(&accept_events);
    event_process_posted(&posted_events);
    /* FIXME remove it */ //sleep(1);
}

extern void event_process_expire()
{
    list_t *pos, *next;
    list_foreach_safe(&timeout_list, pos, next) {
        event_t *ev = list_entry(pos, event_t, timer);
        connection_t *c = ev->data;
        if (c->tmstamp > current_msec)
            return;
        list_del(pos);
        ev->timeout = 1;
        ev->handle(ev);
    }
}

extern msec_t event_min_timer()
{
    if (list_empty(&timeout_list))
        return EVENT_TIMER_INF;
    list_t *head = list_head(&timeout_list);
    event_t *ev = list_entry(head, event_t, timer);
    connection_t *c = ev->data;
    int tms = c->tmstamp - current_msec;
    return (tms < 0)? 0: tms;
}

extern void event_add_timer(event_t *ev)
{
    ev->timeout_set = 1;
    list_add_tail(&timeout_list, &ev->timer);
}

extern void event_del_timer(event_t *ev)
{
    ev->timeout_set = 0;
    list_del(&ev->timer);
}

