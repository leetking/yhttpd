#include <ctype.h>
#include <string.h>
#include <strings.h>    /* for strncasecmp */

#include "common.h"
#include "http.h"

extern int http_request_parse(struct http_request *req, uint8_t const *buff, ssize_t len)
{
    /* 解析请求 */
    enum {
        PS_START,
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
        LINE_RANGE,  /* Range */
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
                        req->method = HTTP_GET;
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
                req->lines[HTTP_URI].str = req->_metadata + req->_metadataidx;
                req->lines[HTTP_URI].len = len;
                strncpy((char*)req->_metadata + req->_metadataidx, (char*)req_start, len);
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
            if (CR == *p || LF == *p) {
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
                req->_ps_status = PS_Host_BF;
            }
            break;

        case PS_Host_BF:    /* keyword "Host" */
            if (CR == *p || LF == *p)
                break;
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
            if (CR == *p || LF == *p) {
                int rest = req->_metadatalen - req->_metadataidx;
                int len = MIN(p - req_start, rest);
                if (len <= 0)
                    return HTTP_REQ_INVALID;    /* to long! */
                req->lines[HTTP_HOST].str = req->_metadata + req->_metadataidx;
                req->lines[HTTP_HOST].len = len;
                strncpy((char*)req->_metadata + req->_metadataidx, (char*)req_start, len);
                req->_metadataidx += len;

                req->_ps_status = PS_LINE_BF;  /* parse normal line */
            }

            break;

        case PS_LINE_BF:
            /* TODO detect the end of head */
            if (CR == *p || LF == *p)
                break;
            if (isalpha(*p) || '-' == *p || '_' == *p) {
                req_start = p;
                req->_ps_status = PS_KEY_IN;
            }
            break;

        case PS_KEY_IN:
            if (':' == *p) {
                static char const *keys[] = {
                    "Connection",
                    "User-Agent",
                    "Accept",
                    "Accept-Encoding",
                    "Accept-Language",
                    "Range",

                    NULL,
                };
                static int const keylens[] = {
                    10, 10, 6, 15, 15, 5,
                };

                static int const line_types[] = {
                    LINE_CON,
                    LINE_UA,
                    LINE_ACCEPT,
                    LINE_ACPT_ENCODING,
                    LINE_ACPT_LANGUAGE,
                    LINE_RANGE,
                };
                int len = p - req_start;
                for (int i = 0; i < (int)ARRSIZE(keys); i++) {
                    if ((keylens[i] == len) && !strncasecmp(keys[i], (char*)req_start, len)) {
                        req->_line_type = line_types[i];
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
            if (CR == *p || LF == *p) {
                int rest = req->_metadatalen - req->_metadataidx;
                int len = MIN(p - req_start, rest);
                if (len <= 0)
                    return HTTP_REQ_INVALID;    /* to long! */
                strncpy((char*)req->_metadata + req->_metadataidx, (char*)req_start, len);
                req->lines[req->_line_type].str = req->_metadata + req->_metadataidx;
                req->lines[req->_line_type].len = len;

                req->_metadataidx += len;

                req->_ps_status = PS_LINE_BF;
            }
            break;
        }
    }

    return HTTP_REQ_CONTINUE;
}
