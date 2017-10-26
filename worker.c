#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/select.h>
#include <netinet/ip.h>

#include "common.h"
#include "worker.h"
#include "connection.h"
#include "event.h"

struct accept_data {
    sem_t *lock;
    int sfd;
};

static void accept_request(void *_data)
{
    struct accept_data *data = (struct accept_data*)_data;
    struct sockaddr_in cip;
    socklen_t ciplen = sizeof(cip);
    if (-1 == sem_wait(data->lock)) {
        _M(LOG_DEBUG2, "sem_wait: %s\n", strerror(errno));
        return;
    }
    int cfd = accept(data->sfd, (struct sockaddr*)&cip, &ciplen);
    if (-1 == sem_post(data->lock))
        _M(LOG_DEBUG2, "sem_post: %s\n", strerror(errno));

    struct connection *con = connection_create(cfd);
    if (!con) {
        _M(LOG_DEBUG2, "connection_create error!\n");
        close(cfd);
        return;
    }
    con->socket.rdeof = 0;
    event_add(cfd, EVENT_READ, (event_fun_t*)parse_request, con);
}

extern int run_worker(int const sfd)
{
    event_init();
    sem_t *lock = sem_open(ACCEPT_LOCK, O_EXCL);
    if (lock == SEM_FAILED) {
        _M(LOG_ERROR, "worker sem: %s\n", strerror(errno));
        return 1;
    }
    struct accept_data data;
    data.sfd = sfd;
    data.lock = lock;
    event_add(sfd, EVENT_READ, accept_request, &data);

    event_loop();

    event_del(sfd, EVENT_READ);
    sem_close(lock);
    event_quit();

    return 0;
}
