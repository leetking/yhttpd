#ifndef HTTP_PAGE_H__
#define HTTP_PAGE_H__

#include <time.h>
#include "common.h"


#define HTTP_SNEDFILE_MAXSIZE_CFG    (1024)
#define HTTP_PAGE_SIZE_CFG           (4*1024)
#define HTTP_LARGE_PAGE_SIZE_CFG     (16*1024)
#define HTTP_PAGE_MAX_CFG            (1024)

typedef struct http_page_t {
    uint32_t hash_value;
    time_t ctime;
    string_t name;
    string_t file;
    unsigned mime:5;
} http_page_t;

#endif
