#include <string.h>
#include "common.h"
#include "ring-buffer.h"

#define next(idx, size)     (((idx)+1)%(size))
#define nextn(idx, n, size) (((idx)+(n))%(size))

extern ringbuffer_t ringbuffer_create(int size)
{
    size += 1;
    ringbuffer_t ret = malloc(sizeof(*ret) + size);
    if (!ret) return NULL;
    ret->size = size;
    ret->idx = ret->datan = 0;

    return ret;
}
extern void ringbuffer_destory(ringbuffer_t *buff)
{
    if (!buff || !*buff) return;
    free(*buff);

    *buff = NULL;
}
extern int ringbuffer_pad(ringbuffer_t _buff, uint8_t *padding, int len)
{
    if (!_buff) return -1;
    /* full */
    if (ringbuffer_full(_buff))
        return 0;
    int idx    = _buff->idx;
    int *datan = &_buff->datan;
    int size   = _buff->size;
    uint8_t *buff = _buff->buffer;
    int rdn = (*datan < idx)? (idx - *datan -1): (size - *datan -1);
    /* TODO ringbuffer_pad, if len > rdn, to finish it */
    rdn = MIN(rdn, len);
    memcpy(buff+*datan, padding, rdn);
    *datan = nextn(*datan, rdn, size);

    return rdn;
}

extern int ringbuffer_read(int fd, ringbuffer_t _buff)
{
    if (!_buff) return -1;
    if (ringbuffer_full(_buff)) return 0;

    int *datan = &_buff->datan;
    int *idx   = &_buff->idx;
    int buffsize   = _buff->size;
    uint8_t *buff  = _buff->buffer;
    //_M(LOG_DEBUG2, "ringbuffer_read()  in fd: %d buffsize: %d idx: %d n: %d\n",
    //        fd, buffsize, *idx, *datan);

    /* #: pad
     * .: empty
     * buffer: |#######datan.......idx##########|     *datan < *idx
     * buffer: |.......idx#######datan..........|     *datan >= *idx
     */
    int rdn = (*datan < *idx)? (*idx - *datan -1): (buffsize - *datan -1);
    rdn = read_s(fd, buff + *datan, rdn);
    *datan = nextn(*datan, rdn, buffsize);

    //_M(LOG_DEBUG2, "ringbuffer_read()  out fd: %d buffsize: %d idx: %d n: %d\n",
    //        fd, buffsize, *idx, *datan);
    return rdn;
}

extern int ringbuffer_write(int fd, ringbuffer_t _buff)
{
    if (!_buff) return -1;
    if (ringbuffer_empty(_buff)) return 0;

    int *datan = &_buff->datan;
    int *idx   = &_buff->idx;
    int buffsize   = _buff->size;
    uint8_t *buff  = _buff->buffer;

    //_M(LOG_DEBUG2, "ringbuffer_write() in fd: %d buffsize: %d idx: %d n: %d\n",
    //        fd, buffsize, *idx, *datan);
    /* empty */
    if (*idx == *datan) {
        *idx = *datan = 0;
        return 0;
    }

    int wrn = (*idx < *datan)? (*datan - *idx): (buffsize - *idx);
    wrn = write_s(fd, buff + *idx, wrn);
    *idx = nextn(*idx, wrn, buffsize);

    //_M(LOG_DEBUG2, "ringbuffer_write() out fd: %d buffsize: %d idx: %d n: %d\n",
    //        fd, buffsize, *idx, *datan);
    return wrn;
}
