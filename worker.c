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
#include "set.h"

#define SET_MAX   1024

extern int run_worker(int const sfd)
{
    /* TODO  添加负载均衡 */
    int maxfd = sfd;
    int ret = 0;
    struct connection *con;
    fd_set rdset, wrset;
    set_t set = set_create(sizeof(con), (free_t*)connection_destory);

    if (!set) {
        _M("init set error.\n");
        ret = 1;
        return ret;
    }
    con = connection_create(sfd);   /* just add sfd */
    if (!con) {
        _M("create connection for sfd error!\n");
        return 1;
    }
    set_add(set, con);
    sem_t *sem = sem_open(ACCEPT_LOCK, O_EXCL);
    if (sem == SEM_FAILED) {
        _M("worker sem: %s\n", strerror(errno));
        ret = 1;
        goto set_err;
    }

    for (;;) {
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        struct connection *con;
        set_foreach(set, con) {
            FD_SET(con->fd, &rdset);
            if (sfd != con->fd) 
                FD_SET(con->fd, &wrset);
        }
        int s = select(sfd+1, &rdset, &wrset, NULL, NULL);
        if (-1 == s) {
            _M("select: %s\n", strerror(errno));
            continue;
        }

        set_foreach(set, con) {
            /* new connection of client */
            if (sfd == con->fd && FD_ISSET(con->fd, &rdset)) {
                struct sockaddr_in cip;
                socklen_t ciplen = sizeof(cip);
                if (-1 == sem_wait(sem)) {
                    _M("sem_wait: %s\n", strerror(errno));
                    continue;   /* ignore */
                }
                int cfd = accept(sfd, (struct sockaddr*)&cip, &ciplen);
                if (-1 == sem_post(sem))
                    _M("sem_post: %s\n", strerror(errno));
                if (-1 == cfd) {
                    _M("accept: %s, ignore.\n", strerror(errno));
                    continue;
                }
                /* TODO record log */
                maxfd = MAX(maxfd, cfd);
                con = connection_create(cfd);
                if (!con) {
                    _M("Cant accept new connection. close(%d).\n", cfd);
                    close(cfd); /* sorry, I cant accept */
                    continue;
                }
                set_add(set, con);

            /* normal read */
            } else if (FD_ISSET(con->fd, &rdset)) {
                connection_read(con);
            /* normal write */
            } else if (FD_ISSET(con->fd, &wrset)) {
                connection_write(con);
            }
            /* finish a transfer */
            if (!connection_isvalid(con)) {
                set_remove(set, con);
                connection_destory(&con);
            }
        }
    }

    sem_close(sem);
set_err:
    set_destory(&set);

    return ret;
}
