#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/ip.h>

#include <linux/limits.h>

#include "common.h"
#include "http.h"
#include "connection.h"
#include "ring-buffer.h"
#include "event.h"

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

static void read_from_normalfd(struct connection *c);
static void write_to_normalfd(struct connection *c);
static void read_request_content(struct connection *c);
static void respond_header(struct connection *c);
static void prepare_response(struct connection *c);
static void write_to_socket(struct connection *c);

/**
 * SOME THINGS OF BUFFERS WHICH YOU SHOULD KNWON
 *
 * sktfd --read--> [ rdbuff ] --write-> fdwo
 * sktfd <-write-- [ wrbuff ] <--read-- fdro
 *
 * Figure 1
 */

/**
 * write to socket (respond)
 */
static void write_to_socket(struct connection *c)
{
    if (ringbuffer_empty(c->wrbuff) && c->socket.wrreg) {
        c->socket.wrreg = 0;
        event_del(c->socket.fd, EVENT_WRITE);
        return;
    }

    int wrn = ringbuffer_write(c->socket.fd, c->wrbuff);
    if (wrn <= 0) {
        c->socket.wreof = 1;
        event_del(c->socket.fd, EVENT_WRITE);
        return;
    }

    c->socket.wrn += wrn;
    _M(LOG_DEBUG2, "write_to_socket(): wrn: %d wrmax: %d\n", c->socket.wrn, c->socket.wrmax);
    if (c->socket.wrn == c->socket.wrmax) {
        c->socket.wrn = c->socket.wrmax = 0;
        c->socket.wrreg = 0;
        c->socket.wreof = 1;
        event_del(c->socket.fd, EVENT_WRITE);
        if (connection_a_tx(c)) {
            if (c->http->iscon) {
                _M(LOG_DEBUG2, "write_to_socket(): a new request maybe.\n");
                /* A new request maybe */
                c->socket.rdeof = 0;
                event_add(c->socket.fd, EVENT_READ, (event_fun_t*)parse_request, c);
            } else {
                _M(LOG_DEBUG2, "write_to_socket(): finish a transcation.\n");
                connection_destory(&c);
            }
        }
    }
}

/**
 * read from the normal fd which maybe is pipe or regular file
 */
static void read_from_normalfd(struct connection *c)
{
    /* Ref: Figure 1 */
    if (ringbuffer_full(c->wrbuff) && c->normal.rdreg) {
        c->normal.rdreg = 0;
        event_del(c->normal.fdro, EVENT_READ);
        return;
    }
    int rdn = ringbuffer_read(c->normal.fdro, c->wrbuff);
    _M(LOG_DEBUG2, "read_from_normalfd(): rdn: %d\n", rdn);

    if (rdn <= 0) {
        c->normal.rdeof = 1;
        event_del(c->normal.fdro, EVENT_READ);
        close(c->normal.fdro);
        c->normal.fdro = -1;
    } else {
        c->normal.rdn += rdn;
        if (c->normal.rdn == c->normal.rdmax) {
            _M(LOG_DEBUG2, "read_from_normalfd(): read %d over\n", c->normal.fdro);
            c->normal.rdn = c->normal.rdmax = 0;
            c->normal.rdeof = 1;
            event_del(c->normal.fdro, EVENT_READ);
            close(c->normal.fdro);
            c->normal.fdro = -1;
        }
    }

    /* register write event to socket */
    if (!ringbuffer_empty(c->wrbuff) && !c->socket.wrreg) {
        _M(LOG_DEBUG2, "read_from_normalfd(): register write_to_socket\n");
        c->socket.wreof = 0;
        c->socket.wrreg = 1;
        event_add(c->socket.fd, EVENT_WRITE, (event_fun_t*)write_to_socket, c);
    }
}

/**
 * write the content from rdbuff to the input of cgi
 */
static void write_to_normalfd(struct connection *c)
{
}


/**
 * read the content from socket
 */
static void read_request_content(struct connection *c)
{
    /* full, not need read */
    if (ringbuffer_full(c->rdbuff) && c->socket.wrreg) {
        c->socket.wrreg = 0;
        event_del(c->socket.fd, EVENT_READ);
        return;
    }

    int rdn = ringbuffer_read(c->socket.fd, c->rdbuff);
    /* finish */
    if (rdn == 0) {
        event_del(c->socket.fd, EVENT_READ);
        c->socket.rdeof = 1;
        return;
    }

    /* error */
    if (rdn < 0) {
        return;
    }

    if (!ringbuffer_empty(c->rdbuff) && !c->normal.wrreg) {
        c->normal.wrreg = 1;
        event_add(c->normal.fdwo, EVENT_WRITE, (event_fun_t*)write_to_normalfd, c);
        return;
    }
}
static void respond_header(struct connection *c)
{
    /* Ref: Figure 1 */
    char header[] = "HTTP/1.1 200 OK"CRLF
        "Connection: Keey-Alive" CRLF
        "Content-Type: text/html"CRLF
        "Content-Length: %d" CRLF
        "Server: yhttpd/"VER CRLF
        CRLF;
    char buff[BUFF_SIZE];
    int len = snprintf(buff, BUFF_SIZE, header, c->http->filesize);
    /* TODO complete respond_header() */
    ringbuffer_pad(c->wrbuff, (uint8_t*)buff, len);

    c->socket.wreof = 0;
    c->socket.wrn = 0;
    c->socket.wrmax = len+c->http->filesize;
    _M(LOG_DEBUG2, "respond_header(): wrn:%d wrmax: %d\n", c->socket.wrn, c->socket.wrmax);
    event_add(c->socket.fd, EVENT_WRITE, (event_fun_t*)write_to_socket, c);
}

/**
 * parse request
 */
extern void parse_request(struct connection *c)
{
    uint8_t buff[BUFF_SIZE];
    int rdn = read_s(c->socket.fd, buff, BUFF_SIZE);
    _M(LOG_DEBUG2, "parse_request(): read: %.*s\n", rdn, buff);

    if (rdn <= 0) {
        _M(LOG_DEBUG2, "parse_request(): read over.\n");
        c->socket.rdeof = 1;
        event_del(c->socket.fd, EVENT_READ);
        if (connection_a_tx(c)) {
            _M(LOG_INFO, "parse_request(): Finish a transcation.\n");
            connection_destory(&c);
        }
        return;
    }

    int parse_status = http_head_parse(c->http, buff, rdn);

    /* http_head_parse finish */
    if (0 == parse_status) {
        if (c->http->method == HTTP_POST) {
            /* change to read_request_content */
            event_del(c->socket.fd, EVENT_READ);
            event_add(c->socket.fd, EVENT_READ, (event_fun_t*)read_request_content, c);
            return;
        }

        /* method is GET */
        prepare_response(c);
        event_del(c->socket.fd, EVENT_READ);
        c->socket.rdeof = 1;

        respond_header(c);

        c->normal.rdeof = 0;
        c->normal.rdn = 0;
        c->normal.rdmax = c->http->filesize;
        event_add(c->normal.fdro, EVENT_READ, (event_fun_t*)read_from_normalfd, c);
    }
}

extern struct connection *connection_create(int sktfd)
{
    struct connection *con = malloc(sizeof(*con));
    if (!con) return NULL;
    con->socket.fd = sktfd;
    con->socket.rdeof = con->socket.wreof = 1;
    con->normal.rdeof = con->normal.wreof = 1;

    con->socket.rdn = con->socket.rdmax = 0;
    con->socket.wrn = con->socket.wrmax = 0;
    con->normal.rdn = con->normal.rdmax = 0;
    con->normal.wrn = con->normal.wrmax = 0;

    con->normal.fdro = con->normal.fdwo = -1;
    con->wrbuff = ringbuffer_create(BUFF_SIZE);
    con->rdbuff = ringbuffer_create(BUFF_SIZE);
    con->http = http_head_malloc(HTTP_METADATA_LEN);

    return con;
}

extern void connection_destory(struct connection **c)
{
    if (!c || !*c) return;
    if ((*c)->http) http_head_free(&(*c)->http);
    if (-1 != (*c)->socket.fd)   close((*c)->socket.fd);
    if (-1 != (*c)->normal.fdro) close((*c)->normal.fdro);
    if (-1 != (*c)->normal.fdwo) close((*c)->normal.fdwo);
    if ((*c)->wrbuff) ringbuffer_destory(&(*c)->wrbuff);
    if ((*c)->rdbuff) ringbuffer_destory(&(*c)->rdbuff);
    free(*c);

    *c = NULL;
}

extern int connection_a_tx(struct connection *c)
{
    return (c->socket.rdeof && c->socket.wreof)
        && (c->normal.rdeof && c->normal.wreof);
}

static void prepare_response(struct connection *c)
{
    /* TODO Check file or execute cgi */
    char uri[PATH_MAX];
    string_t uriline = c->http->lines[HTTP_URI];
    if (1 == uriline.len && '/' == uriline.str[0])
        snprintf(uri, PATH_MAX, "%s/index.html", root_path);
    else
        snprintf(uri, PATH_MAX, "%s%.*s", root_path, uriline.len, uriline.str);
    _M(LOG_DEBUG2, "full path: %s\n", uri);
    struct stat st;
    if (-1 == stat(uri, &st)) {
        _M(LOG_DEBUG2, "stat %s: %s\n", uri, strerror(errno));
        c->http->status_code = HTTP_404;
        return;
    }

    /* file can execute, run as cgi */
    if (st.st_mode & (S_IXUSR | S_IXGRP)) {
        /* TODO finish cgi */
    } else if (st.st_mode & (S_IRUSR | S_IRGRP)) {
        int fd = open(uri, O_RDONLY);
        if (-1 == fd) {
            _M(LOG_DEBUG2, "open %s: %s\n", uri, strerror(errno));
            /* TODO server: inner error 5xx */
            return;
        }
        c->normal.fdro = fd;
        c->http->filesize = st.st_size;
        _M(LOG_DEBUG2, "open %s: success %d\n", uri, fd);
    } else {
        /* no permission to open file */
        _M(LOG_DEBUG2, "response_transfer no permission open file.\n");
        c->http->status_code = HTTP_403;
    }
}

