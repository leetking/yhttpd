#ifndef COMMON_H__
#define COMMON_H__

#include <stdint.h>
#include <stdlib.h>
#include <linux/limits.h>

#include "log.h"

#define BUFF_SIZE   (7)
#define ACCEPT_LOCK "yhttp-" VER ".lock"
#define TIMEOUT     (50)     /* s */

#define _M(l, ...) yhttp_log(l, __VA_ARGS__)

#define MAX(x, y)    ((x)>(y)? (x): (y))
#define MIN(x, y)    ((x)<(y)? (x): (y))
#define ARRSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

/* Global veriablesa */
extern char root_path[PATH_MAX];
extern char cgi_path[PATH_MAX];
extern char err_codes_path[PATH_MAX];

/* read stable, ignore interrupt */
ssize_t write_s(int fd, uint8_t const *buff, ssize_t n);
ssize_t read_s(int fd, uint8_t *buff, ssize_t n);

#endif /* COMMON_H__ */
