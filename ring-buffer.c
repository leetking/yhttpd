#include "common.h"
#include "ring-buffer.h"

#define next(idx, size)     (((idx)+1)%(size))
#define nextn(idx, n, size) (((idx)+(n))%(size))

extern int ringbuffer_read(int fd, uint8_t *buff, size_t buffsize, int *idx, int *datan)
{
    _M(LOG_DEBUG2, "ringbuffer_read in fd: %d buffsize: %d idx: %d n: %d\n",
            fd, buffsize, *idx, *datan);
    /* full */
    if (next(*datan, buffsize) == *idx)
        return 0;

    /* #: pad
     * .: empty
     * buffer: |#######datan.......idx##########|     *datan < *idx
     * buffer: |.......idx#######datan..........|     *datan >= *idx
     */
    int rdn = (*datan < *idx)? (*idx - *datan -1): (buffsize - *datan +1);
    rdn = read_s(fd, buff + *datan, rdn);
    *datan = nextn(*datan, rdn, buffsize);

    _M(LOG_DEBUG2, "ringbuffer_read out fd: %d buffsize: %d idx: %d n: %d\n",
            fd, buffsize, *idx, *datan);
    return rdn;
}

extern int ringbuffer_write(int fd, uint8_t *buff, size_t buffsize, int *idx, int *datan)
{
    _M(LOG_DEBUG2, "ringbuffer_write in fd: %d buffsize: %d idx: %d n: %d\n",
            fd, buffsize, *idx, *datan);
    /* empty */
    if (*idx == *datan) {
        *idx = *datan = 0;
        return 0;
    }

    int wrn = (*idx < *datan)? (*datan - *idx): (buffsize - *idx);
    wrn = write_s(fd, buff + *idx, wrn);
    *idx = nextn(*idx, wrn, buffsize);

    _M(LOG_DEBUG2, "ringbuffer_write out fd: %d buffsize: %d idx: %d n: %d\n",
            fd, buffsize, *idx, *datan);
    return wrn;
}
