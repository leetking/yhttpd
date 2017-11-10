#ifndef LOG_H__
#define LOG_H__

enum {
    LOG_ERROR  = 0,
    LOG_WARN   = 1,
    LOG_INFO   = 2,
    LOG_DEBUG  = 3,
    LOG_DEBUG2 = 4,
};

extern void yhttp_log_set(int level);
extern void yhttp_log(int level, char const *str, ...);
#define yhttp_error(...)   yhttp_log(LOG_ERROR,  __VA_ARGS__)
#define yhttp_warn(...)    yhttp_log(LOG_WARN,   __VA_ARGS__)
#define yhttp_info(...)    yhttp_log(LOG_INFO,   __VA_ARGS__)
#define yhttp_debug(...)   yhttp_log(LOG_DEBUG,  __VA_ARGS__)
#define yhttp_debug2(...)  yhttp_log(LOG_DEBUG2, __VA_ARGS__)

#endif
