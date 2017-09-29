#ifndef HTTP_H__
#define HTTP_H__

#include <stdlib.h>
#include <stdint.h>

#define HTTP_METADATA_LEN    4096

typedef struct string_st {
    uint8_t *str;
    int len;
} string_t;

typedef struct cookie_st {
} cookie_t;

enum {
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

struct http_request {
    int8_t   method;
    int8_t   ver;
    string_t path;
    string_t host;
    string_t ua;
    cookie_t cookie;
    int range1, range2;

    uint8_t iscon:1;
    uint8_t ischunk:1;
    uint8_t _padding:6;

    int _status;  /* 解析状态 */
    int _metadatalen;
    int _metadataidx;
    uint8_t _metadata[1];
};

extern int http_request_parse(struct http_request *req, uint8_t const *buff, ssize_t len);

#endif /* HTTP_H__ */
