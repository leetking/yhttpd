#include <ctype.h>
#include <string.h>
#include <strings.h>    /* strncasecmp */
#include <limits.h>
#include <stdio.h>

#include "log.h"
#include "common.h"
#include "http_parser.h"
#include "hash.h"
#include "http_mime.h"

static struct env {
    hash_t hdr_fields;
} ENV;

enum {
    /* General-Header */
    HDR_CACHE_CONTROL = 1,
    HDR_CONNECTION,
    HDR_DATE,                   /* 1.0 */
    HDR_PRAGMA,                 /* 1.0 */
    HDR_TRAILER,
    HDR_TRANSFER_ENCODING,
    HDR_UPGRADE,
    HDR_VIA,
    HDR_WARNING,
    /* Entity-Header */
    HDR_ALLOW,                  /* 1.0 */
    HDR_CONTENT_ENCODING,       /* 1.0 */
    HDR_CONTENT_LENGTH,         /* 1.0 */
    HDR_CONTENT_LANGUAGE,
    HDR_CONTENT_MD5,
    HDR_CONTENT_RANGE,
    HDR_CONTENT_TYPE,           /* 1.0 */
    HDR_EXPIRES,                /* 1.0 */
    HDR_LAST_MODIFIED,          /* 1.0 */
    /* Request-Header */
    HDR_ACCEPT,
    HDR_ACCEPT_CHARSET,
    HDR_ACCEPT_ENCODING,
    HDR_ACCEPT_LANGUAGE,
    HDR_AUTHORIZATION,          /* 1.0 */
    HDR_EXPECT,
    HDR_FROM,                   /* 1.0 */
    HDR_HOST,
    HDR_IF_MATCH,
    HDR_IF_MODIFIED_SINCE,      /* 1.0 */
    HDR_IF_NONE_MATCH,
    HDR_IF_RANGE,
    HDR_IF_UNMODIFIED_SINCE,
    HDR_MAX_FOREARDS,
    HDR_PROXY_AUTHORICATION,
    HDR_RANGE,
    HDR_REFERER,                /* 1.0 */
    HDR_TE,
    HDR_USER_AGENT,
    HDR_COOKIE,

    HDR_UPGRADE_INSECURE_REQUESTS,
    HDR_KEEP_ALIVE,
    HDR_ORIGIN,
#if 0
    /* response header */
    HDR_ACCEPT_RANGES,
    HDR_AGE,
    HDR_ETAG,
    HDR_LOCATION,               /* 1.0 */
    HDR_PROXY_AUTHORICATE,
    HDR_PROXY_AFTER,
    HDR_SERVER,                 /* 1.0 */
    HDR_VARY,
    HDR_WWW_AUTHENTICAE,        /* 1.0 */

    HDR_SET_COOKIE,
#endif
};
static const struct {
    string_t key;
    int id;
} hdr_keys[] = {
    {string_newstr("cache-control"),            HDR_CACHE_CONTROL},
    {string_newstr("connection"),               HDR_CONNECTION},
    {string_newstr("date"),                     HDR_DATE},
    {string_newstr("pragma"),                   HDR_PRAGMA},
    {string_newstr("trailer"),                  HDR_TRAILER},
    {string_newstr("transfer-encoding"),        HDR_TRANSFER_ENCODING},
    {string_newstr("upgrade"),                  HDR_UPGRADE},
    {string_newstr("via"),                      HDR_VIA},
    {string_newstr("warning"),                  HDR_WARNING},
    {string_newstr("allow"),                    HDR_ALLOW},
    {string_newstr("content-encoding"),         HDR_CONTENT_ENCODING},
    {string_newstr("content-length"),           HDR_CONTENT_LENGTH},
    {string_newstr("content-language"),         HDR_CONTENT_LANGUAGE},
    {string_newstr("content-md5"),              HDR_CONTENT_MD5},
    {string_newstr("content-range"),            HDR_CONTENT_RANGE},
    {string_newstr("content-type"),             HDR_CONTENT_TYPE},
    {string_newstr("expires"),                  HDR_EXPIRES},
    {string_newstr("last-modified"),            HDR_LAST_MODIFIED},
    {string_newstr("accept"),                   HDR_ACCEPT},
    {string_newstr("accept-charset"),           HDR_ACCEPT_CHARSET},
    {string_newstr("accept-encoding"),          HDR_ACCEPT_ENCODING},
    {string_newstr("accept-language"),          HDR_ACCEPT_LANGUAGE},
    {string_newstr("authorization"),            HDR_AUTHORIZATION},
    {string_newstr("expect"),                   HDR_EXPECT},
    {string_newstr("from"),                     HDR_FROM},
    {string_newstr("host"),                     HDR_HOST},
    {string_newstr("if-match"),                 HDR_IF_MATCH},
    {string_newstr("if-modified-since"),        HDR_IF_MODIFIED_SINCE},
    {string_newstr("if-none-match"),            HDR_IF_NONE_MATCH},
    {string_newstr("if-range"),                 HDR_IF_RANGE},
    {string_newstr("if-unmodified-since"),      HDR_IF_UNMODIFIED_SINCE},
    {string_newstr("max-foreards"),             HDR_MAX_FOREARDS},
    {string_newstr("proxy-authorication"),      HDR_PROXY_AUTHORICATION},
    {string_newstr("range"),                    HDR_RANGE},
    {string_newstr("referer"),                  HDR_REFERER},
    {string_newstr("te"),                       HDR_TE},
    {string_newstr("user-agent"),               HDR_USER_AGENT},
    {string_newstr("cookie"),                   HDR_COOKIE},
    {string_newstr("upgrade-insecure-requests"),HDR_UPGRADE_INSECURE_REQUESTS},
    {string_newstr("keep-alive"),               HDR_KEEP_ALIVE},
    {string_newstr("origin"),                   HDR_ORIGIN},
};

static int http_parse_time_gmt(time_t *t, char const *str, char const *end)
{
    /* TODO parse rfc 850 and asctime */
    BUG_ON(NULL == t);

    int year, month, day, hour, min, sec;
    char week[4];
    char mon[4];
    static char const *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    if (end-str < (int)SSTR_LEN("Sun, 25 Feb 2018 01:32:02 GMT"))
        return YHTTP_ERROR;

    if (11 != sscanf(str, "%c%c%c, %d %c%c%c %d %d:%d:%d GMT",
            week, week+1, week+2,
            &day, mon, mon+1, mon+2, &year, &hour, &min, &sec)) {
        return YHTTP_ERROR;
    }
    for (month = 0; month < 12; month++) {
        if (months[month][0] == mon[0] && months[month][1] == mon[1] && months[month][2] == mon[2])
            break;
    }
    if (month == 12)
        return 1;
    *t = (uint64_t)(365 * year + year / 4 - year / 100 + year / 400
            + 367 * month / 12 - 30
            + day - 1
            - 719527 + 31 + 28) * 86400 + hour * 3600 + min * 60 + sec;

    return YHTTP_OK;
}

static int http_parse_encoding(char const *str, char const *end)
{
    /* x-gzip, x-compress, gzip, compress, deflate, identity */
    enum {
        ps_encoding_gzip = 0,
        ps_encoding_compress = 1,
        ps_encoding_deflate = 2,
        ps_encoding_identity = 3,
        ps_encoding_x,
        ps_init,
        ps_qvalue,
    };

    string_t strs[] = {
        string_newstr("gzip"),
        string_newstr("compress"),
        string_newstr("deflate"),
        string_newstr("identity"),
    };
    uint8_t encods[] = {HTTP_GZIP, HTTP_COMPRESS, HTTP_DEFLATE, HTTP_IDENTITY, };

    unsigned int ret = HTTP_IDENTITY;

    char const *pos = str;
    char const *p;
    int state = ps_init;
    for (p = str; p < end; p++) {
        switch (state) {
        case ps_init:
            if (isspace(*p))
                break;
            if ('x' == *p) {
                state = ps_encoding_x;
                break;
            }
            if ('g' == *p) {
                pos = p;
                state = ps_encoding_gzip;
                break;
            }
            if ('c' == *p) {
                pos = p;
                state = ps_encoding_compress;
                break;
            }
            if ('d' == *p) {
                pos = p;
                state = ps_encoding_deflate;
                break;
            }
            if ('i' == *p) {
                pos = p;
                state = ps_encoding_identity;
                break;
            }
            return HTTP_IDENTITY;
            break;
        case ps_encoding_x:
            if ('-' == *p) {
                state = ps_init;
                break;
            }
            return HTTP_IDENTITY;
            break;
        case ps_encoding_gzip:
        case ps_encoding_compress:
        case ps_encoding_deflate:
        case ps_encoding_identity:
            if (isalpha(*p))
                break;
            if (',' == *p || ';' == *p) {
                if (!strncasecmp(pos, strs[state].str, strs[state].len))
                    ret |= encods[state];
                state = (','==*p)? ps_init: ps_qvalue;
                break;
            }
            return HTTP_IDENTITY;
            break;
        case ps_qvalue:
            if (',' == *p)
                state = ps_init;
            break;
        default:
            BUG_ON("parse language unkown state");
            break;
        }
    }
    if (!strncasecmp(pos, strs[state].str, strs[state].len))
        ret |= encods[state];

    return ret;
}

static int http_parse_language(char const *str, char const *end)
{
    return YHTTP_AGAIN;
}
static int http_parse_host_and_port(http_request_t *r, char const *start, char const *end)
{
    char const *p;
    for (p = start; p < end; p++)
        if (':' == *p)
            break;
    if (p >= end) {
        r->hdr_req.host.str = start;
        r->hdr_req.host.len = end-start;
    } else {
        int port = atoi(p+1);
        if (0 >= port)
            return YHTTP_ERROR;
        r->hdr_req.port = port;
        r->hdr_req.host.str = start;
        r->hdr_req.host.len = p-start;
    }
    return YHTTP_OK;
}

static int http_parse_range(http_request_t *r, char const *start, char const *end)
{
    /* Only support 'bytes=xxx-xxx' */
    while (start < end && '=' != *start)
        start++;
    if (start == end) {
        r->hdr_res.code = HTTP_400; /* Bad Rquest */
        return YHTTP_ERROR;
    }
    start++;
    if ('-' == *start) {
        if (1 == sscanf(start, "%d", &r->hdr_req.range1))
            return YHTTP_AGAIN;
        r->hdr_res.code = HTTP_400;
        return YHTTP_ERROR;
    }
    if (isdigit(*start)) {
        int n = sscanf(start, "%d-%d", &r->hdr_req.range1, &r->hdr_req.range2);
        return (0 == n)? YHTTP_ERROR: YHTTP_AGAIN;
    }
    return YHTTP_ERROR;
}

static int http_parse_key_of_hdr(http_request_t *r, char *start, char *end)
{
    int len = end-start;
    string_tolower(start, len);
    yhttp_debug2("key: %.*s\n", len, start);
    void *x = hash_getk(ENV.hdr_fields, start, len);
    if (NULL == x) {
        yhttp_debug2("don't header key: %.*s\n", len, start);
        return YHTTP_ERROR;
    }
    r->hdr_type = (uint8_t)((intptr_t)x&0xff);
    return YHTTP_AGAIN;
}

static int http_parse_value_of_hdr(http_request_t *r, char const *start, char const *end)
{
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    int len = end-start;
    yhttp_debug2("value: %.*s\n", len, start);
    switch (r->hdr_type) {
    case HDR_CACHE_CONTROL:
        break;
    case HDR_CONNECTION:
        com->connection = (0 != strncasecmp("close", start, 5));
        break;
    case HDR_DATE:
        break;
    case HDR_PRAGMA:
        if (0 == strncasecmp("no-cache", start, SSTR_LEN("no-cache")))
            com->pragma = HTTP_PRAGMA_NO_CACHE;
        else
            com->pragma = HTTP_PRAGMA_UNSET;
        break;
    case HDR_TRAILER:
        break;
    case HDR_TRANSFER_ENCODING:
        if (0 == strncmp("chunked", start, SSTR_LEN("chunked"))) {
            com->transfer_encoding = HTTP_CHUNKED;
            break;
        }
        return YHTTP_ERROR;
        break;
    case HDR_UPGRADE:
        break;
    case HDR_VIA:
        break;
    case HDR_WARNING:
        break;

    case HDR_ALLOW:
        break;
    case HDR_CONTENT_ENCODING:
        break;
    case HDR_CONTENT_LENGTH:
        com->content_length = atoi(start);
        break;
    case HDR_CONTENT_LANGUAGE:
        break;
    case HDR_CONTENT_MD5:
        break;
    case HDR_CONTENT_RANGE:
        break;
    case HDR_CONTENT_TYPE:
        com->content_type = http_mime_to_digit(start, end);
        break;
    case HDR_EXPIRES:
        return http_parse_time_gmt(&com->expires, start, end);
        break;
    case HDR_LAST_MODIFIED:
        break;

    case HDR_ACCEPT:
        break;
    case HDR_ACCEPT_CHARSET:
        break;
    case HDR_ACCEPT_ENCODING:
        req->accept_encoding = http_parse_encoding(start, end);
        break;
    case HDR_ACCEPT_LANGUAGE:
        req->accept_language = http_parse_language(start, end);
        break;
    case HDR_AUTHORIZATION:
        break;
    case HDR_EXPECT:
        break;
    case HDR_FROM:
        req->from.str = start;
        req->from.len = len;
        break;
    case HDR_HOST:
        return http_parse_host_and_port(r, start, end);
        break;
    case HDR_IF_MATCH:
        break;
    case HDR_IF_MODIFIED_SINCE:
        return http_parse_time_gmt(&req->if_modified_since, start, end);
        break;
    case HDR_IF_NONE_MATCH:
        /* Parse Etag */
        req->if_none_match.str = start;
        req->if_none_match.len = len;
        break;
    case HDR_IF_RANGE:
        break;
    case HDR_IF_UNMODIFIED_SINCE:
        return http_parse_time_gmt(&req->if_unmodified_since, start, end);
        break;
    case HDR_MAX_FOREARDS:
        break;
    case HDR_PROXY_AUTHORICATION:
        break;
    case HDR_RANGE:
        return http_parse_range(r, start, end);
        break;
    case HDR_REFERER:
        req->referer.str = start;
        req->referer.len = len;
        break;
    case HDR_TE:
        break;
    case HDR_USER_AGENT:
        req->user_agent.str = start;
        req->user_agent.len = len;
        break;
    case HDR_COOKIE:
        req->cookie.str = start;
        req->cookie.len = len;
        break;

    case HDR_UPGRADE_INSECURE_REQUESTS:
        break;
    case HDR_KEEP_ALIVE:
        break;
    case HDR_ORIGIN:
        break;
    default:
        BUG_ON("http parsing header field unkown type");
        return YHTTP_ERROR;
    }
    return YHTTP_AGAIN;
}

/**
 * Return: YHTTP_AGAIN
 *         YHTTP_ERROR
 *         YHTTP_OK
 */
extern int http_parse_request_head(http_request_t *r, char *start, char *end)
{
    enum {
        PS_START = HTTP_PARSE_INIT,
        PS_METHOD,
        PS_URI_BF,              PS_URI_IN,
        PS_URI_SCHEME_IN,
        PS_URI_SCHEME_SLASH1,   PS_URI_SCHEME_SLASH2,
        PS_URI_HOST_BF,         PS_URI_HOST_IN,
        PS_URI_PORT_BF,         PS_URI_PORT_IN,
        PS_URI_PARAMS_BF,       PS_URI_PARAMS_IN,
        PS_URI_QUERY_BF,        PS_URI_QUERY_IN,
        PS_VERSION_BF,          PS_VERSION_IN,

        PS_HEADER_BF,
        PS_HEADER_KEY_IN,
        PS_HEADER_VALUE_BF,     PS_HEADER_VALUE_IN,

        PS_ALMOST_END,
        PS_END,
    };

    struct http_head_com *com = &r->hdr_com;
    struct http_head_req *req = &r->hdr_req;
    struct http_head_res *res = &r->hdr_res;
    char *pos;
    char *p;

#define return_error(c)   do { \
    r->parse_pos = pos; \
    r->parse_p = p; \
    res->code = (c); \
    return YHTTP_ERROR; \
} while (0)

    pos = r->parse_p? r->parse_pos: start;
    for (p = start; p < end; p++) {
        switch (r->parse_state) {
        case PS_START:
            if (CR == *p || LF == *p)
                break;
            if (isupper(*p)) {
                pos = p;
                r->parse_state = PS_METHOD;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_METHOD:
            /**
             * HTTP/1.0 GET PUT HEAD POST [DELETE LINK UNLINK]
             * HTTP/1.1 OPTIONS GET HEAD POST PUT DELETE TRACE CONNECT
             */
            if (isupper(*p))
                break;
            if (' ' == *p) {
                switch (p-pos) {
                case 3: /* GET, PUT */
                    if (!strncmp("GET", pos, 3))
                        req->method = HTTP_GET;
                    else if (!strncmp("PUT", pos, 3))
                        req->method = HTTP_PUT;
                    else
                        return_error(HTTP_400);
                    break;
                case 4: /* POST, HEAD */
                    if (!strncmp("POST", pos, 4))
                        req->method = HTTP_POST;
                    else if (!strncmp("HEAD", pos, 4))
                        req->method = HTTP_HEAD;
                    else if (!strncmp("LINK", pos, 4)) {
                        req->method = HTTP_LINK;
                        return_error(HTTP_501);     /* Not Implemented */
                    } else
                        return_error(HTTP_400);
                    break;
                case 5:
                    if (!strncmp("TRACE", pos, 5))
                        req->method = HTTP_TRACE;
                    else
                        return_error(HTTP_400);
                    break;
                case 6:
                    if (!strncmp("DELETE", pos, 6))
                        req->method = HTTP_DELETE;
                    else if (!strncmp("UNLINK", pos, 6)) {
                        req->method = HTTP_UNLINK;
                        return_error(HTTP_501);
                    } else
                        return_error(HTTP_400);
                    break;
                case 7:
                    if (!strncmp("OPTIONS", pos, 7))
                        req->method = HTTP_OPTIONS;
                    else if (!strncmp("CONNECT", pos, 7)) {
                        req->method = HTTP_CONNECT;
                        return_error(HTTP_501);
                    } else
                        return_error(HTTP_400);
                    break;

                default:
                    return_error(HTTP_400);
                }
                r->parse_state = PS_URI_BF;
                break;
            }

            return_error(HTTP_400);
            break;

        case PS_URI_BF:
            pos = p;
            /* OPTIONS * HTTP/1.1 */
            r->parse_state = ('/' == *p || '*' == *p)? PS_URI_IN: PS_URI_SCHEME_IN;
            break;

        case PS_URI_SCHEME_IN:
            /* "http" only */
            if ('h' == *p || 't' == *p || 'p' == *p)
                break;
            if (':' == *p) {
                if (!strncmp("http", pos, p-pos))
                    r->parse_state = PS_URI_SCHEME_SLASH1;
                else
                    return_error(HTTP_400);
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_SCHEME_SLASH1:
            if ('/' == *p) {
                r->parse_state = PS_URI_SCHEME_SLASH2;
                break;
            }
            return_error(HTTP_400);
            break;
        case PS_URI_SCHEME_SLASH2:
            if ('/' == *p) {
                r->parse_state = PS_URI_HOST_BF;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_HOST_BF:
            /* Ref: https://tools.ietf.org/html/rfc952 */
            if (isalnum(*p) || '-' == *p || '.' == *p) {
                pos = p;
                r->parse_state = PS_URI_HOST_IN;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_HOST_IN:
            if (isalnum(*p) || '-' == *p || '.' == *p)
                break;
            if ('/' == *p || ':' == *p) {
                req->host.str = pos;
                req->host.len = p-pos;
                r->parse_state = (':' == *p)? PS_URI_PORT_BF: PS_URI_BF;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_PORT_BF:
            if (isdigit(*p)) {
                pos = p;
                r->parse_state = PS_URI_PORT_IN;
                break;
            }
            if ('/' == *p) {
                /* the uri, http://foo.com:/xxx, is valid */
                req->port = 80;
                pos = p;
                r->parse_state = PS_URI_IN;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_PORT_IN:
            if (isdigit(*p))
                break;
            if ('/' == *p) {
                /* http://foo.com:8080/xxx */
                int port = atoi(pos);
                if (port == 0)
                    return_error(HTTP_400);
                req->port = port;
                pos = p;
                r->parse_state = PS_URI_IN;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_IN:
            if (isprint(*p) && '?' != *p && ';' != *p && !isspace(*p))
                break;
            if (' ' == *p || '?' == *p || ';' == *p) {
                req->uri.str = pos;
                req->uri.len = p-pos;

                switch (*p) {
                case ' ':
                    r->parse_state = PS_VERSION_BF;
                    break;
                case '?':
                    r->parse_state = PS_URI_QUERY_BF;
                    break;
                case ';':
                    r->parse_state = PS_URI_PARAMS_BF;
                    break;
                }
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_PARAMS_BF:
            if (isprint(*p) && '?' != *p && !isspace(*p)) {
                pos = p;
                r->parse_state = PS_URI_PARAMS_IN;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_PARAMS_IN:
            if (isprint(*p) && '?' != *p && !isspace(*p))
                break;
            if ('?' == *p || ' ' == *p) {
                req->uri_params.str = pos;
                req->uri_params.len = p-pos;
                r->parse_state = ('?' == *p)? PS_URI_PARAMS_IN: PS_VERSION_BF;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_QUERY_BF:
            if (isprint(*p) && !isspace(*p) && !isdigit(*p)) {
                pos = p;
                r->parse_state = PS_URI_QUERY_IN;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_URI_QUERY_IN:
            if (isprint(*p) && !isspace(*p))
                break;
            if (' ' == *p) {
                req->query.str = pos;
                req->query.len = p-pos;
                r->parse_state = PS_VERSION_BF;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_VERSION_BF:
            if ('H' != *p)
                return_error(HTTP_400);
            pos = p;
            r->parse_state = PS_VERSION_IN;
            break;

        case PS_VERSION_IN:
            if (CR == *p) {
                if ((8 != p-pos) || (0 != strncmp("HTTP/", pos, 5)))
                    return_error(HTTP_400);
                pos += 5;
                if (!strncmp("0.9", pos, 3))
                    com->version = HTTP09;
                else if (!strncmp("1.0", pos, 3)) {
                    com->version = HTTP10;
                    com->connection = 0;    /* A connection is a request */
                } else if (!strncmp("1.1", pos, 3)) {
                    com->version = HTTP11;
                    com->connection = 1;     /* Default is keep-alive */
                } else {
                    return_error(HTTP_400);
                }
            } else if (LF == *p) {
                r->parse_state = PS_HEADER_BF;
            }
            break;

        case PS_HEADER_BF:
            if (isalpha(*p) || '-' == *p) {
                pos = p;
                r->parse_state = PS_HEADER_KEY_IN;
                break;
            }
            if (CR == *p) {
                r->parse_state = PS_ALMOST_END;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_HEADER_KEY_IN:
            if (isalpha(*p) || '-' == *p || '_' == *p)
                break;
            if (':' == *p) {
                int s = http_parse_key_of_hdr(r, pos, p);
                if (YHTTP_ERROR == s)
                    return_error(HTTP_400);
                r->parse_state = PS_HEADER_VALUE_BF;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_HEADER_VALUE_BF:
            if (' ' == *p)
                break;
            if (isprint(*p) && !isspace(*p)) {
                pos = p;
                r->parse_state = PS_HEADER_VALUE_IN;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_HEADER_VALUE_IN:
            if (isprint(*p) && (CR != *p || LF == *p))
                break;
            if (CR == *p) {
                int s = http_parse_value_of_hdr(r, pos, p);
                if (YHTTP_ERROR == s)
                    return_error(HTTP_400);
                r->parse_state = PS_ALMOST_END;
                break;
            }
            return_error(HTTP_400);
            break;

        case PS_ALMOST_END:
            if (LF == *p)
                break;
            if (isalpha(*p) || '-' == *p || '_' == *p) {
                pos = p;
                r->parse_state = PS_HEADER_KEY_IN;
                break;
            }
            if (CR == *p) {
                r->parse_state = PS_END;
                break;
            }

            return_error(HTTP_400);
            break;

        case PS_END:
            if (LF == *p) {
                r->parse_pos = pos;
                r->parse_p = p+1;
                res->code = HTTP_200;
                return YHTTP_OK;
            }
            return_error(HTTP_400);
            break;

        default:
            BUG_ON("http parsing unkown state");
            break;
        }
    }

    r->parse_pos = pos;
    r->parse_p = end;
    return YHTTP_AGAIN;
}

extern int http_parse_init()
{
    ENV.hdr_fields = hash_create(20, NULL, NULL);
    if (!ENV.hdr_fields)
        return YHTTP_ERROR;
    for (size_t i = 0; i < ARRSIZE(hdr_keys); i++) {
        intptr_t id = hdr_keys[i].id;
        hash_add(ENV.hdr_fields, hdr_keys[i].key.str, hdr_keys[i].key.len, (void*)id);
        BUG_ON((void*)id != hash_getk(ENV.hdr_fields, hdr_keys[i].key.str, hdr_keys[i].key.len));
    }
    return YHTTP_OK;
}

extern void http_parse_destroy()
{
    hash_destroy(&ENV.hdr_fields);
}

extern void http_parse_file_suffix(http_request_t *r, char const *start, char const *end)
{
    struct http_head_req *req = &r->hdr_req;
    req->suffix[0] = '\0';
    req->suffix_len = 0;
    if (start < end-SUFFIX_LEN)
        start = end-SUFFIX_LEN;
    for (; start < end; start++) {
        if ('.' == *start) {
            while (++start < end)
                req->suffix[req->suffix_len++] = *start;
            return;
        }
    }
}


