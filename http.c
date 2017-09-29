#include <ctype.h>

#include "http.h"

extern int http_request_parse(struct http_request *req, uint8_t const *buff, ssize_t len)
{
    /* 解析请求 */
    enum {
        PS_START,
        PS_METHOD,
    };

    /* TODO parse it! */
    uint8_t const *end = buff+len;
    for (uint8_t const *p = buff; p < end; p++) {
        switch (req->_status) {
        case PS_START:
            break;
        case PS_METHOD:
            break;
        }
    }

    return HTTP_REQ_INVALID;
}


