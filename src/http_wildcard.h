#ifndef HTTP_WILDCARD_H__
#define HTTP_WILDCARD_H__

#include <stdio.h>

#define HTTP_WILDCARD_MAXDEPTH   (10)

int http_wildcard_match(char const *url, ssize_t urln, char const *pat,
        ssize_t patn);

#endif

