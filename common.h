#ifndef COMMON_H__
#define COMMON_H__

#include <stdint.h>
#include <stdlib.h>

#include "log.h"

#define BUFF_SIZE   2048
#define ACCEPT_LOCK "yhttp-"VER".lock"

#define _M(l, ...) yhttp_log(l, __VA_ARGS__)

#define MAX(x, y)   ((x)>(y)? (x): (y))
#define MIN(x, y)   ((x)<(y)? (x): (y))

ssize_t writen(int fd, uint8_t const *buff, ssize_t n);
ssize_t readn(int fd, uint8_t const *buff, ssize_t n);

#endif /* COMMON_H__ */
