#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "log.h"

static int level = LOG_INFO;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

extern void yhttp_log_set(int l)
{
    if (l < LOG_ERROR || l > LOG_DEBUG2) return;
    level = l;
}

extern void yhttp_log(int l, char const *str, ...)
{
    if (l > level || l < LOG_ERROR) return;
    char buff[] = "1970-01-01 00:00:00.0000";
    struct timespec rawtime = {0, 0};
    struct tm *timeinfo;
    int ret;
    ret = clock_gettime(CLOCK_REALTIME, &rawtime);
    timeinfo = localtime(&rawtime.tv_sec);
    if (0 == ret)
        strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", timeinfo);
    static char const *strlevel[] = {
        "[ERROR] ", "[WARN]  ",
        "[INFO]  ", "[DEBUG1]",
        "[DEBUG2]",
    };
    va_list ap;
    va_start(ap, str);

    pthread_mutex_lock(&lock);
    printf("%s.%04d %s", buff, (int)(rawtime.tv_nsec/1e9 * 1e4), strlevel[l]);
    vprintf(str, ap);
    pthread_mutex_unlock(&lock);

    va_end(ap);
}

