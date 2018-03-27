#ifndef HTTP_FILE_H__
#define HTTP_FILE_H__

#include <fcntl.h>

#include "connection.h"

typedef struct http_file_t {
    struct stat stat;
    connection_t *duct;                 /* a static file or a pipe */
} http_file_t;

#endif

