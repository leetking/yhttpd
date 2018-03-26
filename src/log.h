#ifndef LOG_H__
#define LOG_H__

#include <stdio.h>

enum {
    LOG_ERROR  = 0,
    LOG_WARN   = 1,
    LOG_INFO   = 2,
    LOG_DEBUG  = 3,
    LOG_DEBUG2 = 4,
};

extern void yhttp_log_set(int level);
extern int  yhttp_log_get(void);
extern void yhttp_log(int level, char const *str, ...);

#ifdef YHTTP_DEBUG
# define YHTTP_PRINT(level, ...) do { \
    if (LOG_ERROR <= (level) && (level) <= yhttp_log_get()) { \
        printf("<%s:%d(%s)> ", __FILE__, __LINE__, __FUNCTION__); \
            yhttp_log(level, __VA_ARGS__); \
    } \
} while (0)
#else
# define YHTTP_PRINT(level, ...) yhttp_log(level, __VA_ARGS__)
#endif

#define yhttp_error(...)   YHTTP_PRINT(LOG_ERROR,  __VA_ARGS__)
#define yhttp_warn(...)    YHTTP_PRINT(LOG_WARN,   __VA_ARGS__)
#define yhttp_info(...)    YHTTP_PRINT(LOG_INFO,   __VA_ARGS__)
#define yhttp_debug(...)   YHTTP_PRINT(LOG_DEBUG,  __VA_ARGS__)
#define yhttp_debug2(...)  YHTTP_PRINT(LOG_DEBUG2, __VA_ARGS__)

#endif
