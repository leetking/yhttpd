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
    if (!_buff || !padding || len < 0) return -1;
    if (ringbuffer_full(_buff) || 0 == len) return 0;

    int ret;
    int idx    = _buff->idx;
    int *datan = &_buff->datan;
    int size   = _buff->size;
    uint8_t *buff = _buff->buffer;
    /*
     * #: padded
     * .: empty
     * i: idx
     * n: datan
     * s: size
     * buffer: [#######.......##########]     datan <  idx
     *                 n      i         s
     * buffer: [.......#########........]     datan >= idx
     *                 i        n       s
     */
    int rdn = (*datan < idx)? (idx - *datan -1): (size - *datan - (0==idx));

    rdn = MIN(rdn, len);
    memcpy(buff+*datan, padding, rdn);
    *datan = nextn(*datan, rdn, size);
    ret = rdn;
    len -= rdn;

    /*
     * [......#########]
     *  n     i
     */
    if (!ringbuffer_full(_buff) && len > 0) {
        rdn = idx-*datan -1;
        rdn = MIN(rdn, len);
        memcpy(buff+*datan, padding+ret, rdn);
        *datan = nextn(*datan, rdn, size);
        ret += rdn;
    }

    return ret;
}
extern int ringbuffer_dump(ringbuffer_t _buff, uint8_t *buffer, int len)
{
    if (!_buff || !buffer || len < 0) return -1;
    if (ringbuffer_empty(_buff) || len == 0) return 0;

    int ret;
    int *idx  = &_buff->idx;
    int datan = _buff->datan;
    int size   = _buff->size;
    uint8_t *buff = _buff->buffer;

    /*
     * #: padded
     * .: empty
     * i: idx
     * n: datan
     * s: size
     * buffer: [.......#########........]     datan >= idx
     *                 i        n       s
     * buffer: [#######.......##########]     datan <  idx
     *                 n      i         s
     */
    int wrn = (*idx < datan)? (datan - *idx): (size - *idx);
    wrn = MIN(wrn, len);
    memcpy(buffer, buff+*idx, wrn);
    ret = wrn;
    len -= wrn;
    *idx = nextn(*idx, wrn, size);

    /*
     * [#######......]
     *  i      n
     */
    if (!ringbuffer_empty(_buff) && len > 0) {
        wrn = datan - *idx;
        wrn = MIN(wrn, len);
        memcpy(buffer+ret, buff+*idx, wrn);
        *idx = nextn(*idx, wrn, size);
        ret += wrn;
    }

    return ret;
}

extern int ringbuffer_read(int fd, ringbuffer_t _buff)
{
    if (!_buff || -1 == fd) return -1;
    if (ringbuffer_full(_buff)) return 0;

    int *datan = &_buff->datan;
    int idx    = _buff->idx;
    int buffsize   = _buff->size;
    uint8_t *buff  = _buff->buffer;

    int rdn = (*datan < idx)? (idx - *datan -1): (buffsize - *datan - (0==idx));
    rdn = read_s(fd, buff + *datan, rdn);
    if (rdn == -1) return -1;
    *datan = nextn(*datan, rdn, buffsize);

    return rdn;
}

extern int ringbuffer_write(int fd, ringbuffer_t _buff)
{
    if (!_buff || -1 == fd) return -1;
    if (ringbuffer_empty(_buff)) {
        _buff->idx = _buff->datan = 0;
        return 0;
    }

    int datan = _buff->datan;
    int *idx  = &_buff->idx;
    int buffsize   = _buff->size;
    uint8_t *buff  = _buff->buffer;

    int wrn = (*idx < datan)? (datan - *idx): (buffsize - *idx);
    wrn = write_s(fd, buff + *idx, wrn);
    if (-1 == wrn) return -1;
    *idx = nextn(*idx, wrn, buffsize);

    return wrn;
}
