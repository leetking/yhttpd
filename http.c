#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>    /* for strncasecmp */

#include "common.h"
#include "http.h"

extern int http_request_parse(struct http_request *req, uint8_t const *buff, ssize_t len)
{
    /* 解析请求 */
    enum {
        PS_START = HTTP_PARSE_INIT,
        PS_METHOD,
        PS_URI_BF,
        PS_URI_IN,
        PS_VERSION_BF,
        PS_VERSION_IN,
        PS_Host_BF,
        PS_Host_IN,
        PS_host_bf,
        PS_host_in,

        PS_LINE_BF,
        PS_KEY_IN,
        PS_VALUE_BF,
        PS_VALUE_IN,
    };
    enum {
        LINE_CON,    /* Connection */
        LINE_UA,     /* User-Agent */
        LINE_ACCEPT, /* Accept */
        LINE_ACPT_ENCODING, /* Accept-Encoding */
        LINE_ACPT_LANGUAGE, /* Accept-Language */
        LINE_RANGE,         /* Range */
        LINE_REFERER,       /* Referer */
        LINE_COOKIE,        /* Cookie */
        /* TODO request type */
    };

    uint8_t const *req_start = buff;
    uint8_t const *end = buff+len;
    for (uint8_t const *p = buff; p < end; p++) {
        switch (req->_ps_status) {
        case PS_START:
            if (CR == *p || LF == *p)
                break;
            if (!isupper(*p))
                return HTTP_REQ_INVALID;
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
                    else
                        return HTTP_REQ_INVALID;
                    break;
                case 4: /* POST, HEAD */
                    if (!strncmp("POST", (char*)req_start, 4))
                        req->method = HTTP_POST;
                    else if (!strncmp("HEAD", (char*)req_start, 4))
                        req->method = HTTP_HEAD;
                    else
                        return HTTP_REQ_INVALID;
                    break;

                default:
                    return HTTP_REQ_INVALID;
                }
                req->_ps_status = PS_URI_BF;
            }
            if (!isupper(*p))
                return HTTP_REQ_INVALID;
            break;

        case PS_URI_BF:
            if (' ' == *p)
                break;
            if ('/' != *p)
                return HTTP_REQ_INVALID;
            req_start = p;
            req->_ps_status = PS_URI_IN;
            break;

        case PS_URI_IN:
            if (' ' == *p) {
                int rest = req->_metadatalen - req->_metadataidx;
                int len = MIN(p - req_start, rest);
                if (len <= 0)
                    return HTTP_REQ_INVALID;    /* to long! */
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
            if ('H' != *p)
                return HTTP_REQ_INVALID;
            req_start = p;
            req->_ps_status = PS_VERSION_IN;
            break;

        case PS_VERSION_IN:
            if (CR == *p) {
                if ((8 != (p-req_start)) || (0 != strncmp("HTTP/", (char*)req_start, 5)))
                    return HTTP_REQ_INVALID;
                req_start = req_start+5;
                if (!strncmp("0.9", (char*)req_start, 3))
                    req->ver = HTTP09;
                else if (!strncmp("1.0", (char*)req_start, 3))
                    req->ver = HTTP10;
                else if (!strncmp("1.1", (char*)req_start, 3)) {
                    req->ver = HTTP11;
                    req->iscon = 1;     /* default is keep-alive */
                } else
                    return HTTP_REQ_INVALID;
            }
            if (LF == *p)
                req->_ps_status = PS_Host_BF;
            break;

        case PS_Host_BF:    /* keyword "Host" */
            if ('h' != tolower(*p))
                return HTTP_REQ_INVALID;
            req_start = p;
            req->_ps_status = PS_Host_IN;
            break;

        case PS_Host_IN:
            if (':' == *p) {
                if ((4 == p - req_start) && (!strncasecmp("Host", (char*)req_start, 4)))
                    req->_ps_status = PS_host_bf;
                else
                    return HTTP_REQ_INVALID;
            }
            break;

        case PS_host_bf:    /* The real host */
            if (' ' == *p)
                break;
            req->_ps_status = PS_host_in;
            req_start = p;
            break;

        case PS_host_in:
            if (CR == *p) {
                int rest = req->_metadatalen - req->_metadataidx;
                int len = MIN(p - req_start, rest);
                if (len <= 0)
                    return HTTP_REQ_INVALID;    /* to long! */
                req->lines[HTTP_HOST].str = (char*)req->_metadata + req->_metadataidx;
                req->lines[HTTP_HOST].len = len;
                memcpy(req->_metadata + req->_metadataidx, req_start, len);
                req->_metadataidx += len;
            }
            if (LF == *p)
                req->_ps_status = PS_LINE_BF;  /* parse normal line */

            break;

        case PS_LINE_BF:
            /* request end, dual CRLF */
            if (CR == *p) {
                req->_fin_lf = 1;
                break;
            }
            if (req->_fin_lf && LF == *p)
                return HTTP_REQ_FINISH;

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
                    { { "Connection",       10, }, LINE_CON, },
                    { { "Referer",          7,  }, LINE_REFERER, },
                    { { "User-Agent",       10, }, LINE_UA,  },
                    { { "Accept",           6,  }, LINE_ACCEPT, },
                    { { "Accept-Encoding",  15, }, LINE_ACPT_ENCODING, },
                    { { "Accept-Language",  15, }, LINE_ACPT_LANGUAGE, },
                    { { "Range",            5,  }, LINE_RANGE, },
                    { { "Cookie",           6,  }, LINE_COOKIE, },
                };
                int len = p - req_start;
                for (int i = 0; i < (int)ARRSIZE(keys); i++) {
                    if ((keys[i].key.len == len) && !strncasecmp(keys[i].key.str, (char*)req_start, len)) {
                        req->_line_type = keys[i].type;
                        req->_ps_status = PS_VALUE_BF;
                        break;
                    }
                }
            }
            if (!(isalpha(*p) || '-' == *p || '_' == *p))
                return HTTP_REQ_INVALID;
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
                case LINE_RANGE:    /* TODO http.c parse range */

                    break;

                case LINE_CON:
                    if ((5 == p-req_start) && !strncasecmp("close", (char*)req_start, 5))
                        req->iscon = 0;
                    break;

                default:
                    rest = req->_metadatalen - req->_metadataidx;
                    len = MIN(p - req_start, rest);
                    if (len <= 0)
                        return HTTP_REQ_INVALID;    /* to long! */
                    req->lines[req->_line_type].str = (char*)req->_metadata + req->_metadataidx;
                    req->lines[req->_line_type].len = len;
                    memcpy(req->_metadata + req->_metadataidx, req_start, len);
                    req->_metadataidx += len;
                }
            }
            if (LF == *p)
                req->_ps_status = PS_LINE_BF;
            break;
        }
    }

    return HTTP_REQ_CONTINUE;
}