#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>

#include "../ring-buffer.h"

static void test_pad_dump(void)
{
    uint8_t temp[512];
    const int BUFF_SIZE = 5;
    int ret;
    ringbuffer_t buff = ringbuffer_create(BUFF_SIZE);
    assert(buff);
    assert(buff->idx == 0);
    assert(buff->datan == 0);
    assert(BUFF_SIZE == ringbuffer_size(buff));
    /*
     * [0123.x]
     *  i   n
     */
    ret = ringbuffer_pad(buff, (uint8_t*)"0123", 4);
    assert(ret == 4);
    assert(buff->idx == 0);
    assert(buff->datan == 4);
    /*
     * [..23.x]
     *    i n
     */
    ret = ringbuffer_dump(buff, temp, 2);
    assert(ret == 2);
    assert(buff->idx == 2);
    assert(buff->datan == 4);
    /*
     * [6x2345]
     *   ni
     */
    ret = ringbuffer_pad(buff, (uint8_t*)"456", 3);
    assert(ret == 3);
    assert(buff->idx == 2);
    assert(buff->datan == 1);
    assert(ringbuffer_full(buff));
    /*
     * [6x..45]
     *   n  i
     */
    ret = ringbuffer_dump(buff, temp, 2);
    assert(ret == 2);
    assert(buff->idx == 4);
    assert(buff->datan == 1);
    /*
     * [.x....]
     *   i(n)
     */
    ret = ringbuffer_dump(buff, temp, 3);
    assert(ret == 3);
    assert(buff->idx == 1);
    assert(buff->datan == 1);
    assert(ringbuffer_empty(buff));

    ringbuffer_destory(&buff);
}

static void test_read_write(void)
{
    const int BUFF_SIZE = 3;
    int fd = open("tests.in/test-ringbuffer.data", O_RDONLY);
    assert(-1 != fd);
    ringbuffer_t buff = ringbuffer_create(BUFF_SIZE);

    int n = ringbuffer_read(fd, buff);
    assert(n == BUFF_SIZE);
    assert(buff->idx == 0);

    ringbuffer_destory(&buff);
    close(fd);
}

static void test_random(void)
{
}

static void test_performance(void)
{
}

int main()
{
    test_pad_dump();
    test_read_write();
    test_random();
    test_performance();
}
