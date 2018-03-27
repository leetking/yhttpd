#include "http_file.h"

#if 0
extern void http_file_close(http_file_t *file)
{
    connection_t *c = file->duct;
    close(c->fd);
    if (c->wev) {
        event_del(c->wev, EVENT_WRITE);
        event_free(c->wev);
    }
    if (c->rev) {
        event_del(c->rev, EVENT_READ);
        event_free(c->rev);
    }
    connection_free(c);
}
#endif

