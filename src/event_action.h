#ifndef EVENT_ACTION__
#define EVENT_ACTION__

#include "event.h"

extern void event_accept_request(event_t *ev);
extern void event_init_http_request(event_t *ev);
extern void event_parse_http_head(event_t *ev);

/* respond the special page in the memory */
extern void event_respond_page(event_t *ev);

extern void event_read_file(event_t *ev);
extern void event_send_file(event_t *ev);

#endif
