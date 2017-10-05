#ifndef RING_BUGGER_H__
#define RING_BUGGER_H__

#include <stdlib.h>
#include <stdint.h>

extern int ringbuffer_read(int fd, uint8_t *buff, size_t buffsize, int *idx, int *datan);
extern int ringbuffer_write(int fd, uint8_t *buff, size_t buffsize, int *idx, int *datan);

#endif /* RING_BUGGER_H__ */
