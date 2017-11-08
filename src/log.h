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

#endif
