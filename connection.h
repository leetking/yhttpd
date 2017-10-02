#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <stdint.h>
#include "http.h"

/* 内部维护一个状态机 */

struct connection {
    /* public */
    int sktfd;  /* socket file description */
    int nrmfd;  /* local file description, init with -1 */

    /* private */
    struct http_request *_http_req;
    int _status;
    int _buffsize;          /* only use _buffsize-1 */
    int _rdi, _rdn;
    int _wri, _wrn;
    uint8_t *_wrbuff;
    uint8_t _rdbuff[1];     /* a trick */
    /*
     * sktfd --read--> [ rdi | rdbuff | rdn ] --write-> nrmfd
     * sktfd <-write-- [ wri | wrbuff | wrn ] <--read-- nrmfd
     */
};

extern struct connection *connection_create(int fd);
extern int connection_write(struct connection *c);
extern int connection_read(struct connection *c);
extern int connection_read_nrm(struct connection *c);
extern int connection_write_nrm(struct connection *c);
extern int connection_isvalid(struct connection const *c);
extern void connection_destory(struct connection **c);

#endif /* CONNECTION_H__ */
