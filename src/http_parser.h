#ifndef HTTP_PARSE_H__
#define HTTP_PARSE_H__

#include "http.h"

int http_parse_request_header(http_request_t *req, char const *start, char const *end);

#endif

