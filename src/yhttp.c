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
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <semaphore.h>

#include <linux/limits.h>

#include "log.h"
#include "common.h"
#include "setting.h"
#include "worker.h"

static int dump = 0;
static int as_daemon = 0;
static int verbose = 0;

static pid_t works[YHTTP_WORKER_MAX];
static int64_t alive = 0;
static char quit = 0;

static void help(int argc, char **argv)
{
    (void)argc;
    printf("Usage: %s [OPTIONS]\n"
           "a simple but powerful http server, just for studing.\n"
           "OPTIONS:\n"
           "      -r path    set `root` directory, default current directory(.)\n"
           "      -p num     listening port\n"
           "      -w num     number of spawning worker process\n"
           "      -l path    specify a log file\n"
           "      -d         run as daemon\n"
           "      -v <1,2>   verbose\n"
           "      -z         dump settings and quit, if no configuration file,"
                                "dump default configuration\n"
           "      -c cfg     specify a configuration file\n"
           "      -h         show this page\n"
           "\n"
           "%s v%s\n"
           "(C) GPL v3\n"
           "Author: leetking <li_Tking@163.com>\n",
           argv[0], argv[0], VER);
}

static int parse_opt(int argc, char **argv, struct setting_t *setting)
{
    int ret = YHTTP_OK;
    int c;
    int v;
    struct setting_vars *vars = &setting->vars;
    struct setting_server *ser = &setting->server;
    struct setting_static *dft_sta = (struct setting_static*)ser->map->setting;
    while ((c = getopt(argc, argv, "r:p:w:l:dv:zc:h")) != -1) {
        switch (c) {
        case 'r':
            dft_sta->root.str = optarg;
            dft_sta->root.len = strlen(optarg);
            yhttp_debug2("Master# root %s\n", dft_sta->root.str);
            break;
        case 'p':
            v = atoi(optarg);
            if (0 == v)
                yhttp_info("Master# the port is invalid, use the default port 80\n");
            else
                ser->port = v;
            yhttp_debug2("Master# listening at port: %d\n", ser->port);
            break;
        case 'w':
            v = atoi(optarg);
            if (0 == v)
                yhttp_info("Master# the count of worker is invalid\n");
            else
                vars->worker = MIN(v, YHTTP_WORKER_MAX);
            yhttp_debug2("Master# the count of worker : %d\n", vars->worker);
            break;
        case 'l':
            vars->log.str = optarg;
            vars->log.len = strlen(optarg);
            yhttp_debug2("Master# log file: %.*s\n", vars->log.len, vars->log.str);
            break;
        case 'd':
            as_daemon = 1;
            break;
        case 'v':
            v = atoi(optarg);
            if (v > 0)
                verbose = v;
            break;
        case 'z':
            dump = 1;
            break;
        case 'c':
            if (YHTTP_ERROR == setting_parse(optarg, &SETTING)) {
                yhttp_error("Configure file %s has syntax error\n", optarg);
                return YHTTP_ERROR;
            }
            break;
        case '?':   /* error */
        case 'h':
        default:
            help(argc, argv);
            ret = YHTTP_ERROR;
            break;
        }
    }
    return ret;
}

static void run_as_daemon(void)
{
    pid_t pid;
    umask(0);
    pid = fork();

    /* exit parent process */
    if (pid != 0)
        exit(0);
    setsid();
}

static void kill_workers(int _)
{
    (void)_;
    yhttp_debug2("Master# catch SIGINT, quit.\n");
    quit = 1;
    for (int i = 0; i < SETTING.vars.worker; i++)
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
    struct setting_vars const *VARS = &SETTING.vars;
    struct setting_server const *SER = &SETTING.server;

    if (YHTTP_OK != setting_init_default(&SETTING)) {
        yhttp_error("A fatal error in initial phase\n Quit\n");
        return 1;
    }
    if (YHTTP_OK != parse_opt(argc, argv, &SETTING))
        return 1;

    if (dump) {
        setting_dump(&SETTING);
        setting_destroy(&SETTING);
        return 0;
    }
    if (verbose > 0)
        yhttp_log_set(LOG_INFO+verbose);
    if (0 != strncmp("-", VARS->log.str, VARS->log.len)) {
        char buff[PATH_MAX+1];
        int len = MIN(VARS->log.len, PATH_MAX);
        memcpy(buff, VARS->log.str, len);
        buff[len] = '\0';
        freopen(buff, "a", stdout);
        freopen(buff, "a", stderr);
    }
    if (as_daemon)
        run_as_daemon();

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sfd) {
        yhttp_error("Master# Init socket error: %s\n", strerror(errno));
        return 2;
    }
    memset(&sip, 0, sizeof(sip));
    sip.sin_family = AF_INET;
    sip.sin_port   = htons(SER->port);
    sip.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sfd, (struct sockaddr*)&sip, sizeof(sip))) {
        yhttp_error("Master# Bind to any address(0.0.0.0:%d): %s\n",
                SER->port, strerror(errno));
        ret = 3;
        goto sfd_err;
    }

    if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
        yhttp_warn("Master# setsockopt: %s\n", strerror(errno));
    set_nonblock(sfd);

    yhttp_info("Master# Listen at http://%.*s:%d\n", SER->host.len,
            SER->host.str, SER->port);
    if (-1 == listen(sfd, VARS->backlog)) {
        yhttp_warn("Master# Listen at port(%d) error, BACKLOG is %d: %s\n",
                SER->port, VARS->backlog, strerror(errno));
        ret = 4;
        goto sfd_err;
    }

    sem_t *sem = sem_open(ACCEPT_LOCK, O_CREAT, 0644, 1);
    if (SEM_FAILED == sem) {
        yhttp_error("Master# Init lock %s: %s\n", ACCEPT_LOCK, strerror(errno));
        ret = 5;
        goto sfd_err;
    }

    /* fork and run the worker */
    for (int i = 0; i < VARS->worker; i++) {
        pid_t pid = fork();
        switch (pid) {
        case 0: /* child */
            set_cpu_affinity(i);
            ret = run_worker(i, sfd);
            setting_destroy(&SETTING);
            exit(ret);
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
        for (id = 0; id < VARS->worker; id++) {
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
                set_cpu_affinity(id);
                ret = run_worker(id, sfd);
                setting_destroy(&SETTING);
                exit(ret);

            } else if (-1 == neopid) {
                yhttp_warn("Master# Restart work process %d# error\n", id);
            } else {
                works[id] = neopid;
                alive |= 1<<id;
                yhttp_info("Master# Restart work process %d# "
                               "success (%d -> %d)\n", id, pid, neopid);
            }
        }
    }

    sem_close(sem);
    sem_unlink(ACCEPT_LOCK);
sfd_err:
    close(sfd);
    setting_destroy(&SETTING);
    yhttp_info("Master# Bye.\n");

    return ret;
}
