#include <time.h>
#include <sys/time.h>

#include "http_time.h"

msec_t current_msec;
time_t current_sec;
char current_http_time[HTTP_GMT_TIME_LEN+1];

extern msec_t http_update_time()
{
    struct timeval tv;
    struct tm *gmt;
    gettimeofday(&tv, NULL);
    current_msec = 1000*tv.tv_sec + tv.tv_usec/1000;
    current_sec = tv.tv_sec;
    gmt = gmtime(&current_sec);
    snprintf(current_http_time, HTTP_GMT_TIME_LEN+1, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                WEEK[gmt->tm_wday], gmt->tm_mday,
                MONTHS[gmt->tm_mon - 1], gmt->tm_year+1900,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    return current_msec;
}

