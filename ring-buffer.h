#ifndef RING_BUGGER_H__
#define RING_BUGGER_H__

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    int size;
    int idx;
    int datan;
    uint8_t buffer[1];
} *ringbuffer_t;

extern ringbuffer_t ringbuffer_create(int size);
extern void         ringbuffer_destory(ringbuffer_t *buff);
extern int          ringbuffer_read(int fd, ringbuffer_t buff);
extern int          ringbuffer_write(int fd, ringbuffer_t buff);
extern int          ringbuffer_pad(ringbuffer_t buff, uint8_t *padding, int len);
#define ringbuffer_full(buff)   (((buff)->datan+1) % (buff)->size == (buff)->idx)
#define ringbuffer_empty(buff)  ((buff)->datan == (buff)->idx)

#endif /* RING_BUGGER_H__ */
