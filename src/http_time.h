#ifndef HTTP_TIME_H__
#define HTTP_TIME_H__

#include <time.h>
#include <limits.h>

#include "common.h"

#define HTTP_GMT_TIME_LEN       SSTR_LEN("Mon, 28 Sep 1970 06:00:00 GMT")
#define HTTP_TIME_MAX           INT_MAX         /* 2038 */

extern msec_t current_msec;
extern time_t current_sec;
extern char current_http_time[HTTP_GMT_TIME_LEN+1];

msec_t http_update_time();

#endif

