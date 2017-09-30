#ifndef HTTP_H__
#define HTTP_H__

#include <stdlib.h>
#include <stdint.h>

#define HTTP_METADATA_LEN    4096

#define LF     (uint8_t)'\n'
#define CR     (uint8_t)'\r'
#define CRLF   "\r\n"


typedef struct string_st {
    uint8_t *str;
    int len;
} string_t;

typedef struct cookie_st {
} cookie_t;

enum {
    HTTP_REQ_CONTINUE,
    HTTP_REQ_INVALID,
    HTTP_REQ_FINISH,
};
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

enum {
    HTTP_URI = 0,
    HTTP_HOST,
    HTTP_CON,
    HTTP_UA,
    HTTP_ACCEPT,
    HTTP_ACCEPT_ENCODING,
    HTTP_ACCEPT_LANGUAGE,

    HTTP_LINE_MAX,
};

struct http_request {
    int8_t   method;
    int8_t   ver;
    string_t lines[HTTP_LINE_MAX];
    int range1, range2;

    uint8_t iscon:1;
    uint8_t ischunk:1;
    uint8_t _padding:6;

    int _line_type;
    int _ps_status;  /* 解析状态 */
    int _metadatalen;
    int _metadataidx;
    uint8_t _metadata[1];
};

extern int http_request_parse(struct http_request *req, uint8_t const *buff, ssize_t len);

#endif /* HTTP_H__ */
