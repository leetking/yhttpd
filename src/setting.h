#ifndef SETTING_H__
#define SETTING_H__

#include <linux/limits.h>

#include "hash.h"
#include "common.h"

#define YHTTP_WORKER_MAX            (64)

/**
 * The Default Varables
 */
#define ACCEPT_LOCK                 ("yhttp-"VER".lock")
#define YHTTP_WORKER_CFG            (2)
#define YHTTP_TIMEOUT_CFG           (5*1000)        /* 5 s */
#define YHTTP_EVENT_INTERVAL_CFG    (1*1000)        /* 1 s */
#define YHTTP_BACKLOG_CFG           SOMAXCONN

#define YHTTP_CACHE_MAX_AGE_CFG      (3600)
#define YHTTP_BUFFER_SIZE_CFG        (4096)
#define YHTTP_LARGE_BUFFER_SIZE_CFG  (2*YHTTP_BUFFER_SIZE_CFG)

#define HTTP_COOKIE_PAIR_MAX_CFG    12


struct setting_static {
    string_t root;
    string_t index; /* FIXME index is a list */
};
struct setting_fastcgi {
    string_t server;
#define YHTTP_SETTING_FASTCGI_PIPE  1
#define YHTTP_SETTING_FASTCGI_UNIX  2
#define YHTTP_SETTING_FASTCGI_TCP   3
#define YHTTP_SETTING_FASTCGI_UDP   4
    uint16_t scheme;
    string_t host;
    uint16_t port;
};

struct setting_server_map {
    string_t uri;
    void *setting;
#define YHTTP_SETTING_STATIC    1
#define YHTTP_SETTING_FASTCGI   2
    int type;
#define YHTTP_SETTING_SERVER_MAP_MAX    7
    struct setting_server_map *next;
};

struct setting_server {
    uint16_t port;
    string_t user;
    string_t host;
    string_t error_pages;
    struct setting_server_map *map;
};

struct setting_vars {
    int worker;
    int event_interval;
    int timeout;
    int backlog;
};

struct setting_t {
    struct setting_vars vars;
    struct setting_server server;
};

extern struct setting_t SETTING;

extern int setting_parse(char const *file, struct setting_t *setting);
extern int setting_init_default(struct setting_t *setting);
extern int setting_destroy(struct setting_t *setting);

#endif
