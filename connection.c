#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/limits.h>

#include "common.h"
#include "http.h"
#include "connection.h"

enum con_status {
    CON_INIT,
    CON_REQ_CTU,    /* 解析没有结束接着解析 */
    CON_REQ_FIN,    /* 一次请求接受完毕 */
    CON_REQ_ERR,    /* 请求格式不符合 HTTP 协议 */
    CON_CLOSE,      /* 关闭这个连接 */
};
static char const err404[] = "";
static char const err501[] = "";

/* FIXME replace with linux kernel ring buffer */
#define next(idx, size)     (((idx)+1)%(size))
#define nextn(idx, n, size) (((idx)+(n))%(size))

extern struct connection *connection_create(int sktfd)
{
    /* TODO 缓冲区大小也许可以通过 getsockopt 来设置 */
    int buffsize = BUFF_SIZE;
    struct connection *con = calloc(1, sizeof(*con)+2*buffsize);
    if (!con) return NULL;
    con->_buffsize = buffsize;
    con->_wrbuff = con->_rdbuff + con->_buffsize;
    con->sktfd = sktfd;
    con->nrmfd = -1;
    con->_status = CON_INIT;

    return con;
}

static int check_nrm_file(struct connection *c)
{
    _M(LOG_DEBUG2, "method: %d\nuri: %.*s\nhost: %.*s\n",
            c->_http_req->method,
            c->_http_req->lines[HTTP_URI].len, c->_http_req->lines[HTTP_URI].str,
            c->_http_req->lines[HTTP_HOST].len, c->_http_req->lines[HTTP_HOST].str);
    char path[PATH_MAX] = ".";
    strncat(path, c->_http_req->lines[HTTP_URI].str, c->_http_req->lines[HTTP_URI].len);

    _M(LOG_DEBUG2, "full request path: %s\n", path);
    /* index.html */
    if ('/' == path[1]) {
        write(c->sktfd, index, sizeof(index)-1);
    }
    c->nrmfd = open(path, O_RDONLY);
    if (-1 == c->nrmfd) {
        _M(LOG_DEBUG2, "check_nrm_file: %s", strerror(errno));
    }
    return 0;
}

static int connection_response(struct connection *c)
{
    char head[] = "HTTP/1.1 200 OK"CRLF
                   "Server: yhttpd"CRLF
                   CRLF;

    if (-1 != write(c->sktfd, head, sizeof(head)-1)) {
        _M(LOG_DEBUG2, "response 200 OK\n");
        return 0;
    }

    /* TODO connection 分离传输环形缓冲区 */

    c->_status = CON_CLOSE;

    return 0;
}

extern int connection_write(struct connection *c)
{
    if (!c) return -1;
    switch (c->_status) {
    case CON_REQ_FIN:
        check_nrm_file(c);
        connection_response(c);
        break;

    case CON_REQ_ERR:
        write(c->sktfd, err501, sizeof(err501)-1);
        c->_status = CON_CLOSE;
        break;

    default: /* do nothing */
        break;
    }

    /* write */
    return 0;
}

extern int connection_read(struct connection *c)
{
    if (!c) return -1;
    if (!c->_http_req) {
        c->_http_req = http_request_malloc(HTTP_METADATA_LEN);
        if (!c->_http_req)
            return 1;
    }

    int rdn = read(c->sktfd, c->_rdbuff, c->_buffsize);
    /* TODO 处理中断和错误 */

    switch (http_request_parse(c->_http_req, c->_rdbuff, rdn)) {
        /* 请求完成 */
    case HTTP_REQ_FINISH:
        c->_status = CON_REQ_FIN;
        break;
    case HTTP_REQ_INVALID:
        c->_status = CON_REQ_ERR;
        break;
        /* TODO other error! */
    }

    return 0;
}

/**
 * read from nrmfd, wirte to wrbuff
 */
extern int connection_read_nrm(struct connection *c)
{
    if (!c || -1 == c->nrmfd) return -1;
    /* wrbuff is full */
    if (next(c->_wrn, c->_buffsize) == c->_wri)
        return 0;

    int rdn;
    /* #: pad
     * .: empty
     * buffer: |#######wrn.......idx##########| */
    if (c->_wrn < c->_wri) {
        rdn = read(c->nrmfd, c->_wrbuff+c->_wrn, c->_wri - c->_wrn - 1);
        if (rdn >= 0)
            c->_wrn += rdn;

        /* buffer: |.......idx#######wrn..........| */
    } else {
        /* only write part */
        rdn = read(c->nrmfd, c->_wrbuff+c->_wrn, c->_buffsize - c->_wrn + 1);
        if (rdn >= 0)
            c->_wrn = nextn(c->_wrn, rdn, c->_buffsize);
    }

    return 0;
}

/**
 * read from rdbuff, write to nrmfd
 */
extern int connection_write_nrm(struct connection *c)
{
    if (!c) return -1;
    /* rdbuff is empty */
    if (c->_rdi == c->_rdn)
        return 0;

    int wrn;
    if (c->_rdi < c->_rdn) {
        wrn = write(c->nrmfd, c->_wrbuff + c->_wri, c->_rdn - c->_rdi -1);
        if (wrn >= 0)
            c->_rdi += wrn;
    } else {
        wrn = write(c->nrmfd, c->_wrbuff + c->_wri, c->_buffsize - c->_rdi);
        if (wrn >= 0)
            c->_rdi = nextn(c->_rdi, wrn, c->_buffsize);
    }

    return 0;
}

extern int connection_isvalid(struct connection const *c)
{
    return (c && c->_status != CON_CLOSE);
}

extern void connection_destory(struct connection **c)
{
    if (!c || !*c) return;
    http_request_free(&(*c)->_http_req);
    close((*c)->sktfd);
    if (-1 != (*c)->nrmfd)
        close((*c)->nrmfd);
    free(*c);

    *c = NULL;
}
