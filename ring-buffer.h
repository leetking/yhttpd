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
extern void ringbuffer_destory(ringbuffer_t *buff);
extern int  ringbuffer_read(int fd, ringbuffer_t buff);
extern int  ringbuffer_write(int fd, ringbuffer_t buff);
/**
 * pad from `padding' to ringbuffer
 * @return: >=0: byte(s) of padding, maybe it is lesser than len
 *           -1: error
 */
extern int ringbuffer_pad(ringbuffer_t buff, uint8_t *padding, int len);
/**
 * dump from ringbuffer to `buffer'
 * len: the size of `buffer'
 * @return: >=0: byte(s) of dumping
 *           -1: error
 */
extern int ringbuffer_dump(ringbuffer_t buff, uint8_t *buffer, int len);
#define ringbuffer_full(buff)   (((buff)->datan+1) % (buff)->size == (buff)->idx)
#define ringbuffer_empty(buff)  ((buff)->datan == (buff)->idx)
#define ringbuffer_size(buff)   ((buff)->size - 1)
#define ringbuffer_clean(buff)  ((buff)->datan = (buff)->idx = 0)

#endif /* RING_BUGGER_H__ */
