#ifndef CONNECTION_H__
#define CONNECTION_H__

#include <stdint.h>
#include <time.h>
#include <semaphore.h>
#include "http.h"
#include "ring-buffer.h"

/* 内部维护一个状态机 */

struct connection {
    struct {
        int fd;
        int rdn, rdmax;
        int wrn, wrmax;
        uint8_t rdeof:1;
        uint8_t wreof:1;
        uint8_t rdreg:1;
        uint8_t wrreg:1;
    } socket;

    struct {
        int fdro, fdwo;
        int rdn, rdmax;
        int wrn, wrmax;
        uint8_t rdeof:1;
        uint8_t wreof:1;
        uint8_t rdreg:1;
        uint8_t wrreg:1;
    } normal;

    /*
     * sktfd --read--> [ rdbuff ] --write-> fdwo
     * sktfd <-write-- [ wrbuff ] <--read-- fdro
     */
    ringbuffer_t rdbuff;
    ringbuffer_t wrbuff;

    struct http_head *http;
};

extern void connection_destory(struct connection **c);
extern struct connection *connection_create(int sktfd);
/**
 * Finish a reqeust and response
 */
extern int connection_a_tx(struct connection *c);
extern void parse_request(struct connection *con);

#endif /* CONNECTION_H__ */
