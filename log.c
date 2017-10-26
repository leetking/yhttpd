#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "log.h"

static int level = 2;

extern void yhttp_log_set(int l)
{
    if (l < LOG_ERROR || l > LOG_DEBUG2) return;
    level = l;
}

extern void yhttp_log(int l, char const *str, ...)
{
    if (l > level || l < LOG_ERROR) return;
    char buff[] = "1970-01-01 00:00:00";
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo)
        strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", timeinfo);
    static char const *strlevel[] = {
        "[ERROR] ", "[WARN]  ",
        "[INFO]  ", "[DEBUG1]",
        "[DEBUG2]",
    };
    printf("%s ", buff);
    printf("%s ", strlevel[l]);
    va_list ap;
    va_start(ap, str);
    vprintf(str, ap);
    va_end(ap);
}

