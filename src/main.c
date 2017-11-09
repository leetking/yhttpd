#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

#include <getopt.h>
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
static uint16_t port = 80;

static pid_t works[WORKS];
static int8_t quit = 0;

static void help(int argc, char **argv)
{
    printf("usage: %s [OPTION]\n"
           "a sample http server, for studing.\n"
           "OPTIONS:\n"
           "      -r path    set `www` directory, default current directory(.)\n"
           "      -p port    specify a port, default 80\n"
           "      -d         run as daemon\n"
           "      -c path    specify `cgi` directory, default current directory(.)\n"
           "      -w num     create num number worker process.\n"
           "      -e path    specify `status-codes` file directory, default `$www/err-codes`\n"
           "      -l path    specify the log file.\n"
           "      -v <1,2,3> verbose.\n"
           "      -h         show this page.\n"
           "\n"
           "%s %s\n"
           "(c) GPL v3\n"
           "Author: leetking <li_Tking@63.com>\n",
           argv[0], argv[0], VER);
}
static int parse_opt(int argc, char **argv)
{
    int ret = 0;
    int c;
    while ((c = getopt(argc, argv, "r:p:dc:w:e:l:v:h")) != -1) {
        switch (c) {
        case 'r':
            strncpy(root_path, optarg, strlen(optarg));
            _M(LOG_DEBUG2, "root_path: %s\n", root_path);
            break;
        case 'p':
            port = atoi(optarg);
            _M(LOG_DEBUG2, "port: %d\n", port);
            break;
        case 'd':
            break;
        case 'c':
            break;
        case 'w':
            break;
        case 'e':
            break;
        case 'l':
            break;
        case 'v':
            break;
        case '?':   /* error */
            ret = -1;
            break;
        case 'h':
        default:
            help(argc, argv);
            ret = -1;
            break;
        }
    }
    return ret;
}
static void deal_sigint(int x)
{
    _M(LOG_DEBUG2, "catch SIGINT, quit.\n");
    quit = 1;
    for (int i = 0; i < WORKS; i++)
        kill(works[i], SIGINT);
}

int main(int argc, char **argv)
{
    /* FIXME remove yhttp_log_set(LOG_DEBUG2); */
    yhttp_log_set(LOG_DEBUG2);

    if (0 != parse_opt(argc, argv))
        return 1;

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
    _M(LOG_DEBUG2, "Listen at 0.0.0.0:%d\n", port);
    if (-1 == listen(sfd, BACKLOG)) {
        _M(LOG_ERROR, "listen(%d): %s\n", BACKLOG, strerror(errno));
        ret = 1;
        goto sfd_err;
    }

    /* TODO comsider change to pthread_mutex_t and mmap */
    sem_t *sem = sem_open(ACCEPT_LOCK, O_CREAT, 0644, 1);
    if (SEM_FAILED == sem) {
        _M(LOG_ERROR, "init lock %s: %s\n", ACCEPT_LOCK, strerror(errno));
        ret = 1;
        goto sfd_err;
    }

    for (int i = 0; i < WORKS; i++) {
        pid_t pid = fork();
        switch (pid) {
        case 0: /* child */
            exit(run_worker(sfd));
            break;
        case -1:
            _M(LOG_WARN, "fork: %s\n", strerror(errno));
            break;
        default:
            works[i] = pid;
            _M(LOG_DEBUG2, "start child %d# %d\n", i+1, pid);
            break;
        }
    }

    /* TODO add singal to control childs. */
    signal(SIGINT, deal_sigint);
    signal(SIGCHLD, SIG_IGN);

    /* main loop */
    for (;;) {
        pid_t pid, neopid;
        pid = wait(NULL);
        if (quit) break;
        _M(LOG_DEBUG2, "child %d quit.\n", pid);

        neopid = fork();
        if (0 == neopid) {
            exit(run_worker(sfd));

        } else if (-1 == pid) {
            _M(LOG_WARN, "restart child %d error.\n", neopid);
        } else {
            for (int i = 0; i < WORKS; i++) {
                if (pid == works[i])
                    works[i] = neopid;
            }
            _M(LOG_DEBUG2, "restart child %d -> %d.\n", pid, neopid);
        }
    }

    sem_unlink(ACCEPT_LOCK);
sfd_err:
    close(sfd);
    _M(LOG_INFO, "Bye.\n");

    return ret;
}
