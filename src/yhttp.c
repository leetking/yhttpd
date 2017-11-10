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

#include "log.h"
#include "common.h"
#include "setting.h"
#include "worker.h"

#define BACKLOG     SOMAXCONN
#define WORKS       2

static int as_deamon = 0;
static int log = 0;
static uint16_t port = 80;

static pid_t works[WORKS];
static int32_t alive = 0;
static char quit = 0;

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
           "      -v <1,2>   verbose.\n"
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
    int v;
    while ((c = getopt(argc, argv, "r:p:dc:w:e:l:v:h")) != -1) {
        switch (c) {
        case 'r':
            strncpy(SETTING.root_path, optarg, strlen(optarg));
            yhttp_debug2("Master# path of root: %s\n", SETTING.root_path);
            break;
        case 'p':
            port = atoi(optarg);
            yhttp_debug2("Master# listening at port: %d\n", port);
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
            v = atoi(optarg);
            if (v > 0)
                yhttp_log_set(LOG_INFO+v);
            break;
        case '?':   /* error */
        case 'h':
        default:
            help(argc, argv);
            ret = -1;
            break;
        }
    }
    return ret;
}

static void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags) {
        yhttp_warn("Master# Get fd(%d) status error: %s\n", fd, strerror(errno));
        return;
    }
    if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
        yhttp_warn("Master# Set fd(%d) as nonblock error: %s\n", fd, strerror(errno));
}

static void kill_workers(int _)
{
    (void)_;
    yhttp_debug2("Master# catch SIGINT, quit.\n");
    quit = 1;
    for (int i = 0; i < WORKS; i++)
        kill(works[i], SIGINT);
}

static void register_signal()
{
    signal(SIGINT, kill_workers);
}

int main(int argc, char **argv)
{
    int ret = 0;
    int on = 1;
    int sfd;                /* fd of server */
    struct sockaddr_in sip;

    if (0 != parse_opt(argc, argv))
        return 1;

    /* TODO support ipv6 */
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sfd) {
        yhttp_error("Master# Init socket error: %s\n", strerror(errno));
        return 2;
    }
    memset(&sip, 0, sizeof(sip));
    sip.sin_family = AF_INET;
    sip.sin_port   = htons(port);
    sip.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sfd, (struct sockaddr*)&sip, sizeof(sip))) {
        yhttp_error("Master# Bind to any address(0.0.0.0:%d): %s\n", port, strerror(errno));
        ret = 3;
        goto sfd_err;
    }

    /* TODO reuse address for DEBUG, what is meaning? */
    if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
        yhttp_warn("Master# setsockopt: %s\n", strerror(errno));
    set_nonblock(sfd);

    yhttp_info("Master# Listen at http://0.0.0.0:%d\n", port);
    if (-1 == listen(sfd, BACKLOG)) {
        yhttp_warn("Master# Listen at port(%d) error, BACKLOG is %d: %s\n", port, BACKLOG, strerror(errno));
        ret = 4;
        goto sfd_err;
    }

    sem_t *sem = sem_open(ACCEPT_LOCK, O_CREAT, 0644, 1);
    if (SEM_FAILED == sem) {
        yhttp_error("Master# Init lock %s: %s\n", ACCEPT_LOCK, strerror(errno));
        ret = 5;
        goto sfd_err;
    }

    /* TODO bind work process to cpu */
    /* fork and run the worker */
    for (int i = 0; i < WORKS; i++) {
        pid_t pid = fork();
        switch (pid) {
        case 0: /* child */
            exit(run_worker(i, sfd));
            break;
        case -1:
            yhttp_warn("Master# Fork error for %d#: %s\n", i, strerror(errno));
            break;
        default:
            works[i] = pid;
            alive |= 1<<i;
            yhttp_debug("Master# Start work process %d# %d\n", i, pid);
            break;
        }
    }

    /* register some signals and handle it */
    register_signal();

    /* main loop: manager worker process */
    for (;;) {
        int id;
        pid_t pid = wait(NULL);

        /* find the id of work process */
        for (id = 0; id < WORKS; id++) {
            if (pid == works[id]) {
                works[id] = -1;
                alive &= ~(1<<id);
                break;
            }
        }
        yhttp_info("Master# Work process %d# quits\n", id);
        if (quit && !alive) {
            yhttp_info("Master# All work process have quited\n");
            break;
        }
        
        /* need restart work process */
        if (!quit) {
            pid_t neopid = fork();
            if (0 == neopid) {
                exit(run_worker(id, sfd));

            } else if (-1 == neopid) {
                yhttp_warn("Master# Restart work process %d# error\n", id);
            } else {
                works[id] = neopid;
                alive |= 1<<id;
                yhttp_info("Master# Restart work process %d# success (%d -> %d)\n", id, pid, neopid);
            }
        }

        /* FIXME remove sleep(2) */
        sleep(2);
    }

    sem_unlink(ACCEPT_LOCK);
sfd_err:
    close(sfd);
    yhttp_info("Master# Bye.\n");

    return ret;
}
