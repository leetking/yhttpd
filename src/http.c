#include <stdlib.h>

#include "memory.h"
#include "http.h"

extern http_request_t *http_request_new(int fd)
{
    http_request_t *req;
    req = yhttp_malloc(sizeof(*req));
    if (!req)
        return NULL;

    req->con_req.fd = fd;
    //req->con_req.read = ;

    return req;
}

extern void http_request_destroy(http_request_t **req)
{
}

