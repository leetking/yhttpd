#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>

#include "memory.h"
#include "common.h"
#include "setting.h"
#include "log.h"

struct setting_t SETTING;

struct env {
    char *cfg_map;
    int cfg_size;
} ENV;

enum {
    vars_timeout = 0,
    vars_accept_lock,
    vars_backlog,
    vars_worker,
};

enum {
    server_port = 0,
    server_user,
    server_host,
    server_error_pages,
};

static void free_server_map_list(struct setting_server_map *list)
{
    for (struct setting_server_map *n = list; n; n = list) {
        list = list->next;
        yhttp_free(n->setting);
        yhttp_free(n);
    }
}

static int setting_parse_fcgi(char *start, char *end, struct setting_fastcgi *fcgi)
{
    enum {
        ps_init = 0,

        ps_key_bf,      ps_key,
        ps_comment,
        ps_value_bf,    ps_value,
        ps_equal,

        ps_af_local_bf, ps_af_local,
        ps_host_bf,     ps_host,
        ps_port_bf,     ps_port,

        ps_end,
    };
    int state = ps_init;
    char *p, *pos = start;
    for (p = start; p < end; p++) {
        switch (state) {
        case ps_init:
            if (isspace(*p))
                break;
            if ('{' == *p) {
                state = ps_key_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_key_bf:
            if (isspace(*p))
                break;
            if ('#' == *p) {
                state = ps_comment;
                break;
            }
            if ('s' == *p) {
                pos = p;
                state = ps_key;
                break;
            }
            if ('}' == *p) {
                state = ps_end;
                break;
            }
            return YHTTP_ERROR;

        case ps_key:
            if (isalpha(*p) || '_' == *p)
                break;
            if (isspace(*p) || '=' == *p) {
                if (0 != strncmp("server", pos, p-pos))
                    return YHTTP_ERROR;
                state = ('=' == *p)? ps_value_bf: ps_equal;
                break;
            }
            return YHTTP_ERROR;

        case ps_equal:
            if (isspace(*p))
                break;
            if ('=' == *p) {
                state = ps_value_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_value_bf:
            if (isspace(*p))
                break;
            if (isalpha(*p)) {
                pos = p;
                state = ps_value;
                break;
            }
            return YHTTP_ERROR;

        case ps_value:
            if (isalnum(*p))
                break;
            if (':' == *p) {
                if (!strncmp("unix", pos, 4)) {
                    fcgi->scheme = YHTTP_SETTING_FASTCGI_UNIX;
                    state = ps_af_local_bf;
                } else if (!strncmp("tcp6", pos, 4)) {
                    fcgi->scheme = YHTTP_SETTING_FASTCGI_TCP6;
                    state = ps_host_bf;
                } else if (!strncmp("tcp", pos, 3)) {
                    fcgi->scheme = YHTTP_SETTING_FASTCGI_TCP;
                    state = ps_host_bf;
                } else {
                    return YHTTP_ERROR;
                }
                break;
            }
            return YHTTP_ERROR;
        case ps_af_local_bf:
            if (isprint(*p) && ';' != *p && '}' != *p) {
                pos = p;
                state = ps_af_local;
                break;
            }
            return YHTTP_ERROR;

        case ps_af_local:
            if (isprint(*p) && ';' != *p && '}' != *p)
                break;
            if (';' == *p) {
                fcgi->host.str = pos;
                fcgi->host.len = p-pos;
                state = ps_key_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_host_bf:
            if (isalnum(*p) || '.' == *p) {
                pos = p;
                state = ps_host;
                break;
            }
            return YHTTP_ERROR;

        case ps_host:
            if (isalnum(*p) || '.' == *p)
                break;
            if (':' == *p) {
                fcgi->host.str = pos;
                fcgi->host.len = p-pos;
                state = ps_port_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_port_bf:
            if (isdigit(*p)) {
                pos = p;
                state = ps_port;
                break;
            }
            return YHTTP_ERROR;

        case ps_port:
            if (isdigit(*p))
                break;
            if (';' == *p) {
                int port = atoi(pos);
                if (port == 0)
                    return YHTTP_ERROR;
                fcgi->port = port;
                state = ps_key_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_comment:
            if ('\n' != *p)
                break;
            state = ps_key_bf;
            break;

        case ps_end:
            if (isspace(*p))
                break;
            if (';' == *p)
                return p-start;
            return YHTTP_ERROR;

        default:
            BUG_ON("unkown state when parse fcgi");
            return YHTTP_ERROR;
        }
    }
    return YHTTP_OK;
}

static int setting_parse_vars_value(int key_id, char *start, char *end, struct setting_vars *vars)
{
    switch (key_id) {
    case vars_timeout:
        if (1 == sscanf(start, "%d", &vars->timeout)) {
            if (vars->timeout <= 0)
                vars->timeout = YHTTP_TIMEOUT_CFG;
            break;
        }
        return YHTTP_ERROR;

    case vars_accept_lock:
        vars->accept_lock.str = start;
        vars->accept_lock.len = end-start;
        break;

    case vars_backlog:
        if (1 == sscanf(start, "%d", &vars->backlog)) {
            if (vars->backlog <= 0)
                vars->backlog = YHTTP_BACKLOG_CFG;
            break;
        }
        return YHTTP_ERROR;

    case vars_worker:
        if (1 == sscanf(start, "%d", &vars->worker)) {
            if (vars->worker <= 0)
                vars->worker = YHTTP_WORKER_CFG;
            break;
        }
        return YHTTP_ERROR;

    default:
        BUG_ON("setting parse vars value unkown status");
        return YHTTP_ERROR;
    }
    return YHTTP_OK;
}

static int setting_parse_vars(char *str, char *end, struct setting_vars *vars)
{
    enum {
        ps_init = 0,
        ps_comment,
        ps_word_bf,   ps_word,
        ps_equal,
        ps_value_bf,   ps_value,

        ps_end,
    };

    static struct {
        string_t key;
        int id;
    } vars_keys[] = {
        {string_newstr("timeout"), vars_timeout},
        {string_newstr("accept_lock"), vars_accept_lock},
        {string_newstr("backlog"), vars_backlog},
        {string_newstr("worker"), vars_worker},
    };


    char *p, *pos;
    int key_id;
    int state = ps_init;
    pos = str;
    for (p = str; p < end; p++) {
        switch (state) {
        case ps_init:
            if (isspace(*p))
                break;
            if ('{' == *p) {
                state = ps_word_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_word_bf:
            if (isspace(*p))
                break;
            if ('#' == *p) {
                state = ps_comment;
                break;
            }
            if (isalpha(*p) || '_' == *p) {
                pos = p;
                state = ps_word;
                break;
            }
            if ('}' == *p) {
                state = ps_end;
                break;
            }
            return YHTTP_ERROR;

        case ps_word:
            if (isalpha(*p) || '_' == *p)
                break;
            if (isspace(*p) || '=' == *p) {
                yhttp_debug2("word: %.*s\n", p-pos, pos);
                size_t i;
                for (i = 0; i < ARRSIZE(vars_keys); i++) {
                    if (vars_keys[i].key.len == p-pos
                            && !strncmp(pos, vars_keys[i].key.str, vars_keys[i].key.len)) {
                        key_id = vars_keys[i].id;
                        break;
                    }
                }
                if (i == ARRSIZE(vars_keys))
                    return YHTTP_ERROR;
                state = isspace(*p)? ps_equal: ps_value_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_equal:
            if (isspace(*p))
                break;
            if ('=' == *p) {
                state = ps_value_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_value_bf:
            if (isspace(*p))
                break;
            if (isprint(*p) && ';' != *p && '}' != *p) {
                pos = p;
                state = ps_value;
                break;
            }
            return YHTTP_ERROR;

        case ps_value:
            if (isprint(*p) && ';' != *p)
                break;
            if (';' == *p) {
                yhttp_debug2("value: %.*s\n", p-pos, pos);
                int s = setting_parse_vars_value(key_id, pos, p, vars);
                if (s == YHTTP_ERROR)
                    return YHTTP_ERROR;
                state = ps_word_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_end:
            if (isspace(*p))
                break;
            if (';' == *p) {
                return p-str;
            }
            break;

        case ps_comment:
            if ('\n' == *p)
                state = ps_word_bf;
            break;

        default:
            BUG_ON("unkown state when parse vars");
            return YHTTP_ERROR;
        }
    }
    return YHTTP_ERROR;
}

static int setting_parse_server_map(char *start, char *end, struct setting_server_map *new)
{
    struct setting_static *sta;
    enum {
        ps_init = 0,
        ps_word_bf, ps_word,
        ps_comment,

        ps_equal,
        ps_value_bf, ps_value,

        ps_fcgi,

        ps_end,
    };
    int state = ps_init;
#define SERVER_MAP_STATIC_ROOT  0
#define SERVER_MAP_STATIC_INDEX 1
#define SERVER_MAP_FCGI_SERVER  2
    int key_id = -1;
    char *p, *pos = start;
    for (p = start; p < end; p++) {
        switch (state) {
        case ps_init:
            if (isspace(*p))
                break;
            if ('{' == *p) {
                state = ps_word_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_word_bf:
            if (isspace(*p))
                break;
            if (isalpha(*p) || '_' == *p) {
                pos = p;
                state = ps_word;
                break;
            }
            if ('#' == *p) {
                state = ps_comment;
                break;
            }
            if ('}' == *p) {
                state = ps_end;
                break;
            }
            return YHTTP_ERROR;

        case ps_word:
            if (isalpha(*p) || '_' == *p)
                break;
            if (isspace(*p) || '{' == *p || '=' == *p) {
                if (!strncasecmp("fastcgi", pos, 7) && '=' != *p) {
                    if (new->type == YHTTP_SETTING_STATIC) {
                        yhttp_free(new->setting);
                        new->setting = NULL;
                        return YHTTP_ERROR;
                    }
                    struct setting_fastcgi *fcgi = yhttp_malloc(sizeof(*fcgi));
                    new->setting = fcgi;
                    new->type = YHTTP_SETTING_FASTCGI;
                    if ('{' == *p)
                        p--;

                    int s = setting_parse_fcgi(p, end, fcgi);
                    if (YHTTP_ERROR == s) {
                        yhttp_free(new->setting);
                        new->setting = NULL;
                        return YHTTP_ERROR;
                    }
                    p += s;
                    state = ps_word_bf;
                    break;
                }

                if ((!strncmp("root", pos, 4) || !strncmp("index", pos, 5)) && '{' != *p) {
                    yhttp_debug2("key: %.*s\n", p-pos, pos);
                    if (new->type == YHTTP_SETTING_FASTCGI) {
                        yhttp_free(new->setting);
                        new->setting = NULL;
                        return YHTTP_ERROR;
                    }
                    if (new->setting == NULL) {
                        struct setting_static *sta = yhttp_malloc(sizeof(*sta));
                        sta->index.str = "index.html";
                        sta->index.len = SSTR_LEN("index.html");
                        new->setting = sta;
                        new->type = YHTTP_SETTING_STATIC;
                    }
                    key_id = ('r' == *pos)? SERVER_MAP_STATIC_ROOT: SERVER_MAP_STATIC_INDEX;
                    state = ('=' == *p)? ps_value_bf: ps_equal;
                    break;
                }
                return YHTTP_ERROR;
            }
            return YHTTP_ERROR;

        case ps_equal:
            if (isspace(*p))
                break;
            if ('=' == *p) {
                state = ps_value_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_value_bf:
            if (isspace(*p))
                break;
            if (isprint(*p) && ';' != *p && '}' != *p) {
                pos = p;
                state = ps_value;
                break;
            }
            return YHTTP_ERROR;

        case ps_value:
            if (isprint(*p) && ';' != *p && '}' != *p)
                break;
            if (';' == *p || isspace(*p)) {
                yhttp_debug2("value: %.*s\n", p-pos, pos);
                switch (key_id) {
                case SERVER_MAP_STATIC_ROOT:
                    sta = new->setting;
                    sta->root.str = pos;
                    sta->root.len = p-pos;
                    break;
                case SERVER_MAP_STATIC_INDEX:
                    sta = new->setting;
                    sta->index.str = pos;
                    sta->index.len = p-pos;
                    break;
                default:
                    BUG_ON("unkown key in parse server map static");
                    return YHTTP_ERROR;
                }
                state = ps_word_bf;
                break;
            }
            return YHTTP_ERROR;

        case ps_comment:
            if ('\n' == *p)
                state = ps_word_bf;
            break;

        case ps_end:
            if (isspace(*p))
                break;
            if (';' == *p) {
                return p-start;
            }
            break;

        default:
            BUG_ON("unkown status in parse server map");
            return YHTTP_ERROR;
        }
    }
    return YHTTP_AGAIN;
}

static int setting_parse_server(char *str, char *end, struct setting_server *ser)
{
    enum {
        ps_init = 0,
        ps_comment,
        ps_word_bf,     ps_word,
        ps_equal,
        ps_value_bf,    ps_value,

        ps_location_bf, ps_location,

        ps_end,
    };
    static struct {
        string_t key;
        int id;
    } keys[] = {
        {string_newstr("port"), server_port},
        {string_newstr("user"), server_user},
        {string_newstr("host"), server_host},
        {string_newstr("error_pages"), server_error_pages},
    };
    int state = ps_init;
    char *p, *pos;
    int key_id;
    struct setting_server_map *map = NULL;
    pos = str;

#define free_and_return(err) do { \
    free_server_map_list(map); \
    return (err); \
} while (0)

    for (p = str; p < end; p++) {
        switch (state) {
        case ps_init:
            if (isspace(*p))
                break;
            if ('{' == *p) {
                state = ps_word_bf;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_word_bf:
            if (isspace(*p))
                break;
            if ('#' == *p) {
                state = ps_comment;
                break;
            }
            if (isalpha(*p)) {
                pos = p;
                state = ps_word;
                break;
            }
            if ('}' == *p) {
                state = ps_end;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_word:
            if (isalpha(*p) || '_' == *p)
                break;
            if (isspace(*p) || '=' == *p) {
                yhttp_debug2("word: %.*s\n", p-pos, pos);
                size_t i;
                for (i = 0; i < ARRSIZE(keys); i++) {
                    if (keys[i].key.len == p-pos
                            && !strncmp(pos, keys[i].key.str, keys[i].key.len)) {
                        key_id = keys[i].id;
                        break;
                    }
                }
                if (!strncasecmp("Location", pos, p-pos)) {
                    state = ps_location_bf;
                    break;
                }
                if (i == ARRSIZE(keys))
                    return YHTTP_ERROR;
                state = isspace(*p)? ps_equal: ps_value_bf;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_location_bf:
            if (isspace(*p))
                break;
            if ('/' == *p) {
                pos = p;
                state = ps_location;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_location:
            if (isprint(*p) && '{' != *p && !isspace(*p))
                break;
            if (isspace(*p) || '{' == *p) {
                yhttp_debug2("uri: %.*s\n", p-pos, pos);
                struct setting_server_map *new = yhttp_malloc(sizeof(*new));
                if (!new) {
                    free_server_map_list(map);
                    return YHTTP_ERROR;
                }
                new->next = map;
                new->uri.str = pos;
                new->uri.len = p-pos;
                map = new;
                if ('{' == *p)
                    p--;
                int s = setting_parse_server_map(p, end, new);
                if (YHTTP_ERROR == s)
                    free_and_return(YHTTP_ERROR);
                p += s;
                state = ps_word_bf;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_equal:
            if (isspace(*p))
                break;
            if ('=' == *p) {
                state = ps_value_bf;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_value_bf:
            if (isspace(*p))
                break;
            if (isprint(*p) && ';' != *p) {
                pos = p;
                state = ps_value;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_value:
            if (isprint(*p) && ';' != *p)
                break;
            if (';' == *p) {
                yhttp_debug2("value: %.*s\n", p-pos, pos);
                int v;
                switch (key_id) {
                case server_port:
                    v = atoi(pos);
                    if (v == 0)
                        return YHTTP_ERROR;
                    ser->port = v;
                    break;
                case server_user:
                    ser->user.str = pos;
                    ser->user.len = p-pos;
                    break;
                case server_host:
                    ser->host.str = pos;
                    ser->host.len = p-pos;
                    break;
                case server_error_pages:
                    ser->error_pages.str = pos;
                    ser->error_pages.len = p-pos;
                    break;
                default:
                    BUG_ON("unkown key in serer");
                    free_and_return(YHTTP_ERROR);
                }
                state = ps_word_bf;
                break;
            }
            free_and_return(YHTTP_ERROR);

        case ps_comment:
            if ('\n' == *p)
                state = ps_word_bf;
            break;

        case ps_end:
            if (isspace(*p))
                break;
            if (';' == *p) {
                if (map != NULL) {
                    yhttp_free(ser->map->setting);
                    yhttp_free(ser->map);
                    ser->map = NULL;
                    for (struct setting_server_map *n = map; n; n = map) {
                        map = n->next;
                        n->next = ser->map;
                        ser->map = n;
                    }
                }
                return p-str;
            }
            break;

        default:
            BUG_ON("unkown status in parse server");
            free_and_return(YHTTP_ERROR);
        }
    }

    free_and_return(YHTTP_ERROR);
}

static int setting_parse_from_string(char *str, int len, struct setting_t *setting)
{
    enum {
        ps_init = 0,
        ps_comment,

        ps_word,
    };
    char *p, *pos, *end = str+len;
    int state = ps_init;
    pos = str;
    for (p = str; p < end; p++) {
        switch (state) {
        case ps_init:
            if (isspace(*p))
                break;
            if ('#' == *p) {
                state = ps_comment;
                break;
            }
            if (isalpha(*p)) {
                pos = p;
                state = ps_word;
                break;
            }
            return YHTTP_ERROR;

        case ps_comment:
            if ('\n' != *p)
                break;
            if ('\n' == *p) {
                state = ps_init;
                break;
            }
            return YHTTP_ERROR;
        case ps_word:
            if (isalpha(*p))
                break;
            if (isspace(*p) || '{' == *p) {
                int s;
                char *st = ('{' == *p)? p-1: p;
                if (!strncasecmp("vars", pos, p-pos)) {
                    s = setting_parse_vars(st, end, &setting->vars);
                } else if (!strncasecmp("Server", pos, p-pos)) {
                    s = setting_parse_server(st, end, &setting->server);
                } else {
                    return YHTTP_ERROR;
                }
                if (YHTTP_ERROR == s)
                    return YHTTP_ERROR;
                p = st+s;
                state = ps_init;
                break;
            }
            return YHTTP_ERROR;
        default:
            BUG_ON("unkown parse configure state");
            return YHTTP_ERROR;
        }
    }

    return (state == ps_init)? YHTTP_OK: YHTTP_ERROR;
}

extern int setting_parse(char const *file, struct setting_t *setting)
{
    int ret = YHTTP_OK;
    struct stat st;
    int fd;

    if (-1 == stat(file, &st))
        return YHTTP_ERROR;
    fd = open(file, O_RDONLY);
    if (-1 == fd) {
        yhttp_debug("open configure file error: %s\n", strerror(errno));
        return YHTTP_ERROR;
    }
    ENV.cfg_size = st.st_size;
    ENV.cfg_map = yhttp_malloc(st.st_size);
    if (!ENV.cfg_map)
        return YHTTP_ERROR;
    if (ENV.cfg_size != read(fd, ENV.cfg_map, ENV.cfg_size)) {
        yhttp_free(ENV.cfg_map);
        ENV.cfg_map = NULL;
        ENV.cfg_size = 0;
        close(fd);
        return YHTTP_ERROR;
    }

    ret = setting_parse_from_string(ENV.cfg_map, ENV.cfg_size, setting);
    if (YHTTP_ERROR == ret) {
        yhttp_warn("Init setting from file(%s) error\n", file);
        setting_destroy(&SETTING);
    }

    close(fd);
    return ret;
}

extern int setting_dump(struct setting_t *setting)
{
    struct setting_server_map *map;
    struct setting_vars *vars = &setting->vars;
    struct setting_server *ser = &setting->server;
    struct setting_static *stat;
    struct setting_fastcgi *fcgi;
    printf("Vars {\n");
    printf("\tbacklog = %d;\n", vars->backlog);
    printf("\tevent_interval = %d;\n", vars->event_interval);
    printf("\ttimeout = %d;\n", vars->timeout);
    printf("\twoker = %d;\n", vars->worker);
    printf("\tlog = %.*s;\n", vars->log.len, vars->log.str);
    /* TODO finish all vars */
    printf("};\n");
    printf("Server {\n");
    printf("\tport = %d;\n", ser->port);
    printf("\tuser = %.*s;\n", ser->user.len, ser->user.str);
    printf("\thost = %.*s;\n", ser->host.len, ser->host.str);
    printf("\terror_pages = %.*s;\n", ser->error_pages.len, ser->error_pages.str);
    for (map = ser->map; map; map = map->next) {
        printf("\tLocation %.*s {\n", map->uri.len, map->uri.str);
        switch (map->type) {
        case YHTTP_SETTING_STATIC:
            stat = map->setting;
            printf("\t\troot = %.*s;\n", stat->root.len, stat->root.str);
            printf("\t\tindex = %.*s;\n", stat->index.len, stat->index.str);
            break;
        case YHTTP_SETTING_FASTCGI:
            fcgi = map->setting;
            switch (fcgi->scheme) {
            case YHTTP_SETTING_FASTCGI_TCP:
                printf("\t\tserver = tcp:%.*s:%d;\n", fcgi->host.len, fcgi->host.str, fcgi->port);
                break;
            case YHTTP_SETTING_FASTCGI_TCP6:
                printf("\t\tserver = tcp6:%.*s:%d;\n", fcgi->host.len, fcgi->host.str, fcgi->port);
                break;
            case YHTTP_SETTING_FASTCGI_UNIX:
                printf("\t\tserver = unix:%.*s;\n", fcgi->host.len, fcgi->host.str);
                break;
            default:
                printf("\t\tserver = unkown;\n");
                break;
            }
            break;
        default:
            BUG_ON("unkown setting type");
            break;
        }
        printf("\t};\n");
    }
    printf("};\n");
    return YHTTP_OK;
}

extern int setting_init_default(struct setting_t *setting)
{
    struct setting_vars *vars = &setting->vars;

    getcwd(setting->cwd, PATH_MAX-1);
    setting->cwd_len = strlen(setting->cwd);

    vars->worker = YHTTP_WORKER_CFG;
    vars->timeout = YHTTP_TIMEOUT_CFG;
    vars->event_interval = YHTTP_EVENT_INTERVAL_CFG;
    vars->backlog = YHTTP_BACKLOG_CFG;
    vars->log.str = "-", vars->log.len = 1;

    /* default server */
    struct setting_server *ser = &setting->server;
    struct setting_server_map *dft = yhttp_malloc(sizeof(*dft));
    struct setting_static *sta = yhttp_malloc(sizeof(*sta));
    if (!dft || !sta) {
        yhttp_free(dft);
        yhttp_free(sta);
        return YHTTP_ERROR;
    }
    ser->port = 80;
    ser->user.str = "http", ser->user.len = SSTR_LEN("http");
    ser->host.str = "*", ser->host.len = 1;
    ser->error_pages.str = ".", ser->error_pages.len = 1;
    ser->map = dft;

    dft->type = YHTTP_SETTING_STATIC;
    dft->uri.str = "/*", dft->uri.len = 2;
    dft->setting = sta;
    dft->next = NULL;

    sta->root.str = ".", sta->root.len = 1;
    sta->index.str = "index.html", sta->index.len = SSTR_LEN("index.html");

    ENV.cfg_map = NULL;
    ENV.cfg_size = 0;

    return YHTTP_OK;
}

extern int setting_destroy(struct setting_t *setting)
{
    free_server_map_list(setting->server.map);
    yhttp_free(ENV.cfg_map);

    return YHTTP_OK;
}
