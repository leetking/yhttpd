#include <stdio.h>

#include <sys/time.h>

#include "memory.h"
#include "event.h"
#include "connection.h"

LIST_NEW(posted_events);
LIST_NEW(timeout_list);
msec_t current_msec;

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

extern int event_process_posted(list_t *queue)
{
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

/* TODO Finish process_all_events(), consider the timer */
extern void process_all_events()
{
    msec_t timer = event_min_timer();

    event_process(timer);

    event_update_time();

    event_process_expire();

    event_process_posted(&posted_events);
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

extern msec_t event_update_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    current_msec = 1000*tv.tv_sec + tv.tv_usec/1000;
    return current_msec;
}


