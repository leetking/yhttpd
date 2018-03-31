#include "memory.h"
#include "common.h"
#include "setting.h"

struct setting_t SETTING;

extern int setting_parse(char const *file, struct setting_t *setting)
{
    return YHTTP_OK;
}

extern int setting_dump(struct setting_t *setting)
{
    return YHTTP_OK;
}

extern int setting_init_default(struct setting_t *setting)
{
    struct setting_vars *vars = &setting->vars;
    vars->worker = YHTTP_WORKER_CFG;
    vars->timeout = YHTTP_TIMEOUT_CFG;
    vars->event_interval = YHTTP_EVENT_INTERVAL_CFG;

    /* default server */
    struct setting_server *ser = &setting->server;
    struct setting_server_map *dft = yhttp_malloc(sizeof(*dft));
    struct setting_static *sta = yhttp_malloc(sizeof(*sta));
    if (!dft || !sta) {
        yhttp_free(dft);
        yhttp_free(sta);
        return YHTTP_ERROR;
    }
    /* ser->port = 80; */
    /* FIXME remove it */ ser->port = 8080;
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

    return YHTTP_OK;
}

extern int setting_destroy(struct setting_t *setting)
{
    struct setting_server_map *map, *next;
    for (map = setting->server.map; map; map = next) {
        next = map->next;
        yhttp_free(map->setting);
        yhttp_free(map);
    }

    return YHTTP_OK;
}
