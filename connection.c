#include <stdlib.h>

#include <unistd.h>

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


struct connection *connection_create(int fd)
{
    /* TODO 缓冲区大小也许可以通过 getsockopt 来设置 */
    int buffsize = BUFF_SIZE;
    struct connection *con = calloc(1, sizeof(*con)+2*buffsize);
    if (!con) return NULL;
    con->_buffsize = buffsize;
    con->_wrbuff = con->_rdbuff + con->_buffsize;
    con->fd = fd;
    con->_status = CON_INIT;
    con->_http_req->_ps_status = HTTP_PARSE_INIT;

    return con;
}

int connection_write(struct connection *c)
{
    if (!c) return -1;
    /* TODO finish connection_write */
    char buff[] = "HTTP/1.1 200 OK"CRLF
                  "Server: yhttpd"CRLF
                  CRLF
                  "<html><head></head><body><h1>200 OK</h1></body></html>";
    switch (c->_status) {
    case CON_REQ_FIN:
        _M(LOG_DEBUG2, "");
        write(c->fd, buff, sizeof(buff)-1);
        break;

    case CON_REQ_ERR:
        write(c->fd, err501, sizeof(err501)-1);
        c->_status = CON_CLOSE;
        break;

    default: /* do nothing */
        break;
    }

    /* write */
    return 0;
}

int connection_read(struct connection *c)
{
    if (!c) return -1;
    if (!c->_http_req) {
        c->_http_req = malloc(sizeof(struct http_request)+HTTP_METADATA_LEN);
        c->_http_req->_ps_status = 0;
        if (!c->_http_req)
            return 1;
    }

    int rdn = read(c->fd, c->_rdbuff, c->_buffsize);
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

int connection_isvalid(struct connection const *c)
{
    return (c && c->_status != CON_CLOSE);
}

void connection_destory(struct connection **c)
{
    if (!c || !*c) return;
    free((*c)->_http_req);
    close((*c)->fd);
    free(*c);

    *c = NULL;
}
