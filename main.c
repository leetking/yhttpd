#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "common.h"

#define BACKLOG     SOMAXCONN
#define WORKS       2
#define ACCEPT_LOCK "yhttp-"VER".lock"


static int is_deamon = 0;
static char root[PATH_MAX] = ".";
static char cgi[]  = ".";
static uint16_t port = 80;

static int works = WORKS;

static void help(int argc, char **argv)
{
    printf("usage: %s [OPTION]\n"
           "xxx\n"
           "      -r path   set root directory, default current directory(.)\n"
           "      -p port   specify a port, default 80\n"
           "      -d        run as daemon\n"
           "      -e path   specify cgi directory, default current directory(.)\n"
           "\n"
           "(c) %s\n",
           argv[0], VER);
}
static void parse_opt(int argc, char **argv)
{
    /* TODO parse option */

}

static int go(int fd)
{
    /*
     * TODO 完成 io 处理
     * 1. 添加新的 fd
     * 2. 通过高级 IO ，如：select 、epoll 来处里这些 fds
     *    先采用 select 来实现吧
     */
    return 0;
}

static int run_worker(int sfd)
{
    /* TODO  添加负载均衡 */
    sem_t *sem = sem_open(ACCEPT_LOCK, O_EXCL);
    if (sem == SEM_FAILED) {
        _M("worker sem: %s\n", strerror(errno));
        return 1;
    }
    int ret = 0;
    for (;;) {
        struct sockaddr_in cip;
        socklen_t ciplen = sizeof(cip);
        if (-1 == sem_wait(sem)) {
            _M("sem_wait: %s\n", strerror(errno));
            ret = 1;
            goto sem_err;
        }
        int fd = accept(sfd, (struct sockaddr*)&cip, &ciplen);
        if (-1 == fd)
            _M("accept: %s\n", strerror(errno));
        if (-1 == sem_post(sem)) {
            _M("sem_post: %s\n", strerror(errno));
            ret = 1;
            goto sem_err;
        }

        if (0 != go(fd))
            _M("deal client error!\n");

        close(fd);
    }

sem_err:
    sem_close(sem);
    return ret;
}

int main(int argc, char **argv)
{
    parse_opt(argc, argv);

    int ret = 0;
    int sfd;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sfd) {
        _M("socket: %s\n", strerror(errno));
        return 1;
    }
    int on = 1;
    if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
        _M("setsockopt: %s\n", strerror(errno));
        ret = 1;
        goto sfd_err;
    }
    struct sockaddr_in sip;
    memset(&sip, 0, sizeof(sip));
    sip.sin_family = AF_INET;
    sip.sin_port   = htons(port);
    sip.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sfd, (struct sockaddr*)&sip, sizeof(sip))) {
        _M("bind: %s\n", strerror(errno));
        ret = 1;
        goto sfd_err;
    }
    if (-1 == listen(sfd, BACKLOG)) {
        _M("listen(%d): %s\n", BACKLOG, strerror(errno));
        ret = 1;
        goto sfd_err;
    }

    /* TODO add singal to control childs. */

    sem_t *sem = sem_open(ACCEPT_LOCK, O_CREAT, 0644, 1);
    if (SEM_FAILED == sem) {
        _M("Init lock %s: %s\n", ACCEPT_LOCK, strerror(errno));
        ret = 1;
        goto sfd_err;
    }

    for (int i = 0; i < works; i++) {
        pid_t pid = fork();
        switch (pid) {
        case 0: /* child */
            exit(run_worker(sfd));
            break;
        case -1:
            _M("fork: %s\n", strerror(errno));
            break;
        default:
            _M("start child %d# %d\n", i+1, pid);
            break;
        }
    }

    /* main loop */
    for (;;) {
        pid_t pid, neopid;
        pid = wait(NULL);
        _M("child %d quit.\n", pid);
        neopid = fork();
        if (0 == pid) {
            exit(run_worker(sfd));
        } else if (-1 == pid) {
            _M("restart child %d error.\n", neopid);
        }
        _M("restart child %d -> %d.\n", pid, neopid);
    }

    sem_unlink(ACCEPT_LOCK);
sfd_err:
    close(sfd);
    _M("%s Bye.\n", argv[0]);

    return ret;
}
