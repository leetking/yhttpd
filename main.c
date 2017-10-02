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
#include <semaphore.h>

#include <linux/limits.h>

#include "common.h"
#include "worker.h"

#define BACKLOG     SOMAXCONN
#define WORKS       2

static int as_deamon = 0;
static int log = 0;
static char root[PATH_MAX] = ".";
static char cgi[PATH_MAX]  = ".";
static uint16_t port = 80;

static int works = WORKS;

static void help(int argc, char **argv)
{
    printf("usage: %s [OPTION]\n"
           "xxx\n"
           "      -r path    set root directory, default current directory(.)\n"
           "      -p port    specify a port, default 80\n"
           "      -d         run as daemon\n"
           "      -e path    specify cgi directory, default current directory(.)\n"
           "      -c num     create num number processer.\n"
           "      -l path    specify the log file.\n"
           "      -v <1,2,3> verbose.\n"
           "\n"
           "(c) %s\n",
           argv[0], VER);
}
static void parse_opt(int argc, char **argv)
{
    /* TODO parse option */
}

int main(int argc, char **argv)
{
    yhttp_log_set(LOG_DEBUG2);
    parse_opt(argc, argv);

    int ret = 0;
    int sfd;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sfd) {
        _M(LOG_ERROR, "socket: %s\n", strerror(errno));
        return 1;
    }
    int on = 1;
    if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
        _M(LOG_WARN, "setsockopt: %s\n", strerror(errno));
    struct sockaddr_in sip;
    memset(&sip, 0, sizeof(sip));
    sip.sin_family = AF_INET;
    sip.sin_port   = htons(port);
    sip.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sfd, (struct sockaddr*)&sip, sizeof(sip))) {
        _M(LOG_ERROR, "bind: %s\n", strerror(errno));
        ret = 1;
        goto sfd_err;
    }
    if (-1 == listen(sfd, BACKLOG)) {
        _M(LOG_ERROR, "listen(%d): %s\n", BACKLOG, strerror(errno));
        ret = 1;
        goto sfd_err;
    }

    /* TODO add singal to control childs. */

    sem_t *sem = sem_open(ACCEPT_LOCK, O_CREAT, 0644, 1);
    if (SEM_FAILED == sem) {
        _M(LOG_ERROR, "init lock %s: %s\n", ACCEPT_LOCK, strerror(errno));
        ret = 1;
        goto sfd_err;
    }

    for (int i = 0; i < works; i++) {
        int r;
        pid_t pid = fork();
        switch (pid) {
        case 0: /* child */
            exit(run_worker(sfd));
            break;
        case -1:
            _M(LOG_DEBUG2, "fork: %s\n", strerror(errno));
            break;
        default:
            _M(LOG_DEBUG2, "start child %d# %d\n", i+1, pid);
            break;
        }
    }

    /* main loop */
    for (;;) {
        pid_t pid, neopid;
        pid = wait(NULL);
        _M(LOG_DEBUG2, "child %d quit.\n", pid);
        neopid = fork();
        if (0 == neopid) {
            exit(run_worker(sfd));

        } else if (-1 == pid) {
            _M(LOG_DEBUG2, "restart child %d error.\n", neopid);
        } else
            _M(LOG_DEBUG2, "restart child %d -> %d.\n", pid, neopid);
    }

    sem_unlink(ACCEPT_LOCK);
sfd_err:
    close(sfd);
    _M(LOG_INFO, "Bye.\n");

    return ret;
}
