#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/limits.h>

#include "common.h"
#include "http.h"
#include "connection.h"
#include "ring-buffer.h"

enum con_status {
    CON_INIT,
    CON_REQ_CTU,    /* 解析没有结束接着解析 */
    CON_REQ_FIN,    /* 一次请求接受完毕 */
    CON_REQ_NXT,
    CON_CLOSE,      /* 关闭这个连接 */
};

/* TODO replace with linux kernel ring buffer */
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
    con->fdro = con->fdwo = -1;
    con->_req_status = HTTP_REQ_START;
    con->_res_status = HTTP_RES_NOP;
    con->_con_status = CON_INIT;
    con->_last_req = time(NULL);
    //con->timeout = TIMEOUT;
    con->timeout = -1;

    return con;
}

static void response_prepare(struct connection *c)
{
    if (HTTP_200 != c->_http->status_code) {
        c->_res_status = HTTP_RES_TRANSFER;
        return;
    }

    /* TODO Check file or execute cgi */
    char uri[PATH_MAX];
    string_t uriline = c->_http->lines[HTTP_URI];
    if (1 == uriline.len && '/' == uriline.str[0])
        snprintf(uri, PATH_MAX, "%s/index.html", root_path);
    else
        snprintf(uri, PATH_MAX, "%s%.*s", root_path, uriline.len, uriline.str);
    _M(LOG_DEBUG2, "full path: %s\n", uri);
    struct stat st;
    if (-1 == stat(uri, &st)) {
        _M(LOG_DEBUG2, "stat %s: %s\n", uri, strerror(errno));
        c->_res_status = HTTP_RES_TRANSFER;
        c->_res_status = HTTP_404;
        return;
    }

    /* file can execute, run as cgi */
    if (st.st_mode & (S_IXUSR | S_IXGRP)) {
        /* TODO finish cgi */
    } else if (st.st_mode & (S_IRUSR | S_IRGRP)) {
        int fd = open(uri, O_RDONLY);
        if (-1 == fd) {
            _M(LOG_DEBUG2, "open %s: %s\n", uri, strerror(errno));
            /* server: I don't kown what happen. */
            c->_res_status = HTTP_RES_TRANSFER;
            c->_res_status = HTTP_404;
            return;
        }
        c->fdro = fd;
        c->_res_status = HTTP_RES_TRANSFER;
        _M(LOG_DEBUG2, "open %s: success %d\n", uri, fd);
    } else {
        /* no permission to open file */
        _M(LOG_DEBUG2, "response_transfer no permission open file.\n");
        c->_res_status = HTTP_RES_TRANSFER;
        c->_res_status = HTTP_404;
    }
}
static void response_transfer(struct connection *c)
{
    int w = ringbuffer_write(c->sktfd, c->_wrbuff, c->_buffsize, &c->_wri, &c->_wrn);
    _M(LOG_DEBUG2, "response_transfer write: %d\n", w);
    if (w <= 0 && (c->_res_status == HTTP_RES_READ_FIN1)) {
        /* finish */
        c->_con_status = CON_CLOSE;
        c->_req_status = HTTP_REQ_FINISH;
    }
}

extern int connection_write_skt(struct connection *c)
{
    if (!c) return -1;
    switch (c->_res_status) {
    case HTTP_RES_START:
        response_prepare(c);
        break;

    case HTTP_RES_READ_FIN1:
    case HTTP_RES_TRANSFER:
        response_transfer(c);
        break;

    case HTTP_RES_FINISH:
        c->_res_status = HTTP_RES_NOP;
        break;

    case HTTP_RES_NOP:
    default: /* do nothing */
        break;
    }

    /* write */
    return 0;
}

static void request_transfer(struct connection *c)
{
}

extern int connection_read_skt(struct connection *c)
{
    if (!c) return -1;
    if (!c->_http) {
        c->_http = http_head_malloc(HTTP_METADATA_LEN);
        if (!c->_http)
            return 1;
    }
    _M(LOG_DEBUG2, "connection_read_skt req_status: %d\n", c->_req_status);

    int rdn;
    switch (c->_req_status) {
    case HTTP_REQ_START:
        rdn = read_s(c->sktfd, c->_rdbuff, c->_buffsize);
        _M(LOG_DEBUG2, "read skt: %.*s\n", rdn, c->_rdbuff);
        if (rdn <= 0) {
            c->_req_status = CON_CLOSE;
            return 0;
        }
        if (0 == http_head_parse(c->_http, c->_rdbuff, rdn)) {
            if (c->_http->method == HTTP_POST)
                c->_req_status = HTTP_REQ_TRANSFER;
            else {
                c->_req_status = HTTP_REQ_NOP;
            }
            c->_res_status = HTTP_RES_START;
        }

        break;

    case HTTP_REQ_TRANSFER:
        request_transfer(c);
        break;

    case HTTP_REQ_NOP:
    default:
        break;
    }

    return 0;
}

/**
 * read from nrmfd, wirte to wrbuff
 */
extern int connection_read_file(struct connection *c)
{
    if (!c || -1 == c->fdro) return -1;
    int rdn = ringbuffer_read(c->fdro, c->_wrbuff, c->_buffsize, &c->_wri, &c->_wrn);
    _M(LOG_DEBUG2, "connection_read_file read: %d\n", rdn);

    if (rdn == 0) {
        c->_res_status = HTTP_RES_READ_FIN1;
    }
    return 0;
}

/**
 * read from rdbuff, write to nrmfd
 */
extern int connection_write_file(struct connection *c)
{
    /* TODO POST can use chunk encoding? */
    if (!c || -1 == c->fdwo) return -1;

    int wrn = ringbuffer_write(c->fdwo, c->_rdbuff, c->_buffsize, &c->_rdi, &c->_rdn);

    if (wrn > 0)
        return 0;
    return 1;
}

extern int connection_isvalid(struct connection const *c)
{
    time_t now = time(NULL);
    return !(!c || c->_con_status == CON_CLOSE ||
            ((c->timeout != -1) && c->timeout < difftime(now, c->_last_req)));
}

extern void connection_destory(struct connection **c)
{
    if (!c || !*c) return;
    http_head_free(&(*c)->_http);
    close((*c)->sktfd);
    if (-1 != (*c)->fdro) close((*c)->fdro);
    if (-1 != (*c)->fdwo) close((*c)->fdwo);
    free(*c);

    *c = NULL;
}

extern int connection_settimeout(struct connection *c, int timeout)
{
    if (!c) return -1;
    int ret = c->timeout;
    c->timeout = timeout;

    return ret;
}

extern int connection_need_write_skt(struct connection *c)
{
    if ((c->_res_status == HTTP_RES_TRANSFER
            || c->_res_status == HTTP_RES_READ_FIN1)
            && (c->_wri == c->_wrn))
        return 0;
    return 1;
}
extern int connection_need_write_file(struct connection *c)
{
    return (c->_rdi != c->_rdn);
}
