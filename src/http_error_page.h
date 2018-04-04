#ifndef HTTP_SPECIAL_PAGE_H__
#define HTTP_SPECIAL_PAGE_H__

#include "http_page.h"
typedef struct http_error_page_t {
    int code;
    char code_str[4];
    string_t reason;
    http_page_t page;
} http_error_page_t;

extern int http_error_page_init(char const *dir);
extern void http_error_page_destroy();
extern http_error_page_t const *http_error_page_get(int code);
extern int http_error_code_support(int code);

#endif

