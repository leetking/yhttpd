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
    int ret = 0;
    struct connection *con;
    fd_set rdset, wrset;
    set_t set = set_create(NULL, (free_t*)connection_destory);

    if (!set) {
        _M(LOG_DEBUG2, "init set error.\n");
        ret = 1;
        return ret;
    }
    con = connection_create(sfd);   /* just add sfd */
    if (!con) {
        _M(LOG_DEBUG2, "create connection for sfd error!\n");
        return 1;
    }
    connection_settimeout(con, -1);
    set_add(set, con);
    _M(LOG_DEBUG2, "open sem at %s\n", ACCEPT_LOCK);
    sem_t *sem = sem_open(ACCEPT_LOCK, O_EXCL);
    if (sem == SEM_FAILED) {
        _M(LOG_DEBUG2, "worker sem: %s\n", strerror(errno));
        ret = 1;
        goto set_err;
    }

    for (;;) {
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        struct connection *con;
        int maxfd = sfd;
        set_foreach(set, con) {
            FD_SET(con->sktfd, &rdset);
            if (sfd != con->sktfd && connection_need_write_skt(con))
                FD_SET(con->sktfd, &wrset);
            maxfd = MAX(maxfd, con->sktfd);
            _M(LOG_DEBUG2, "select add sktfd %d\n", con->sktfd);
            if (-1 != con->fdro) {
                FD_SET(con->fdro, &rdset);
                maxfd = MAX(maxfd, con->fdro);
                _M(LOG_DEBUG2, "select add fdro %d\n", con->fdro);
            }
            if (-1 != con->fdwo && connection_need_write_file(con)) {
                FD_SET(con->fdwo, &wrset);
                maxfd = MAX(maxfd, con->fdwo);
                _M(LOG_DEBUG2, "select add fdwo %d\n", con->fdwo);
            }
        }
        _M(LOG_DEBUG2, "select maxfd %d\n", maxfd);
        int s = select(maxfd+1, &rdset, &wrset, NULL, NULL);
        _M(LOG_DEBUG2, "select return %d\n", s);
        if (-1 == s) {
            _M(LOG_WARN, "select: %s\n", strerror(errno));
            continue;
        }

        set_foreach(set, con) {
            /* new connection of client */
            if (sfd == con->sktfd && FD_ISSET(con->sktfd, &rdset)) {
                struct sockaddr_in cip;
                socklen_t ciplen = sizeof(cip);
                if (-1 == sem_wait(sem)) {
                    _M(LOG_DEBUG2, "sem_wait: %s\n", strerror(errno));
                    continue;   /* ignore */
                }
                int cfd = accept(sfd, (struct sockaddr*)&cip, &ciplen);
                if (-1 == sem_post(sem))
                    _M(LOG_DEBUG2, "sem_post: %s\n", strerror(errno));
                if (-1 == cfd) {
                    _M(LOG_DEBUG2, "accept: %s, ignore.\n", strerror(errno));
                    continue;
                }
                /* TODO record log */
                maxfd = MAX(maxfd, cfd);
                con = connection_create(cfd);
                if (!con) {
                    _M(LOG_DEBUG2, "Cant accept new connection. close(%d).\n", cfd);
                    close(cfd); /* sorry, I cant accept */
                    continue;
                }
                set_add(set, con);

            } else {
                /* normal read */
                if (FD_ISSET(con->sktfd, &rdset)) {
                    connection_read_skt(con);
                }
                /* normal write */
                if (FD_ISSET(con->sktfd, &wrset)) {
                    connection_write_skt(con);

                }
                /* read, write file */
                if (FD_ISSET(con->fdro, &rdset)) {
                    _M(LOG_DEBUG2, "fdro %d read_file\n", con->fdro);
                    connection_read_file(con);
                }
                if (FD_ISSET(con->fdwo, &wrset)) {
                    connection_write_file(con);
                }
                /* finish a transfer */
                if (!connection_isvalid(con)) {
                    _M(LOG_DEBUG2, "Finish a connection %d r: %d w: %d timeout: %d\n",
                            con->sktfd, con->fdro, con->fdwo, con->timeout);
                    set_remove(set, con);
                    connection_destory(&con);
                }
            }
        }
    }

    sem_close(sem);
set_err:
    set_destory(&set);

    return ret;
}
