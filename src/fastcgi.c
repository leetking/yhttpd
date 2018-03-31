#include "fastcgi.h"

extern connection_t *fastcgi_connection_get(struct setting_fastcgi *setting)
{
    return NULL;
}

extern http_fastcgi_t *http_fastcgi_init()
{
    return NULL;
}

extern int http_fastcgi_parse_response()
{
    return 0;
}

extern void http_fastcgi_build_th()
{
}

extern void http_fastcgi_build_bh()
{
}
