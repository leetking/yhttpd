#ifndef HTTP_H__
#define HTTP_H__

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sys/stat.h>

#include "common.h"
#include "buffer.h"
#include "connection.h"
#include "http_page.h"
#include "http_file.h"

#define HTTP_BUFFER_SIZE_CFG        4096
#define HTTP_LARGE_BUFFER_SIZE_CFG  (2*HTTP_BUFFER_SIZE_CFG)

#define CR     '\r'
#define LF     '\n'
#define CRLF   "\r\n"

#define HTTP_COOKIE_PAIR_MAX_CFG    12

#define HTTP_GET     0x001  /* HTTP/1.0 */
#define HTTP_POST    0x002  /* HTTP/1.0 */
#define HTTP_HEAD    0x004  /* HTTP/1.0 */
#define HTTP_PUT     0x008  /* HTTP/1.0 */
#define HTTP_DELETE  0x010  /* HTTP/1.0 */
#define HTTP_TRACE   0x020
#define HTTP_OPTIONS 0x040
#define HTTP_CONNECT 0x080
#define HTTP_LINK    0x100  /* HTTP/1.0 only */
#define HTTP_UNLINK  0x200  /* HTTP/1.0 only */

#define HTTP09  0
#define HTTP10  1
#define HTTP11  2
#define HTTP2   3

/* Content Encoding or Accept Encoding */
#define HTTP_IDENTITY   0x01   /*  don't respond */
#define HTTP_GZIP       0x02
#define HTTP_DEFLATE    0x04
#define HTTP_COMPRESS   0x08

/**
 * Pragma: no-cache
 * HTTP/1.1: if request has Cache-Control: no-cache, SHOULD set it as no-cache
 */
#define HTTP_PRAGMA_UNSET       0
#define HTTP_PRAGMA_NO_CACHE    1

/* language */
#define HTTP_LANG_CN    0x01
#define HTTP_LANG_EN    0x02
#define HTTP_LANG_JP    0x04

/* Transfer Encoding */
#define HTTP_UNCHUNKED 0x00
#define HTTP_CHUNKED   0x01

/* state code */
enum {
    HTTP_200 = 200,     /* OK */
    HTTP_202 = 202,     /* Accept */

    HTTP_301 = 301,     /* Moved Permanently */

    HTTP_400 = 400,     /* Bad Request */
    HTTP_403 = 403,     /* Forbidden */
    HTTP_404 = 404,     /* Not Found */
    HTTP_405 = 405,     /* Method Not Allowed */
    HTTP_413 = 413,     /* Request Entity Too Large */
    HTTP_414 = 414,     /* Request-URI TOo Large */
    HTTP_415 = 415,     /* Unsupported Media Type */
    HTTP_416 = 416,     /* Requested range not statisfiable */

    HTTP_500 = 500,     /* Internal Server Error */
    HTTP_501 = 501,     /* Not Implemented */
    HTTP_505 = 505,     /* HTTP Version not supported */
};

struct http_head_com {

    string_t cache_control;
    string_t update;
    string_t via;
    string_t warning;

    int content_length;
    int content_range1, content_range2;
    string_t content_md5;

    time_t expires;
    time_t last_modified;
    time_t date;

    string_t str_cookie;
    str_pairt_t cookies[HTTP_COOKIE_PAIR_MAX_CFG];
    uint8_t keep_alive;

    unsigned version:2;
    unsigned transfer_encoding:1;
    unsigned content_language:3;
    unsigned connection:1;
    unsigned pragma:1;
    unsigned allow:10;              /* method */
    unsigned trailer:1;             /* dont support */
    unsigned content_encoding:4;    /* only support identity */
    unsigned content_type:10;       /* mime */
};

/* TODO optimize structure http_head_req */
struct http_head_req {
    unsigned method:10;
    uint16_t port;
    string_t uri;
    string_t uri_params;
    string_t suffix;
    string_t query;

    unsigned accept:2;
    string_t accept_charset;
    unsigned accept_encoding:4; /* content_encoding */
    unsigned accept_language:3;
    string_t authorization;
    string_t expect;
    string_t from;
    string_t host;
    time_t if_modified_since;
    string_t if_none_match;
    string_t if_range;
    time_t if_unmodified_since;
    int max_forwards;
    string_t proxy_authorication;
    int range1, range2;
    string_t referer;
    string_t te;
    string_t user_agent;
    string_t cookies;

    string_t origin;
};

struct http_head_res {
    int16_t code;

    string_t accept_ranges;
    int age;
    string_t etag;
    string_t location;
    string_t proxy_authoricate;
    string_t proxy_after;
    /* string_t server; */
    string_t vary;
    string_t www_authenticate;
};

/* a request struct */
typedef struct http_request_t {
    struct http_head_com com;           /* the common portion of head */
    struct http_head_req req;           /* the rest head of requst */
    struct http_head_res res;           /* the rest head of response */

    buffer_t *request_head;
    buffer_t *request_body;
    uint8_t request_head_large:1;

    buffer_t *response_buffer;          /* buffer of response */
    off_t pos, size;
    union {
        http_file_t file;
        http_page_t const *page;        /* cache page or error page */
    } backend;

    char *parse_pos;                    /* current position at the buffer */
    int parse_state;                    /* state of parsing */
    uint8_t hdr_type;
} http_request_t;

extern int http_init();
extern void http_destroy();
extern http_request_t *http_request_malloc();
extern void http_request_free(http_request_t *req);
extern void http_request_reuse(http_request_t *req);

extern int http_check_request(http_request_t *req);
extern int http_build_response_head(http_request_t *req);
extern void http_init_error_page(event_t *ev);
extern void http_init_response(event_t *ev);

#endif /* HTTP_H__ */
