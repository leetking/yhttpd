#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>    /* for strncasecmp */

#include "common.h"
#include "http.h"

/* #define LOCAL_TEST */

#ifdef LOCAL_TEST
# define _D(...) printf(__VA_ARGS__)
#else
# define _D(...) ((void)0)
#endif /* LOCAL_TEST */

extern struct http_head *http_head_malloc(size_t metadatalen)
{
    struct http_head *ret;
    ret = malloc(sizeof(*ret)+metadatalen);
    if (!ret) return NULL;
    ret->_ps_status = HTTP_PARSE_INIT;
    ret->_metadataidx = 0;
    ret->_metadatalen = metadatalen;
    return ret;
}
extern void http_head_free(struct http_head **req)
{
    if (!req || !*req) return;
    free(*req);
    *req = NULL;
}

/**
 * return: 1: parse continue
 *         0: parse finish or error, must interrupt parse and close fd
 */
extern int http_head_parse(struct http_head *req, uint8_t const *buff, ssize_t len)
{
    /* 解析请求 */
    enum {
        PS_START = HTTP_PARSE_INIT,
        PS_METHOD,
        PS_URI_BF,
        PS_URI_IN,
        PS_VERSION_BF,
        PS_VERSION_IN,

        PS_LINE_BF,
        PS_KEY_IN,
        PS_VALUE_BF,
        PS_VALUE_IN,
    };

    enum {
        HTTP_LINE_EXT_CON  = HTTP_LINE_MAX,
        HTTP_LINE_EXT_RANGE,
        HTTP_LINE_EXT_CONT_LEN,

        HTTP_LINE_EXT_IGN,
    };

    uint8_t const *req_start = buff;
    uint8_t const *end = buff+len;
    for (uint8_t const *p = buff; p < end; p++) {
        switch (req->_ps_status) {
        case PS_START:
            if (CR == *p || LF == *p)
                break;
            if (!isupper(*p)) {
                req->status_code = HTTP_400;
                req->_ps_status = HTTP_PARSE_INIT;
                return 0;
            }
            req->_ps_status = PS_METHOD;
            req_start = p;
            break;

        case PS_METHOD:
            if (' ' == *p) {
                switch (p-req_start) {
                case 3: /* GET, PUT */
                    if (!strncmp("GET", (char*)req_start, 3))
                        req->method = HTTP_GET;
                    else if (!strncmp("PUT", (char*)req_start, 3))
                        req->method = HTTP_PUT;
                    else {
                        req->status_code = HTTP_501;
                        req->_ps_status = HTTP_PARSE_INIT;
                        return 0;
                    }
                    break;
                case 4: /* POST, HEAD */
                    if (!strncmp("POST", (char*)req_start, 4))
                        req->method = HTTP_POST;
                    else if (!strncmp("HEAD", (char*)req_start, 4))
                        req->method = HTTP_HEAD;
                    else {
                        req->status_code = HTTP_501;
                        req->_ps_status = HTTP_PARSE_INIT;
                        return 0;
                    }
                    break;

                default:
                    req->status_code = HTTP_501;
                    req->_ps_status = HTTP_PARSE_INIT;
                    return 0;
                }
                req->_ps_status = PS_URI_BF;
            } else if (!isupper(*p)) {
                req->status_code = HTTP_400;
                req->_ps_status = HTTP_PARSE_INIT;
                return 0;
            }
            break;

        case PS_URI_BF:
            if (' ' == *p)
                break;
            if ('/' != *p) {
                req->status_code = HTTP_400;
                req->_ps_status = HTTP_PARSE_INIT;
                return 0;
            }
            req_start = p;
            req->_ps_status = PS_URI_IN;
            break;

        case PS_URI_IN:
            if (' ' == *p) {
                int rest = req->_metadatalen - req->_metadataidx;
                int len = MIN(p - req_start, rest);
                if (len <= 0) {
                    /* to long! */
                    req->status_code = HTTP_413;
                    req->_ps_status = HTTP_PARSE_INIT;
                    return 0;
                }
                req->lines[HTTP_URI].str = (char*)req->_metadata + req->_metadataidx;
                req->lines[HTTP_URI].len = len;
                memcpy(req->_metadata + req->_metadataidx, req_start, len);
                req->_metadataidx += len;

                req->_ps_status = PS_VERSION_BF;
            }
            /* TODO http parse: parse uriencode and argument! */
            break;

        case PS_VERSION_BF:
            if (' ' == *p)
                break;
            if ('H' != *p) {
                req->status_code = HTTP_400;
                req->_ps_status = HTTP_PARSE_INIT;
                return 0;
            }
            req_start = p;
            req->_ps_status = PS_VERSION_IN;
            break;

        case PS_VERSION_IN:
            if (CR == *p) {
                if ((8 != (p-req_start)) || (0 != strncmp("HTTP/", (char*)req_start, 5))) {
                    req->status_code = HTTP_400;
                    req->_ps_status = HTTP_PARSE_INIT;
                    return 0;
                }
                req_start = req_start+5;
                if (!strncmp("0.9", (char*)req_start, 3))
                    req->ver = HTTP09;
                else if (!strncmp("1.0", (char*)req_start, 3))
                    req->ver = HTTP10;
                else if (!strncmp("1.1", (char*)req_start, 3)) {
                    req->ver = HTTP11;
                    req->iscon = 1;     /* default is keep-alive */
                } else {
                    req->status_code = HTTP_400;
                    req->_ps_status = HTTP_PARSE_INIT;
                    return 0;
                }
            }
            if (LF == *p)
                req->_ps_status = PS_LINE_BF;
            break;

        case PS_LINE_BF:
            /* request end, dual CRLF */
            if (CR == *p) {
                req->_fin_lf = 1;
                break;
            }
            if (req->_fin_lf && LF == *p) {
                req->status_code = HTTP_200;
                req->_ps_status = HTTP_PARSE_INIT;
                return 0;
            }

            if (isalpha(*p) || '-' == *p || '_' == *p) {
                req_start = p;
                req->_ps_status = PS_KEY_IN;
            }
            break;

        case PS_KEY_IN:
            if (':' == *p) {
                const struct {
                    string_t key;
                    int type;
                } keys[] = {
                    { { "Host",             4,  }, HTTP_HOST },
                    { { "Connection",       10, }, HTTP_LINE_EXT_CON, },
                    { { "Referer",          7,  }, HTTP_REFERER, },
                    { { "User-Agent",       10, }, HTTP_UA,  },
                    { { "Accept",           6,  }, HTTP_ACCEPT, },
                    { { "Content-Length",   14, }, HTTP_LINE_EXT_CONT_LEN, },
                    { { "Accept-Encoding",  15, }, HTTP_ACCEPT_ENCODING, },
                    { { "Accept-Language",  15, }, HTTP_ACCEPT_LANGUAGE, },
                    { { "Range",            5,  }, HTTP_LINE_EXT_RANGE, },
                    { { "Cookie",           6,  }, HTTP_COOKIE, },

                    /* { { "", 0, }, HTTP_LINE_EXT_IGN, }, */
                };
                int len = p - req_start;
                _D("key: %.*s\t", len, req_start);
                int i;
                for (i = 0; i < (int)ARRSIZE(keys); i++) {
                    if ((keys[i].key.len == len) && !strncasecmp(keys[i].key.str, (char*)req_start, len)) {
                        req->_line_type = keys[i].type;
                        req->_ps_status = PS_VALUE_BF;
                        break;
                    }
                }

                /* ignore */
                if (i >= (int)ARRSIZE(keys)) {
                    req->_line_type = HTTP_LINE_EXT_IGN;
                    req->_ps_status = PS_VALUE_BF;
                }

            } else if (!(isalpha(*p) || '-' == *p || '_' == *p)) {
                req->status_code = HTTP_400;
                req->_ps_status = HTTP_PARSE_INIT;
                return 0;
            }
            break;

        case PS_VALUE_BF:
            if (' ' == *p)
                break;
            req->_ps_status = PS_VALUE_IN;
            req_start = p;
            break;

        case PS_VALUE_IN:
            if (CR == *p) {
                int rest, len;
                switch (req->_line_type) {
                case HTTP_LINE_EXT_RANGE:    /* TODO http.c parse range */
                    _D("value: HTTP_LINE_EXT_RANGE\n");

                    break;

                case HTTP_LINE_EXT_CON:
                    _D("value: HTTP_LINE_EXT_CON\n");
                    if ((5 == p-req_start) && !strncasecmp("close", (char*)req_start, 5))
                        req->iscon = 0;
                    break;

                case HTTP_LINE_EXT_CONT_LEN:
                    _D("value: HTTP_LINE_EXT_CONT_LEN\n");   /* TODO http.c parse content-length */

                    break;

                case HTTP_LINE_EXT_IGN:
                    _D("value: HTTP_LINE_EXT_IGN(%.*s)\n", (int)(p-req_start), req_start);
                    break;

                default:
                    rest = req->_metadatalen - req->_metadataidx;
                    len = MIN(p - req_start, rest);
                    _D("value: %.*s\n", len, req_start);
                    if (len <= 0) {
                        req->status_code = HTTP_413;
                        req->_ps_status = HTTP_PARSE_INIT;
                        return 0;    /* to long! */
                    }
                    req->lines[req->_line_type].str = (char*)req->_metadata + req->_metadataidx;
                    req->lines[req->_line_type].len = len;
                    memcpy(req->_metadata + req->_metadataidx, req_start, len);
                    req->_metadataidx += len;
                }

            } else if (LF == *p) {
                req->_ps_status = PS_LINE_BF;
            }
            break;
        }
    }

    /* parse continue */
    return 1;
}
