#ifndef HTTP_TIME_H__
#define HTTP_TIME_H__

#include <time.h>

#include "common.h"

#define HTTP_GMT_TIME_LEN       SSTR_LEN("Mon, 28 Sep 1970 06:00:00 GMT")

extern msec_t current_msec;
extern time_t current_sec;
extern char current_http_time[HTTP_GMT_TIME_LEN+1];

static char const *WEEK[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char const *MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
msec_t http_update_time();

#endif

