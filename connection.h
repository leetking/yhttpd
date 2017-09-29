#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <stdint.h>
#include "http.h"

/* 内部维护一个状态机 */

struct connection {
    /* public */
    int fd;

    /* private */
    struct http_request *_http_req;
    int _status;
    int _buffsize;
    int _rdn;
    int _wrn;
    uint8_t *_wrbuff;
    uint8_t _rdbuff[1];     /* a trick */
};

struct connection *connection_create(int fd);
int connection_write(struct connection *c);
int connection_read(struct connection *c);
int connection_isvalid(struct connection const *c);
void connection_destory(struct connection **c);

#endif /* CONNECTION_H__ */
