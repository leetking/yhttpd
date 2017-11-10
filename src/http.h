#ifndef HTTP_H__
#define HTTP_H__

#include <stdlib.h>
#include <stdint.h>

#include "common.h"
#include "buffer.h"
#include "connection.h"

#define HTTP_BUFFER_SIZE    4096

#define CR     '\r'
#define LF     '\n'
#define CRLF   "\r\n"

#define HTTP_COOKIE_PAIR_MAX    12
typedef struct cookie_t {
    string_t cookie[HTTP_COOKIE_PAIR_MAX];
} cookie_t;

#define HTTP_GET     0
#define HTTP_POST    1
#define HTTP_HEAD    2
#define HTTP_PUT     3
#define HTTP_DELETE  4
#define HTTP_TRACE   5
#define HTTP_OPTIONS 6
#define HTTP_CONNECT 7

#define HTTP09  9
#define HTTP10  10
#define HTTP11  11

#define HTTP_PARSE_INIT 0

/* HTTP lines index */
enum {
    HTTP_URI = 0,
    HTTP_QUERY,             /* query string for GET, HEAD */
    HTTP_HOST,              /* Host */
    HTTP_UA,                /* User-Agent */
    HTTP_ACCEPT,            /* Accept */
    HTTP_ACCEPT_ENCODING,   /* Accept-Encoding */
    HTTP_ACCEPT_LANGUAGE,   /* Accept-Language */
    HTTP_REFERER,           /* Referer */
    HTTP_COOKIE,            /* Cookie */

    HTTP_LINE_MAX,
};

/* status code */
enum {
    HTTP_200 = 0, /* OK */
    HTTP_202,     /* Accept */

    HTTP_301,     /* Moved Permanently */

    HTTP_400,     /* Bad Request */
    HTTP_403,     /* Forbidden */
    HTTP_404,     /* Not Found */
    HTTP_405,     /* Method Not Allowed */
    HTTP_413,     /* Request Entity Too Large */
    HTTP_414,     /* Request-URI TOo Large */
    HTTP_415,     /* Unsupported Media Type */
    HTTP_416,     /* Requested range not statisfiable */

    HTTP_500,     /* Internal Server Error */
    HTTP_501,     /* Not Implemented */
    HTTP_505,     /* HTTP Version not supported */

    HTTP_CODE_MAX,
};

static char const * http_code_str[] = {
    "200 OK",
    "202 Accept",

    "301 Moved Permanently",

    "400 Bad Request",
    "403 Forbidden",
    "404 Not Found",
    "405 Method Not Allowed",
    "413 Request Entity Too Large",
    "414 Request-URI TOo Large",
    "415 Unsupported Media Type",
    "416 Requested range not statisfiable",

    "500 Internal Server Error",
    "501 Not Implemented",
    "505 HTTP Version not supported",

    NULL,
};

struct http_header_in {
    int8_t version;
    string_t host;
    uint8_t iscon:1;
    uint8_t ischunk:1;
};

struct http_header_req {
    unsigned method;
};

struct http_header_res {
    int8_t code;
};

/* a request struct */
typedef struct http_request_t {
    struct http_header_in header_in;    /* the common portion of header */
    struct http_header_req req;         /* the rest header of requst */
    struct http_header_res res;         /* the rest header of response */

    connection_t con_req;
    connection_t con_res;

    buffer_t *request_head;
    buffer_t *request_body;
    union {
        buffer_t *response;
        /* TODO complete http_request_t structure */
    } response;

    struct {
        uint8_t fin_lf:1;
        int line_type;
        int status;             /* status of parsing */
        char const *pos;        /* current position at the buffer */
        char const *end;        /* end of the buffer */
    } parse_status;
} http_request_t;

extern http_request_t *http_request_new(int fd);
extern void http_request_destroy(http_request_t **req);
void http_init_request(void *req);

#endif /* HTTP_H__ */
