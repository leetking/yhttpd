#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "common.h"
#include "event.h"
#include "log.h"
#include "connection.h"
#include "event_action.h"
#include "http.h"
#include "http_parser.h"
#include "setting.h"
#include "http_time.h"
#include "http_mime.h"
#include "http_url.h"
#include "http_page.h"
#include "http_file.h"
#include "http_error_page.h"

extern void event_accept_request(event_t *rev)
{
    connection_t *c = rev->data;
    struct sockaddr_in cip;
    socklen_t ciplen = sizeof(cip);
    int cfd;
    event_t *ev;
    
    cfd = accept(c->fd, (struct sockaddr*)&cip, &ciplen);
    if (-1 == cfd) {
        yhttp_debug("Accept a new request error(%s)\n", strerror(errno));
        return;
    }
    
    set_nonblock(cfd);

    ev = event_malloc();
    if (!ev) {
        yhttp_debug2("event_malloc error\n");
        goto ev_err;
    }
    c = connection_malloc();
    if (!c) {
        yhttp_debug2("connection_malloc error\n");
        goto con_err;
    }
    c->fd = cfd;
    c->rev = ev;
    c->read = connection_read;
    c->tmstamp = current_msec+TIMEOUT_CFG;

    ev->accept = 0;
    ev->data = c;
    ev->handle = event_init_http_request;

    event_add_timer(ev);
    event_add(ev, EVENT_READ);
    yhttp_debug("Accepted a new request %d tmstamp: %ld\n", c->fd, c->tmstamp);
    return; /* the end of function */

con_err:
    event_free(ev);
ev_err:
    close(cfd);
    yhttp_debug("Accept a new request error\n");
}

/**
 * wait for a read event of the new reqeust and init this reqeust structure
 */
extern void event_init_http_request(event_t *rev)
{
    connection_t *c = rev->data;
    http_request_t *req;

    if (rev->timeout) {
        BUG_ON(rev->timeout && !rev->timeout_set);
        yhttp_debug("A client(%d)'s request timeouts\n", c->fd);
        goto timeout;
    }

    /* An actualy reqeust */
    req = http_request_malloc();
    if (!req) {
        yhttp_debug2("http request malloc error\n");
        goto timeout;
    }

    /* switch to event_parse_http_head() */
    rev->handle = event_parse_http_head;
    req->parse_pos = req->request_head->pos;
    req->parse_state = HTTP_PARSE_INIT;
    req->request_head_large = 0;
    req->req.if_modified_since = 0;     /* 1970-1-1 00:00:00 */
    req->req.range1 = 0;
    req->req.range2 = INT_MAX;          /* indicate infinity */
    req->req.if_none_match.str = NULL;
    req->req.if_none_match.len = 0;
    c->data = req;

    event_parse_http_head(rev);
    return; /* the end of function */

timeout:
    event_del(rev, EVENT_READ);
    event_del_timer(rev);
    close(c->fd);
    event_free(rev);
    connection_free(c);
}

/**
 * parse request head util finish or an error happens
 */
extern void event_parse_http_head(event_t *rev)
{
    connection_t *c = rev->data;
    http_request_t *req = c->data;
    buffer_t *rbuffer = req->request_head;

    if (rev->timeout) {
        BUG_ON(rev->timeout && !rev->timeout_set);
        yhttp_debug("A client(%d)'s request timeouts\n", c->fd);
        goto free_request;
    }

    for (;;) {
        ssize_t rdn;
        int state;

        rdn = c->read(c->fd, rbuffer->last, rbuffer->end);
        if (rdn == YHTTP_ERROR) {
            yhttp_debug2("read error or finish\n");
            goto free_request;
        }
        if (rdn == YHTTP_AGAIN)
            break;
        event_del_timer(rev);               /* update timer */
        c->tmstamp = current_msec+TIMEOUT_CFG;
        event_add_timer(rev);
        if (rdn == YHTTP_BLOCK)
            return;

        rbuffer->last += rdn;
        if (rbuffer->last >= rbuffer->end) {
            buffer_t *b;
            size_t pos_offset;
            if (req->request_head_large) {
                yhttp_debug("http request header is too large\n");
                req->res.code = HTTP_413;
                http_init_error_page(rev);
                return;
            }
            b = buffer_malloc(HTTP_LARGE_BUFFER_SIZE_CFG);
            if (!b) {
                yhttp_debug("allocate memory for http_large_buffer error\n");
                req->res.code = HTTP_500;
                http_init_error_page(rev);
                return;
            }
            yhttp_debug("Allocate memory for http_large_buffer\n");

            BUG_ON(NULL == req->parse_pos);
            BUG_ON(req->parse_pos < rbuffer->pos);

            req->request_head_large = 1;
            pos_offset = req->parse_pos - rbuffer->pos;
            buffer_copy(b, req->request_head);
            buffer_free(req->request_head);
            rbuffer = req->request_head = b;
            req->parse_pos = rbuffer->pos + pos_offset;
        }

        state = http_parse_request_head(req, req->parse_pos, rbuffer->last);
        if (state == YHTTP_ERROR) {
            yhttp_debug("Http parse request error\n");
            http_init_error_page(rev);
            return;
        }

        if (state == YHTTP_OK) {
            struct http_head_req *r = &req->req;
            yhttp_debug("Http parse reqeust finish successfully\n");
            if (SETTING.enable_fcgi
                    && (YHTTP_OK == http_url_match(r->uri.str, r->uri.len,
                        SETTING.fcgi_pattern.str, SETTING.fcgi_pattern.len))) {
                yhttp_debug("forward to fcgi\n");
                /* TODO finish forward FastCGI */
            }

            if (YHTTP_OK == http_check_request(req)) {
                yhttp_debug("Http request checking 1 passed\n");
                http_init_response(rev);
            } else {
                yhttp_debug("Http request checking 1 can't pass\n");
                http_init_error_page(rev);
            }
            return;
        }
        /* YHTTP_AGAIN continue */
    }
    return;

free_request:
    event_del(rev, EVENT_READ);
    event_del_timer(rev);
    http_request_free(req);
    close(c->fd);
    event_free(rev);
    connection_free(c);
}

extern void http_init_special_response(event_t *sev)
{
    connection_t *c = sev->data;
    http_request_t *r = c->data;
    struct http_head_com *com = &r->com;
    struct http_head_res *res = &r->res;
    http_error_page_t const *errpage = http_error_page_get(res->code);

    r->backend.page = NULL;

    buffer_init(r->response_buffer);
    http_build_response_head(r);

    c->rev = NULL;
    event_del(sev, EVENT_READ);
    c->wev = sev;
    c->write = connection_write;
    sev->handle = event_respond_page;
    event_add(sev, EVENT_WRITE);
}

extern void http_init_error_page(event_t *sev)
{
    connection_t *c = sev->data;
    http_request_t *r = c->data;
    struct http_head_com *com = &r->com;
    struct http_head_res *res = &r->res;
    http_error_page_t const *errpage = http_error_page_get(res->code);
    http_page_t const *page = &errpage->page;

    com->pragma = HTTP_PRAGMA_NO_CACHE;
    com->cache_control = HTTP_CACHE_CONTROL_NO_STORE;
    com->content_encoding = HTTP_IDENTITY;
    com->transfer_encoding = HTTP_UNCHUNKED;
    com->content_length = page->file.len;
    com->content_type = page->mime;
    com->connection = 0;
    res->etag[0] = '\0';
    r->pos = 0;
    r->last = com->content_length;
    r->backend.page = page;

    buffer_init(r->response_buffer);
    http_build_response_head(r);

    c->rev = NULL;
    event_del(sev, EVENT_READ);
    c->wev = sev;
    c->write = connection_write;
    sev->handle = event_respond_page;
    event_add(sev, EVENT_WRITE);
}

extern void event_respond_page(event_t *sev)
{
    connection_t *c = sev->data;
    http_request_t *r = c->data;
    http_page_t const *page = r->backend.page;
    buffer_t *wbuffer = r->response_buffer;
    int wrn;

    if (sev->timeout) {
        BUG_ON(sev->timeout && !sev->timeout_set);
        yhttp_debug2("client %d responsd timeout\n", c->fd);
        goto free_request;
    }

    for (;;) {
        /* write header */
        if (wbuffer->pos < wbuffer->last) {
            wrn = c->write(c->fd, wbuffer->pos, wbuffer->last);
            if (YHTTP_AGAIN == wrn)
                continue;
            if (YHTTP_BLOCK == wrn)
                return;
            if (YHTTP_ERROR == wrn) {
                yhttp_debug2("write header error\n");
                break;
            }
            wbuffer->pos += wrn;

        /* write from cache */
        } else {
            /* !page such as the HEAD request, the all is writed over */
            if (!page || r->pos >= r->last) {
                yhttp_debug("Client %d responsd over\n", c->fd);

                /* Connection: close */
                if (!r->com.connection) {
                    yhttp_debug("Client %d close\n", c->fd);
                    goto free_request;
                }

                yhttp_debug("Client %d reuse\n", c->fd);
                http_request_reuse(r);
                if (sev->timeout_set)
                    event_del_timer(sev);
                c->tmstamp = current_msec+TIMEOUT_CFG;
                event_add_timer(sev);
                event_del(sev, EVENT_WRITE);
                sev->handle = event_parse_http_head;
                event_add(sev, EVENT_READ);
                return;
            }

            wrn = c->write(c->fd, page->file.str+r->pos, page->file.str+r->last);
            if (YHTTP_AGAIN == wrn)
                continue;
            if (YHTTP_BLOCK == wrn)
                return;
            if (YHTTP_ERROR == wrn) {
                yhttp_debug2("write content error\n");
                break;
            }
            r->pos += wrn;
        }
    }

free_request:
    event_del(sev, EVENT_WRITE);
    if (sev->timeout_set)
        event_del_timer(sev);
    http_request_free(r);
    close(c->fd);
    event_free(sev);
    connection_free(c);
}

void http_init_static_file(event_t *sev, string_t const *fname)
{
    int fd;
    event_t *fev;
    connection_t *fc;
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    struct http_head_res *res = &r->res;
    struct http_head_com *com = &r->com;
    http_file_t *file = &r->backend.file;

    fd = open(fname->str, O_RDONLY);
    if (-1 == fd) {
        yhttp_debug2("open %s error: %s\n", fname->str, strerror(errno));
        r->res.code = HTTP_500;
        http_init_error_page(sev);
        return;
    }
    if (-1 == lseek(fd, com->content_range1, SEEK_SET)) {
        yhttp_debug2("lseek %s %d error: %s\n", fname->str, com->content_range1, strerror(errno));
        r->res.code = HTTP_500;
        http_init_error_page(sev);
        return;
    }

    fev = event_malloc();
    if (!fev) {
        yhttp_debug2("event_malloc error\n");
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }
    fc = connection_malloc();
    if (!fc) {
        event_free(fev);
        yhttp_debug2("event_malloc error\n");
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }
    fc->fd = fd;
    fc->data = sc;       /* the network connection */
    fc->read = connection_read;
    fev->data = fc;

    file->duct = fc;

    fc->rev = fev;
    fev->handle = event_read_file;
    event_add(fev, EVENT_READ);

    http_build_response_head(r);

    /* pause read event on the socket fd */
    sc->rev = NULL;
    event_del(sev, EVENT_READ);
    event_del_timer(sev);
    sc->wev = sev;
    sc->write = connection_write;
    sev->handle = event_send_file;
    return;
}

extern void http_init_response(event_t *ev)
{
    connection_t *c = ev->data;
    http_request_t *req = c->data;
    http_file_t *file = &req->backend.file;
    struct stat *st = &file->stat;
    char uri[PATH_MAX];
    int uri_len;
    string_t fname;

    if (1 == req->req.uri.len && '/' == req->req.uri.str[0]) {
        uri_len = snprintf(uri, PATH_MAX, "%.*s/index.html",
                SETTING.root_path.len, SETTING.root_path.str);
    } else {
        uri_len = snprintf(uri, PATH_MAX, "%.*s%.*s",
                SETTING.root_path.len, SETTING.root_path.str,
                req->req.uri.len, req->req.uri.str);
    }

    yhttp_debug2("uri: %s\n", uri);
    if (-1 == stat(uri, st)) {
        yhttp_debug2("get stat url(%s) %s\n", uri, strerror(errno));
        req->res.code = HTTP_404;
        http_init_error_page(ev);
        return;
    }

    fname.str = uri;
    fname.len = uri_len;
    http_dispatcher_t dp = http_dispatch(req);
    switch (dp) {
    case HTTP_DISPATCHER(HTTP_GET, HTTP_200):
    case HTTP_DISPATCHER(HTTP_GET, HTTP_206):
        yhttp_debug("Open nornaml static file\n");
        http_init_static_file(ev, &fname);
        break;

    case HTTP_DISPATCHER(HTTP_GET, HTTP_304):
    case HTTP_DISPATCHER(HTTP_HEAD, HTTP_304):
    case HTTP_DISPATCHER(HTTP_HEAD, HTTP_200):
        http_init_special_response(ev);
        break;

    default:
        http_init_error_page(ev);
        break;
    }
}

extern void event_read_file(event_t *fev)
{
    connection_t *fc = fev->data;
    connection_t *sc = fc->data;
    event_t *sev = sc->wev;
    http_request_t *r = sc->data;
    buffer_t *b = r->response_buffer;
    http_file_t *file = &r->backend.file;
    struct http_head_com *com = &r->com;
    struct http_head_res *res = &r->res;

    BUG_ON(NULL == sev);
    BUG_ON(NULL == sev->data);
    BUG_ON(NULL == sc->data);
    BUG_ON(NULL == b);
    BUG_ON(NULL == fev);

    for (;;) {
        char *end = b->last+MIN(b->end - b->last, r->last - r->pos);
        int rdn = fc->read(fc->fd, b->last, end);
        if (YHTTP_AGAIN == rdn)
            continue;
        if (YHTTP_BLOCK == rdn || b->last == b->end) {
            if (b->pos < b->last) {
                yhttp_debug2("read file(%d)\n", b->last - b->pos);
                event_add(sev, EVENT_WRITE);
                event_del(fev, EVENT_READ);
            }
            return;
        }
        if (YHTTP_ERROR == rdn) {
            yhttp_debug2("read file error YHTTP_ERROR: %s\n", strerror(errno));
            event_del(fev, EVENT_READ);
            close(fc->fd);
            event_free(fev);
            connection_free(fc);
            res->code = HTTP_500;
            http_init_error_page(sev);
            return;
        }

        b->last += rdn;
        r->pos += rdn;

        /* read finish */
        if (r->pos == r->last) {
            fc->rd_eof = 1;
            yhttp_debug2("read finish\n");
            event_del(fev, EVENT_READ);
            event_add(sev, EVENT_WRITE);
            return;
        }
    }
}

extern void event_send_file(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *req = sc->data;
    connection_t *fc = req->backend.file.duct;
    event_t *fev = fc->rev;
    buffer_t *b = req->response_buffer;

    /* no data */
    BUG_ON(b->pos >= b->last);

    for (;;) {
        int wrn = sc->write(sc->fd, b->pos, b->last);
        if (YHTTP_AGAIN == wrn)
            continue;
        if (YHTTP_BLOCK == wrn) {
            yhttp_debug2("write return YHTTP_BLOCK\n");
            return;
        }
        if (YHTTP_ERROR == wrn) {
            yhttp_debug2("write file error YHTTP_ERROR\n");
            goto free_request;
        }

        b->pos += wrn;

        if (b->pos == b->last) {
            yhttp_debug2("drain buffer over\n");
            if (fc->rd_eof) {
                yhttp_debug("Client %d reqeust file finish\n", sc->fd);
                if (!req->com.connection) {
                    yhttp_debug("Client %d close connection\n", sc->fd);
                    goto free_request;
                }

                yhttp_debug("Client %d request reuse\n", sc->fd);
                event_free(fev);
                connection_free(fc);
                http_request_reuse(req);
                sc->tmstamp = current_msec+TIMEOUT_CFG;
                event_add_timer(sev);
                event_del(sev, EVENT_WRITE);
                sev->handle = event_parse_http_head;
                event_add(sev, EVENT_READ);
                return;
            }

            buffer_init(b);
            event_add(fev, EVENT_READ);
            event_del(sev, EVENT_WRITE);
            return;
        }
    }
    return;

free_request:
    /* close file */
    close(fc->fd);
    event_free(fev);
    connection_free(fc);
    /* close network connection */
    event_del(sev, EVENT_WRITE);
    http_request_free(req);
    close(sc->fd);
    event_free(sev);
    connection_free(sc);
}

