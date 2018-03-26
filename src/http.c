#include <stdlib.h>
#include <time.h>

#include "log.h"
#include "memory.h"
#include "buffer.h"
#include "http.h"
#include "http_parser.h"
#include "http_cache.h"
#include "http_page.h"
#include "http_error_page.h"
#include "http_time.h"

extern int http_init()
{
    http_error_page_init(NULL);

    return YHTTP_OK;
}

extern void http_destroy()
{
    http_error_page_destroy();
}

extern void http_request_reuse(http_request_t *req)
{
    req->parse_pos = req->request_head->pos;
    req->parse_state = HTTP_PARSE_INIT;
}

extern http_request_t *http_request_malloc()
{
    http_request_t *req = NULL;
    req = yhttp_malloc(sizeof(*req));
    if (!req)
        return req;
    req->request_head = buffer_malloc(HTTP_BUFFER_SIZE);
    if (!req->request_head)
        goto req_head_err;
    req->response_buffer = buffer_malloc(HTTP_BUFFER_SIZE);
    if (!req->response_buffer)
        goto req_head_err;

    return req;

req_head_err:
    buffer_free(req->response_buffer);
    buffer_free(req->request_head);
    yhttp_free(req);
    return NULL;
}

extern void http_request_free(http_request_t *req)
{
    BUG_ON(NULL == req);

    buffer_free(req->request_head);
    buffer_free(req->response_buffer);
    yhttp_free(req);
}

extern int http_check_request(http_request_t *r)
{
    /* 304*/
    struct http_head_com *com = &r->com;
    struct http_head_req *req = &r->req;
    struct http_head_res *res = &r->res;

    if (HTTP09 == com->version && HTTP_GET != req->method) {
        res->code = HTTP_400;
        return YHTTP_ERROR;
    }

    /* NOTE http/1.0 dont support media type and isn't Host */
    if (HTTP10 == com->version) {
        com->transfer_encoding  = HTTP_UNCHUNKED;
    }

    com->content_encoding = HTTP_IDENTITY;

    if (HTTP11 == com->version) {
        com->transfer_encoding  = HTTP_CHUNKED;
    }

    return YHTTP_OK;
}

extern int http_build_response_head(http_request_t *r)
{
    static char const *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static char const *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    struct http_head_com *com = &r->com;
    struct http_head_res *res = &r->res;
    buffer_t *resb = r->response_buffer;
    http_error_page_t const *page = http_error_page_get(res->code);
    string_t const *reason = &page->reason;
    int len;
    struct tm *gmt;

    BUG_ON(NULL == page);
    if (HTTP09 == com->version)
        return 0;

    len = snprintf(resb->last, buffer_rest(resb), "HTTP/1.%c %.*s"CRLF,
            (HTTP10 == com->version)? '0': '1', reason->len, reason->str);
    resb->last += len;

    /* Date */
    len = snprintf(resb->last, buffer_rest(resb), "Date: %s"CRLF,
            current_http_time);
    resb->last += len;

    /* Pragma */
    if (com->pragma == HTTP_PRAGMA_NO_CACHE) {
        len = SSTR_LEN("Pragma: no-cache"CRLF);
        len = MIN(len, buffer_rest(resb));
        memcpy(resb->last, "Pragma: no-cache"CRLF, len);
        resb->last += len;
    }

    /* Content Encoding: gzip, compress, deflate, (identity) */
    switch (com->content_encoding) {
    case HTTP_GZIP:
        len = SSTR_LEN("Content-Encoding: gzip"CRLF);
        len = MIN(len, buffer_rest(resb));
        memcpy(resb->last, "Content-Encoding: gzip"CRLF, len);
        resb->last += len;
        break;
    case HTTP_COMPRESS:
        len = SSTR_LEN("Content-Encoding: compress"CRLF);
        len = MIN(len, buffer_rest(resb));
        memcpy(resb->last, "Content-Encoding: compress"CRLF, len);
        resb->last += len;
        break;
    case HTTP_DEFLATE:
        len = SSTR_LEN("Content-Encoding: deflate"CRLF);
        len = MIN(len, buffer_rest(resb));
        memcpy(resb->last, "Content-Encoding: deflate"CRLF, len);
        resb->last += len;
        break;
    case HTTP_IDENTITY:
        break;
    }

    /* Expires is ignored */

    /* Server */
    len = snprintf(resb->last, buffer_rest(resb), "Server: yhttpd/"VER CRLF);
    resb->last += len;

    /* Content Type */
    len = snprintf(resb->last, buffer_rest(resb), "Content-Type: %.*s"CRLF,
            com->content_type.len, com->content_type.str);
    resb->last += len;

    if (HTTP11 == com->version) {
        /* Content Length is not MUST by response, but MUST by resquest in HTTP/1.0 */
        if (com->transfer_encoding == HTTP_CHUNKED) {
            len = snprintf(resb->last, buffer_rest(resb), "Transfer-Encoding: chuncked"CRLF);
        } else {
            len = snprintf(resb->last, buffer_rest(resb), "Content-Length: %d"CRLF,
                    com->content_length);
        }
        resb->last += len;

        if (com->connection) {
            len = snprintf(resb->last, buffer_rest(resb), "Connection: Keep-Alive"CRLF);
        } else {
            len = snprintf(resb->last, buffer_rest(resb), "Connection: Close"CRLF);
        }
        resb->last += len;
    }

    if (res->code == HTTP_200) {
        /* Last Modified */
        gmt = gmtime(&com->last_modified);
        len = snprintf(resb->last, buffer_rest(resb), "Last-Modified: %s, %02d %s %4d %02d:%02d:%02d GMT"CRLF,
                week[gmt->tm_wday], gmt->tm_mday,
                months[gmt->tm_mon - 1], gmt->tm_year+1900,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
        resb->last += len;
    }

    /* Location 3xx */
    if (res->code/100 == 3) {
        len = snprintf(resb->last, buffer_rest(resb), "Location: %.*s"CRLF,
                res->location.len, res->location.str);
        resb->last += len;
    }

    *resb->last++ = CR;
    *resb->last++ = LF;
    yhttp_debug2("response header\n%s\n", resb->pos, buffer_len(resb));

    return 0;
}
