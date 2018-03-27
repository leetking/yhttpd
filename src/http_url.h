#ifndef HTTP_URL_H__
#define HTTP_URL_H__

#include <stdio.h>

#define HTTP_URL_MAXDEPTH   (10)

int http_url_match(char const *url, ssize_t urln, char const *pat, ssize_t patn);

#endif

