#ifndef EVENT_H__
#define EVENT_H__

#include <stdint.h>

#include "common.h"
#include "list.h"

extern list_t accept_events;
extern list_t posted_events;
extern list_t timeout_list;

typedef struct event_t event_t;
typedef void event_cb_t(event_t *ev);

struct event_t {
    void *data;
    event_cb_t *handle;
    list_t posted;
    list_t timer;
    int index;
    uint8_t accept:1;
    uint8_t read:1;
    uint8_t write:1;
    uint8_t timeout_set:1;
    uint8_t timeout:1;
};

enum {
    EVENT_READ  = 0x01,   /* read  */
    EVENT_WRITE = 0x10,   /* write */
};

extern event_t *event_malloc(void);
extern void event_free(event_t *ev);

extern int event_init();
extern void event_quit();
extern int event_add(event_t *ev, int event);
extern int event_del(event_t *ev, int event);
extern int event_process(msec_t ms);

extern int event_process_posted(list_t *queue);
extern void process_all_events();
extern msec_t event_min_timer();
extern void event_add_timer(event_t *ev);
extern void event_del_timer(event_t *ev);
extern void event_process_expire();

#define EVENT_TIMER_INF     ((msec_t)-1)

#define event_post_event(q, ev) \
    list_add(q, &(ev)->posted)

#endif

