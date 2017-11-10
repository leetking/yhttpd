#include <signal.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"
#include "event.h"
#include "log.h"
#include "connection.h"
#include "event_action.h"

extern void event_accept_request(event_t *rev)
{
    connection_t *c = rev->data;
    struct sockaddr_in cip;
    socklen_t ciplen = sizeof(cip);
    int cfd;
    event_t *ev;
    
    cfd = accept(c->fd, (struct sockaddr*)&cip, &ciplen);
    if (-1 == cfd) {
        yhttp_debug("%s accept error: %s\n", __FUNCTION__, strerror(errno));
        return;
    }

    ev = event_malloc();
    if (!ev) {
        yhttp_debug("%s event_malloc error\n", __FUNCTION__);
        goto ev_err;
    }
    c = connection_malloc();
    if (!c) {
        yhttp_debug("%s connection_malloc error\n", __FUNCTION__);
        goto con_err;
    }
    ev->data = c;
    ev->handle = event_init_http_request;

    c->fd = cfd;
    /* TODO connection read function */ c->read = NULL;
    c->tmstamp = current_msec+TIMEOUT;

    yhttp_debug("accept new request %d\n", c->fd);
    event_add_timer(ev);
    event_add(ev, EVENT_READ);
    return;

con_err:
    event_free(ev);
ev_err:
    close(cfd);
}

extern void event_init_http_request(event_t *rev)
{
    connection_t *c = rev->data;
    if (rev->timeout) {
        yhttp_info("client %d request timeout\n", c->fd);
        event_del(rev, EVENT_READ);
        event_del_timer(rev);
        close(c->fd);
        connection_free(c);
        event_free(rev);
        return;
    }

    yhttp_info("client working ...\n");
    yhttp_info("client work finish\n");

    event_del(rev, EVENT_READ);
    if (rev->timeout_set) {
        yhttp_debug2("client %d finish and dont timeout\n", c->fd);
        event_del_timer(rev);
    }
    close(c->fd);
    connection_free(c);
    event_free(rev);
}
