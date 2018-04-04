#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/ip.h>

#include "common.h"
#include "log.h"
#include "worker.h"
#include "event.h"
#include "http.h"
#include "event_action.h"

/* 0.1 s */
#define LOOP_TIMEOUT    ((int)1e5)

static struct env {
    int id;
    int sfd;
    uint8_t quit:1;
} ENV;

static void worker_exit(int _)
{
    yhttp_debug2("catch SIGINT, worker quit.\n");
    ENV.quit = 1;
}

static void register_signal()
{
    signal(SIGINT, worker_exit);
    signal(SIGPIPE, SIG_IGN);       /* ignore singal SIGPIPE */
}

/* run as a process */
extern int run_worker(int id, int sfd)
{
    int ret = 0;
    event_t *rev;
    connection_t *c;

    register_signal();

    ENV.sfd  = sfd;
    ENV.id   = id;
    ENV.quit = 0;

    if (YHTTP_OK != event_init()) {
        yhttp_error("%d# Init event error\n");
        return 1;
    }
    if (YHTTP_OK != http_init()) {
        yhttp_error("%d# Init http error\n");
        goto init_err;
    }

    rev = event_malloc();
    if (!rev) {
        yhttp_error("%d# Allocate memory for ``rev'' error\n", ENV.id);
        ret = 1;
        goto rev_err;
    }
    c = connection_malloc();
    if (!c) {
        yhttp_error("%d# Allocate memory for ``con'' error\n", ENV.id);
        ret = 2;
        goto con_err;
    }

    c->fd = ENV.sfd;
    rev->handle = event_accept_request;
    rev->data = c;
    rev->accept = 1;

    connection_event_add_now(c, EVENT_READ, rev);
    for (;;) {
        /* TODO handle all master's instructions */
        if (ENV.quit)
            break;

        process_all_events();
        yhttp_debug2("finish a loop\n");
    }
    yhttp_debug2("break the loop\n");

    /* TODO destroy memory pool */
    rev = connection_event_del(c, EVENT_READ);

    connection_free(c);
con_err:
    event_free(rev);
rev_err:
    http_exit();
init_err:
    event_quit();

    return ret;
}
