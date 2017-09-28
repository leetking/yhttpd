#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/select.h>
#include <netinet/ip.h>

#include "common.h"
#include "worker.h"
#include "set.h"

#define SET_MAX   1024

extern int run_worker(int const sfd)
{
    /* TODO  添加负载均衡 */
    int maxfd = sfd;
    int ret = 0;
    fd_set rdset, wrset;
    set_t *set = set_create(SET_MAX);

    if (!set) {
        _M("init set error.\n");
        ret = 1;
        return ret;
    }
    set_add(set, sfd);
    sem_t *sem = sem_open(ACCEPT_LOCK, O_EXCL);
    if (sem == SEM_FAILED) {
        _M("worker sem: %s\n", strerror(errno));
        ret = 1;
        goto err_free_set;
    }
    for (;;) {
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        FD_SET(sfd, &rdset);
        int fd;
        set_foreach(set, fd) {
            FD_SET(fd, &rdset);
            FD_SET(fd, &wrset);
        }
        int s = select(sfd+1, &rdset, &wrset, NULL, NULL);
        if (-1 == s) {
            _M("select: %s\n", strerror(errno));
            continue;
        }

        set_foreach(set, fd) {
            /* has client */
            if (sfd == fd && FD_ISSET(fd, &rdset)) {
                int cfd;
                struct sockaddr_in cip;
                socklen_t ciplen = sizeof(cip);
                if (-1 == sem_wait(sem)) {
                    _M("sem_wait: %s\n", strerror(errno));
                    ret = 1;
                    goto err_close_sem;
                }
                cfd = accept(sfd, (struct sockaddr*)&cip, &ciplen);
                /* TODO record log */
                maxfd = MAX(maxfd, cfd);
                set_add(set, cfd);
                if (-1 == cfd)
                    _M("accept: %s\n", strerror(errno));
                if (-1 == sem_post(sem)) {
                    _M("sem_post: %s\n", strerror(errno));
                    ret = 1;
                    goto err_close_sem;
                }

            /* normal read */
            } else if (FD_ISSET(fd, &rdset)) {

            /* normal write */
            } else if (FD_ISSET(fd, &wrset)) {

            }
        }
    }

err_close_sem:
    sem_close(sem);
err_free_set:
    set_destory(set);

    return ret;
}
