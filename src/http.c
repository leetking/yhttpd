#include <stdlib.h>
#include <time.h>

#include "log.h"
#include "memory.h"
#include "buffer.h"
#include "http.h"
#include "http_parser.h"
#include "http_page.h"
#include "http_error_page.h"
#include "http_time.h"
#include "http_mime.h"

extern int http_init()
{
    if (YHTTP_OK != http_error_page_init(NULL))
        goto error_page_err;
    if (YHTTP_OK != http_parse_init())
        goto parse_init_err;
    if (YHTTP_OK != http_mime_init())
        goto mime_init_err;

    return YHTTP_OK;

mime_init_err:
    http_parse_destroy();
parse_init_err:
    http_error_page_destroy();
error_page_err:
    return YHTTP_ERROR;
}

extern void http_exit()
{
    http_error_page_destroy();
    http_parse_destroy();
}

extern void http_request_init(http_request_t *r)
{
    http_request_reuse(r);
    r->hdr_buffer_large = 0;
}

extern void http_request_reuse(http_request_t *r)
{
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    buffer_init(r->hdr_buffer);
    buffer_init(r->res_buffer);
    r->parse_p = r->hdr_buffer->pos;
    r->parse_state = HTTP_PARSE_INIT;
    req->if_modified_since = 0;     /* 1970-1-1 00:00:00 */
    req->range1 = 0;
    req->range2 = INT_MAX;          /* indicate infinity */
    req->if_none_match.str = NULL;
    req->if_none_match.len = 0;
    com->content_length = 0;
    com->content_encoding = HTTP_UNCHUNKED;
    res->server.str = "yhttpd/"VER;
    res->server.len = SSTR_LEN("yhttpd/"VER);
}

extern http_request_t *http_request_malloc()
{
    http_request_t *req = NULL;
    req = yhttp_malloc(sizeof(*req));
    if (!req)
        return req;
    req->hdr_buffer = buffer_malloc(YHTTP_BUFFER_SIZE_CFG);
    if (!req->hdr_buffer)
        goto req_head_err;
    req->res_buffer = buffer_malloc(YHTTP_BUFFER_SIZE_CFG);
    if (!req->res_buffer)
        goto req_head_err;

    return req;

req_head_err:
    buffer_free(req->res_buffer);
    buffer_free(req->hdr_buffer);
    yhttp_free(req);
    return NULL;
}

/* just free allocated by http_request_malloc */
extern void http_request_free(http_request_t *req)
{
    BUG_ON(NULL == req);

    buffer_free(req->hdr_buffer);
    buffer_free(req->res_buffer);
    yhttp_free(req);
}

/* free all buffer */
extern void http_request_destroy(http_request_t *r)
{
    buffer_free(r->hdr_buffer);
    buffer_free(r->res_buffer);
    buffer_free(r->ety_buffer);
    yhttp_free(r);
}

extern int http_check_request(http_request_t *r)
{
    /* 304*/
    struct http_head_com *com = &r->hdr_com;
    struct http_head_req *req = &r->hdr_req;
    struct http_head_res *res = &r->hdr_res;

    if (HTTP09 == com->version && HTTP_GET != req->method) {
        res->code = HTTP_400;
        return YHTTP_ERROR;
    }

    /* NOTE http/1.0 dont support media type and isn't Host */
    if (HTTP10 == com->version) {
        unsigned int support_method = HTTP_GET|HTTP_HEAD;
        if (!(support_method & req->method)) {
            res->code = HTTP_405;
            return YHTTP_ERROR;
        }
        return YHTTP_OK;
    }

    if (HTTP11 == com->version) {
        unsigned int support_method = HTTP_GET|HTTP_HEAD;
        if (!(support_method & req->method)) {
            res->code = HTTP_405;
            return YHTTP_ERROR;
        }
        return YHTTP_OK;
    }

    return YHTTP_ERROR;
}

extern int http_build_response_head(http_request_t *r)
{
    static char const *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static char const *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    buffer_t *resb = r->res_buffer;
    http_error_page_t const *page = http_error_page_get(res->code);
    string_t const *reason = &page->reason;
    int len;
    struct tm *gmt;
    string_t const *mime;

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
    len = snprintf(resb->last, buffer_rest(resb), "Server: %.*s"CRLF,
            res->server.len, res->server.str);
    resb->last += len;

    /* Content Type */
    mime = &http_mimes[com->content_type];
    len = snprintf(resb->last, buffer_rest(resb), "Content-Type: %.*s"CRLF,
            mime->len, mime->str);
    resb->last += len;

    if (HTTP11 == com->version) {
        /* Content Length is not MUST by response, but MUST by resquest in HTTP/1.0 */
        if (com->transfer_encoding == HTTP_CHUNKED) {
            len = snprintf(resb->last, buffer_rest(resb), "Transfer-Encoding: chunked"CRLF);
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

        /* Cache Control */
        switch (com->cache_control) {
        case HTTP_CACHE_CONTROL_PUBLIC:
            len = snprintf(resb->last, buffer_rest(resb), "Cache-Control: public"CRLF);
            break;
        case HTTP_CACHE_CONTROL_NO_CACHE:
            len = snprintf(resb->last, buffer_rest(resb), "Cache-Control: no-cache, max-age=%d"CRLF,
                    com->cache_control_max_age);
            break;
        case HTTP_CACHE_CONTROL_NO_STORE:
            len = snprintf(resb->last, buffer_rest(resb), "Cache-Control: no-store"CRLF);
            break;
        case HTTP_CACHE_CONTROL_PRIVATE:
        default:
            len = 0;
            break;
        }
        resb->last += len;

        /* append Etag */
        if (res->etag[0] != '\0') {
            len = snprintf(resb->last, buffer_rest(resb), "Etag: %.*s"CRLF, HTTP_ETAG_LEN, res->etag);
            resb->last += len;
        }

        switch (res->code) {
        case HTTP_206:
            len = snprintf(resb->last, buffer_rest(resb), "Content-Range: bytes %d-%d/%d"CRLF,
                    com->content_range1, com->content_range2, com->file_size);
            resb->last += len;
            break;
        }
    }

    switch (res->code) {
    case HTTP_200:
    case HTTP_206:
        /* Last Modified */
        gmt = gmtime(&com->last_modified);
        len = snprintf(resb->last, buffer_rest(resb), "Last-Modified: %s, %02d %s %4d %02d:%02d:%02d GMT"CRLF,
                week[gmt->tm_wday], gmt->tm_mday,
                months[gmt->tm_mon], gmt->tm_year+1900,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
        resb->last += len;
        break;
    case HTTP_301:
    case HTTP_304:
        /* Location 301 */
        if (res->code == 301) {
            len = snprintf(resb->last, buffer_rest(resb), "Location: %.*s"CRLF,
                    res->location.len, res->location.str);
            resb->last += len;
        }
        break;
    }

    if (!string_isnull(&res->set_cookie)) {
        len = snprintf(resb->last, buffer_rest(resb), "Set-Cookie: %.*s"CRLF,
                res->set_cookie.len, res->set_cookie.str);
        resb->last += len;
    }

    *resb->last++ = CR;
    *resb->last++ = LF;
    yhttp_debug2("response header %d:%d\n%.*s\n",
            buffer_len(resb), buffer_rest(resb), buffer_len(resb), resb->pos);

    return 0;
}

extern http_dispatcher_t http_dispatch(http_request_t *r)
{
    struct http_head_req const *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    http_file_t const *file = &r->backend.file;

    if (!(file->stat.st_mode&S_IFREG)
            || !(file->stat.st_mode&(S_IRUSR|S_IRGRP))) {
        res->code = HTTP_403;
        return HTTP_DISPATCHER(req->method, HTTP_403);
    }

    com->file_size = file->stat.st_size;

    com->content_encoding = HTTP_IDENTITY;
    com->content_length = file->stat.st_size;
    com->last_modified = file->stat.st_ctim.tv_sec;
    com->pragma = HTTP_PRAGMA_UNSET;
    com->content_type = http_mime_get(req->suffix, req->suffix_len);
    com->transfer_encoding = HTTP_UNCHUNKED;
    r->pos = 0;
    r->last = com->content_length;

    if (HTTP11 == com->version) {
        int etag_n;
        com->cache_control = HTTP_CACHE_CONTROL_NO_CACHE;
        com->cache_control_max_age = YHTTP_CACHE_MAX_AGE_CFG;

        etag_n = HTTP_ETAG_LEN;
        http_generate_etag(&req->uri, file->stat.st_ctim.tv_nsec, file->stat.st_size,
                res->etag, &etag_n);
        if (!string_isnull(&req->if_none_match)
                && !strncmp(req->if_none_match.str, res->etag, HTTP_ETAG_LEN)) {
            res->code = HTTP_304;
            return HTTP_DISPATCHER(req->method, HTTP_304);
        }

        /* Range, 206 Partial Content */
        if (req->range1 != 0 || req->range2 != INT_MAX) {
            r->pos = (req->range1 < 0)? r->last+req->range1: req->range1;
            if (req->range2 < INT_MAX)
                r->last = req->range2+1;
            if (r->pos < 0 || r->last > file->stat.st_size) {
                res->code = HTTP_416;
                return HTTP_DISPATCHER(req->method, HTTP_416);
            } else if (r->pos > 0 || r->last < file->stat.st_size) {
                com->content_range1 = r->pos;
                com->content_range2 = r->last-1;
                com->content_length = r->last - r->pos;
                res->code = HTTP_206;
                return HTTP_DISPATCHER(req->method, HTTP_206);
            }
            /* start-end indicates all file */
        }
    }

    if ((int)difftime(req->if_modified_since, file->stat.st_ctim.tv_sec) > 0) {
        res->code = HTTP_304;
        return HTTP_DISPATCHER(req->method, HTTP_304);
    }

    res->code = HTTP_200;
    return HTTP_DISPATCHER(req->method, HTTP_200);
}

extern void http_generate_etag(string_t const *url, time_t ctime, size_t size,
        char *out, int *out_n)
{
#define HTTP_ETAG_HALFLEN   (HTTP_ETAG_LEN/2)
    unsigned char etag[HTTP_ETAG_HALFLEN] = "";
    if (*out_n < HTTP_ETAG_LEN) {
        *out_n = -1;
        return;
    }
    *out_n = HTTP_ETAG_LEN;
    uint64_t h = 0;
    for (int i = 0; i < url->len; i++)
        h = 31*h+ url->str[i];

#define hash_digit(h, d) do { \
    char const *p = (char const *)&d; \
    for (size_t i = 0; i < sizeof(d); i++) \
    h = 31*h+p[i]; \
} while (0)

#define xor_string(d) do { \
    char const *p = (char const *)&d; \
    for (int i = 0; i < HTTP_ETAG_HALFLEN; i++) \
    etag[i] ^= p[i%sizeof(d)]; \
} while (0)

    hash_digit(h, ctime);
    hash_digit(h, size);
    xor_string(h);

#undef hash_digit
#undef xor_digit
    for (int i = 0; i < HTTP_ETAG_HALFLEN; i++)
    sprintf(out+2*i, "%02x", etag[i]);
}

extern int http_request_extend_hdr_buffer(http_request_t *r)
{
    buffer_t *b;

    if (r->hdr_buffer_large) {
        yhttp_debug2("http request header is too large\n");
        return YHTTP_FAILE;
    }
    b = buffer_malloc(YHTTP_LARGE_BUFFER_SIZE_CFG);
    if (!b) {
        yhttp_debug("allocate memory for http_large_buffer error\n");
        return YHTTP_ERROR;
    }
    yhttp_debug("Allocate memory for http_large_buffer\n");

    BUG_ON(NULL == r->parse_p);
    BUG_ON(r->parse_p < r->hdr_buffer->pos);

    r->hdr_buffer_large = 1;
    buffer_copy(b, r->hdr_buffer);
    buffer_free(r->hdr_buffer);
    r->hdr_buffer = b;
    r->parse_p = b->pos;
    /* must REPARSE the header ... */
    return YHTTP_OK;
}

extern int http_chunk_transmit_over(char const *start, char const *end)
{
    return YHTTP_OK;
}

extern void http_print_request(http_request_t *r)
{
    char const *method;
    struct http_head_req *req = &r->hdr_req;
    switch (req->method) {
    case HTTP_GET:
        method = "GET";
        break;
    case HTTP_POST:
        method = "POST";
        break;
    case HTTP_PUT:
        method = "PUT";
        break;
    case HTTP_DELETE:
        method = "DELETE";
        break;
    case HTTP_TRACE:
        method = "TRACE";
        break;
    case HTTP_HEAD:
        method = "HEAD";
        break;
    default:
        method = "Unkown";
        break;
    }
    yhttp_info("%s %.*s\n", method, req->uri.len, req->uri.str);
}
