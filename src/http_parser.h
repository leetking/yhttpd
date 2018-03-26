#ifndef HTTP_PARSE_H__
#define HTTP_PARSE_H__

#include "http.h"

#define HTTP_PARSE_INIT 0

int http_parse_request_head(http_request_t *req, char *start, char *end);

int  url_decode_char(char const *chr);
void url_incode_char(int chr, char *out, int len);

#endif

