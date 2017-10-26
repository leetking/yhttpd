#ifndef EVENT_H__
#define EVENT_H__

#include "connection.h"
#include "set.h"

#define CON_MAX     (1024)

enum {
    EVENT_READ = 0,   /* read  */
    EVENT_WRITE,      /* write */
    EVENT_ONCE,       /* only be invoked once */
    EVENT_TIMEOUT,    /* timeout */

    EVENT_ID_MAX,
};

typedef void event_fun_t(void *data);

extern int  event_init();
extern void event_quit();
/**
 * add event, but if fd-event is registered function `fun' alreadly,
 * there is no thing to do
 * @return 0: success
 *         1: fd-event has been registered, there is no thing to do
 *        -1: error
 */
extern int  event_add(int fd, int event, event_fun_t *fun, void *data);
/**
 * @return 0: success
 *         1: fd-event is not found
 *        -1: error
 */
extern int  event_del(int fd, int event);

/**
 * start event loop
 * @return 0: finish loop normally
 *        -1: finish loop unnormally
 */
extern int  event_loop();

#endif

