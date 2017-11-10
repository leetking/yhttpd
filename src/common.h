#ifndef COMMON_H__
#define COMMON_H__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

#define BUFF_SIZE   (2048)
#define ACCEPT_LOCK "yhttp-"VER".lock"
#define TIMEOUT     (10*1000)     /* 10 s */
#define TIME_INTERVAL   (1000)    /* 1 s */

#define MAX(x, y)    ((x)>(y)? (x): (y))
#define MIN(x, y)    ((x)<(y)? (x): (y))
#define ARRSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#define YHTTP_OK     (0)
#define YHTTP_ERROR  (-1)
#define YHTTP_AGAIN  (-2)

typedef intptr_t msec_t;

typedef struct string_t {
    char const *str;
    int len;
} string_t;

/* read stable, ignore interrupt */
ssize_t write_s(int fd, uint8_t const *buff, ssize_t n);
ssize_t read_s(int fd, uint8_t *buff, ssize_t n);

#ifndef offsetof
# define offsetof(type, member) \
    ((int)&((type*)0)->member)
#endif
#ifndef container_of
# define container_of(ptr, type, member) \
    ((type*)((char*)(ptr)-offsetof(type, member)))
#endif

#endif /* COMMON_H__ */
