#ifndef EVENT_ACTION__
#define EVENT_ACTION__

#include "event.h"

extern void event_accept_request(event_t *ev);
extern void event_init_http_request(event_t *ev);

#endif
