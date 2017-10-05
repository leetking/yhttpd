#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <stdint.h>
#include <time.h>
#include "http.h"

/* 内部维护一个状态机 */

struct connection {
    /* public */
    int sktfd;  /* socket file description */
    int fdro;   /* local file description, read only, init with -1 */
    int fdwo;   /* local file description, write only, init with -1 */
    int timeout; /* s, -1 never, default TIMEOUT */

    /* private */
    time_t _last_req;
    struct http_head *_http;
    int _con_status;
    int _req_status;
    int _res_status;
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

extern struct connection *connection_create(int sfd);
extern int connection_write_skt(struct connection *c);
extern int connection_read_skt(struct connection *c);
extern int connection_read_file(struct connection *c);
extern int connection_write_file(struct connection *c);
extern int connection_isvalid(struct connection const *c);
extern void connection_destory(struct connection **c);
extern int connection_settimeout(struct connection *c, int timeout);

extern int connection_need_write_skt(struct connection *c);
extern int connection_need_write_file(struct connection *c);

#endif /* CONNECTION_H__ */
