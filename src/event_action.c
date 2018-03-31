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
#include "http_wildcard.h"
#include "http_page.h"
#include "http_file.h"
#include "http_error_page.h"
#include "setting.h"
#include "fastcgi.h"

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
    ev->accept = 0;
    ev->data = c;
    ev->handle = event_init_http_request;

    c->fd = cfd;
    connection_event_add(c, EVENT_READ, ev);
    connection_read_timeout(c, current_msec+SETTING.vars.timeout);

    yhttp_debug("Accepted a new request %d tmstamp: %ld\n", c->fd, c->tmstamp);
    return; /* the end of function */

con_err:
    event_free(ev);
ev_err:
    close(cfd);
    yhttp_info("Accept a new request error\n");
}

/**
 * wait for a read event of the new reqeust and init this reqeust structure
 */
extern void event_init_http_request(event_t *rev)
{
    connection_t *c = rev->data;
    http_request_t *r;

    if (rev->timeout) {
        BUG_ON(rev->timeout && !rev->timeout_set);
        yhttp_debug("A client(%d)'s request timeouts\n", c->fd);
        connection_destroy(c);
        return;
    }

    /* An actualy reqeust */
    r = http_request_malloc();
    if (!r) {
        yhttp_debug2("http request malloc error\n");
        connection_destroy(c);
        return;
    }

    http_request_init(r);

    /* switch to event_parse_http_head() */
    rev->handle = event_parse_http_head;
    c->data = r;
    event_parse_http_head(rev);
}

/**
 * parse request head util finish or an error happens
 */
extern void event_parse_http_head(event_t *sev)
{
    connection_t *c = sev->data;
    http_request_t *r = c->data;
    struct http_head_res *res = &r->hdr_res;
    buffer_t *rb  = r->hdr_buffer;
    ssize_t rdn;
    int state;

    if (sev->timeout) {
        BUG_ON(sev->timeout && !sev->timeout_set);
        yhttp_debug("A client(%d)'s request timeouts\n", c->fd);
        http_request_free(r);
        connection_destroy(c);
        return;
    }

    for (;;) {
        rdn = connection_read(c->fd, rb->last, rb->end);
        if (rdn == YHTTP_AGAIN)
            continue;
        if (rdn == YHTTP_ERROR) {
            yhttp_debug2("read error or finish\n");
            http_request_free(r);
            connection_destroy(c);
            return;
        }
        if (rdn == YHTTP_BLOCK)
            return;

        /* Update read  timeout */
        connection_read_timeout(c, current_msec+SETTING.vars.timeout);
        rb->last += rdn;
        if (rb->last >= rb->end) {
            yhttp_info("Http request extend header buffer\n");
            int s = http_request_extend_hdr_buffer(r);
            if (YHTTP_ERROR == s) {
                sev = connection_event_del(c, EVENT_READ);
                BUG_ON(sev->data != c);
                res->code = HTTP_500;
                http_init_error_page(sev);
                return;
            }
            if (YHTTP_FAILE == s) {
                res->code = HTTP_413;
                http_init_error_page(sev);
                return;
            }
            rb = r->hdr_buffer;
        }

        state = http_parse_request_head(r, r->parse_pos, rb->last);
        if (state == YHTTP_ERROR) {
            yhttp_debug("Http parse request error\n");
            sev = connection_event_del(c, EVENT_READ);
            BUG_ON(sev->data != c);
            http_init_error_page(sev);
            return;
        }

        if (state == YHTTP_OK) {
            struct setting_server_map *url_map;
            struct http_head_req *req = &r->hdr_req;
            struct http_head_com *com = &r->hdr_com;
            struct http_head_res *res = &r->hdr_res;
            struct setting_server *ser = &SETTING.server;
            event_t *ev = connection_event_del(c, EVENT_READ);
            BUG_ON(ev->data != c);
            
            if (HTTP11 == com->version
                    && (req->port != ser->port
                        || YHTTP_OK != http_wildcard_match(req->host.str, req->host.len,
                            ser->host.str, ser->host.len))) {
                yhttp_debug("Http invalid host\n");
                res->code = HTTP_400;
                http_init_error_page(ev);
                return;
            }

            /* url route */
            for (url_map = ser->map; url_map; url_map = url_map->next) {
                if (YHTTP_OK == http_wildcard_match(req->uri.str, req->uri.len,
                            url_map->uri.str, url_map->uri.len)) {
                    switch (url_map->type) {
                    case YHTTP_SETTING_STATIC:
                        yhttp_debug("match static router\n");
                        if (YHTTP_OK == http_check_request(r)) {
                            yhttp_debug("Http request checking 1 passed\n");
                            http_init_response(sev, ser->map->setting);
                        } else {
                            yhttp_debug("Http request checking 1 can't pass\n");
                            http_init_error_page(ev);
                        }
                        return;
                        break;
                    case YHTTP_SETTING_FASTCGI:
                        yhttp_debug("match fastcgi router\n");
                        http_fastcgi_respond(ev, ser->map->setting);
                        return;
                        break;
                    default:
                        BUG_ON("Unregister setting type");
                        break;
                    }
                }
            }

            /* Cant match url */
            yhttp_debug("Http cant match url\n");
            res->code = HTTP_404;
            http_init_error_page(ev);
            return;
        }
    }
}

extern void http_init_special_response(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;

    r->backend.page = NULL;

    buffer_init(r->res_buffer);
    http_build_response_head(r);

    sev->handle = event_respond_page;
    connection_event_add(sc, EVENT_WRITE, sev);
}

extern void http_init_error_page(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    http_error_page_t const *errpage = http_error_page_get(res->code);
    http_page_t const *page = &errpage->page;

    if (com->version != HTTP11 || com->version != HTTP10)
        com->version = HTTP10;

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

    buffer_init(r->res_buffer);
    http_build_response_head(r);

    sev->handle = event_respond_page;
    connection_event_add(sc, EVENT_WRITE, sev);
}

extern void event_respond_page(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    struct http_head_req *req = &r->hdr_req;
    struct http_head_res *res = &r->hdr_res;
    struct http_head_com *com = &r->hdr_com;
    http_page_t const *page = r->backend.page;
    buffer_t *wbuffer = r->res_buffer;
    int wrn;

    if (sev->timeout) {
        BUG_ON(sev->timeout && !sev->timeout_set);
        yhttp_debug2("client %d responsd timeout\n", sc->fd);
        goto finish_request;
    }

    for (;;) {
        /* write header */
        if (wbuffer->pos < wbuffer->last) {
            wrn = connection_write(sc->fd, wbuffer->pos, wbuffer->last);
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
                yhttp_debug("Client %d responsd over\n", sc->fd);

                /* Connection: close */
                if (!com->connection) {
                    yhttp_debug("Client %d close\n", sc->fd);
                    goto finish_request;
                }

                yhttp_debug("Client %d reuse\n", sc->fd);
                http_request_reuse(r);

                /* reuse @sev as a read event for the next request on this connection */
                sev = connection_event_del(sc, EVENT_WRITE);
                sev->handle = event_parse_http_head;
                connection_event_add(sc, EVENT_READ, sev);
                connection_read_timeout(sc, current_msec+SETTING.vars.timeout);
                return;
            }

            wrn = connection_write(sc->fd, page->file.str+r->pos, page->file.str+r->last);
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

finish_request:
    http_request_destroy(r);
    connection_destroy(sc);
}

void http_init_static_file(event_t *sev, string_t const *fname)
{
    int fd;
    event_t *fev;
    connection_t *fc;
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    struct http_head_res *res = &r->hdr_res;
    struct http_head_com *com = &r->hdr_com;
    http_file_t *file = &r->backend.file;

    BUG_ON(r->backend_type != HTTP_REQUEST_BACKEND_FILE);

    fd = open(fname->str, O_RDONLY);
    if (-1 == fd) {
        yhttp_debug2("open %s error: %s\n", fname->str, strerror(errno));
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }
    if (-1 == lseek(fd, com->content_range1, SEEK_SET)) {
        yhttp_debug2("lseek %s %d error: %s\n", fname->str, com->content_range1, strerror(errno));
        res->code = HTTP_500;
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
    file->duct = fc;    /* the network connection cant find me through @file->duct */

    fc->fd = fd;
    fc->data = sc;       /* hold my peer(the network connection) */
    fev->data = fc;
    fev->handle = event_read_file;
    connection_event_add(fc, EVENT_READ, fev);

    http_build_response_head(r);

    /* reuse @sev as a write event */
    sev->handle = event_send_file;
    connection_event_add(sc, EVENT_WRITE, sev);
    connection_pause(sc, EVENT_WRITE);
}

extern void http_init_response(event_t *sev, struct setting_static *sta)
{
    char uri[PATH_MAX];
    int uri_len;
    string_t path;
    connection_t *c = sev->data;
    http_request_t *r = c->data;
    struct http_head_req *req = &r->hdr_req;
    struct http_head_res *res = &r->hdr_res;
    http_file_t *file = &r->backend.file;
    struct stat *st = &file->stat;
    string_t const *fname = &req->uri;

    r->backend_type = HTTP_REQUEST_BACKEND_FILE;

    if (1 == req->uri.len && '/' == req->uri.str[0])
        fname = &sta->index;

    uri_len = snprintf(uri, PATH_MAX, "%.*s/%.*s",
            sta->root.len, sta->root.str, fname->len, fname->str);

    yhttp_debug2("uri: %s\n", uri);
    if (-1 == stat(uri, st)) {
        yhttp_debug2("get stat url(%s) %s\n", uri, strerror(errno));
        res->code = HTTP_404;
        http_init_error_page(sev);
        return;
    }

    path.str = uri;
    path.len = uri_len;
    http_dispatcher_t dp = http_dispatch(r);
    switch (dp) {
    case HTTP_DISPATCHER(HTTP_GET, HTTP_200):
    case HTTP_DISPATCHER(HTTP_GET, HTTP_206):
        yhttp_debug("Open nornaml static file\n");
        http_init_static_file(sev, &path);
        break;

    case HTTP_DISPATCHER(HTTP_GET, HTTP_304):
    case HTTP_DISPATCHER(HTTP_HEAD, HTTP_304):
    case HTTP_DISPATCHER(HTTP_HEAD, HTTP_200):
        yhttp_debug("Init special response\n");
        http_init_special_response(sev);
        break;

    default:
        http_init_error_page(sev);
        break;
    }
}

extern void http_fastcgi_respond(event_t *rev, struct setting_fastcgi *setting_fcgi)
{
    connection_t *rc = rev->data;
    http_request_t *r = rc->data;
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    int need_body = HTTP_POST|HTTP_PUT;
    int read_body = 0;

    event_t *wev = event_malloc();
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    connection_t *wc = fastcgi_connection_get(setting_fcgi);
    /* TODO CONTINUE fastcgi process malloc failure */
    if (YHTTP_OK != http_fastcgi_init(fcgi)) {
        res->code = HTTP_403;
        http_init_error_page(rev);
        return;
    }

    if (req->method & need_body) {
        r->bdy_buffer = buffer_malloc(YHTTP_BUFFER_SIZE_CFG);
        if (!r->bdy_buffer) {
            res->code = HTTP_500;
            http_init_error_page(rev);
            return;
        }
        /* copy the rest part of @r->hdr_buffer */
        r->hdr_buffer->pos = r->parse_pos;
        buffer_copy(r->bdy_buffer, r->hdr_buffer);

        /* TODO reqeust support chuncked */
        if (buffer_len(r->bdy_buffer) < com->content_length)
            read_body = 1;
    }

    /* begin request, params and part reqeust body */
    http_fastcgi_build_th(r);

    fcgi->requst_con = rc;
    wev->handle = event_send_to_fcgi;
    wev->data = wc;
    wc->data = fcgi;
    event_add(wev, EVENT_WRITE);
    if (read_body) {
        rev->handle = event_read_request_body;
        event_del_timer(rev);               /* update timer */
        rc->tmstamp = current_msec+SETTING.vars.timeout;
        event_add_timer(rev);
    } else {
        event_del(rev, EVENT_READ);
    }
}

extern void event_read_file(event_t *fev)
{
    event_t *sev;
    connection_t *fc = fev->data;
    connection_t *sc = fc->data;
    http_request_t *r = sc->data;
    struct http_head_res *res = &r->hdr_res;
    buffer_t *b = r->res_buffer;

    BUG_ON(NULL == sc->data);
    BUG_ON(NULL == b);
    BUG_ON(NULL == fev);
    BUG_ON(r->backend_type != HTTP_REQUEST_BACKEND_FILE);

    for (;;) {
        char *end = b->last+MIN(b->end - b->last, r->last - r->pos);
        int rdn = connection_read(fc->fd, b->last, end);
        if (YHTTP_AGAIN == rdn)
            continue;
        if (YHTTP_BLOCK == rdn || b->last == b->end) {
            if (b->pos < b->last) {
                yhttp_debug2("read file(%d)\n", b->last - b->pos);
                connection_pause(fc, EVENT_READ);
                connection_revert(sc, EVENT_WRITE);
            }
            return;
        }
        if (YHTTP_ERROR == rdn) {
            yhttp_debug2("read file error YHTTP_ERROR: %s\n", strerror(errno));
            connection_destroy(fc);
            sev = connection_event_del(fc, EVENT_WRITE);
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
            fev = connection_event_del(fc, EVENT_READ);
            BUG_ON(fev->data != fc);
            event_free(fev);
            connection_revert(sc, EVENT_WRITE);
            return;
        }
    }
}

extern void event_send_file(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    connection_t *fc = r->backend.file.duct;
    struct http_head_com *com = &r->hdr_com;
    buffer_t *b = r->res_buffer;

    /* no data */
    BUG_ON(b->pos >= b->last);
    BUG_ON(r->backend_type != HTTP_REQUEST_BACKEND_FILE);

    for (;;) {
        int wrn = connection_write(sc->fd, b->pos, b->last);
        if (YHTTP_AGAIN == wrn)
            continue;
        if (YHTTP_BLOCK == wrn) {
            yhttp_debug2("write return YHTTP_BLOCK\n");
            return;
        }
        if (YHTTP_ERROR == wrn) {
            yhttp_debug2("write file error YHTTP_ERROR\n");
            goto finish_request;
        }

        b->pos += wrn;
        if (b->pos == b->last) {
            yhttp_debug2("drain write buffer over\n");
            if (fc->rd_eof) {
                yhttp_debug("Client %d reqeust file over\n", sc->fd);
                if (!com->connection) {
                    yhttp_debug("Client %d close connection\n", sc->fd);
                    goto finish_request;
                }

                yhttp_debug("Client %d request reuse\n", sc->fd);
                connection_destroy(fc);
                http_request_reuse(r);

                /* reuse @sev as a read event for the next request on this connection */
                sev = connection_event_del(sc, EVENT_WRITE);
                sev->handle = event_parse_http_head;
                connection_event_add(sc, EVENT_READ, sev);
                connection_read_timeout(sc, current_msec+SETTING.vars.timeout);
                return;
            }

            buffer_init(b);
            connection_pause(sc, EVENT_WRITE);
            connection_revert(fc, EVENT_READ);
            return;
        }
    }
    return;

finish_request:
    connection_destroy(fc);
    http_request_destroy(r);
    connection_destroy(sc);
}

extern void event_read_request_body(event_t *rev)
{
}


extern void event_send_to_fcgi(event_t *wev)
{
    connection_t *wc = wev->data;
    http_fastcgi_t *fcgi = wc->data;
    connection_t *rc = fcgi->requst_con;
    buffer_t *b = fcgi->req_buffer;

    BUG_ON(b->pos >= b->last);

    for (;;) {
        int wrn = connection_write(wc->fd, b->pos, b->last);

        if (YHTTP_AGAIN == wrn)
            continue;
        if (YHTTP_BLOCK == wrn) {
            yhttp_debug2("write return YHTTP_BLOCK\n");
            return;
        }
        if (YHTTP_ERROR == wrn) {
            yhttp_debug2("write fcgi error YHTTP_ERROR\n");
            goto read_err;
        }
        b->pos += wrn;

        if (b->pos == b->last) {
            yhttp_debug2("drain over the write buffer\n");
            if (rc->rd_eof) {
                yhttp_debug2("Client %d request body finish\n", rc->fd);

                wev->handle = event_read_fcgi;
                wc->tmstamp = current_msec+SETTING.vars.timeout;
                event_add_timer(wev);
                event_del(wev, EVENT_WRITE);
                event_add(wev, EVENT_READ);
                return;
            }
            buffer_init(b);
            return;
        }
    }
    return;

read_err:
    return;
}

extern void event_read_fcgi(event_t *rev)
{
    connection_t *rc = rev->data;
    http_fastcgi_t *fcgi = rc->data;
    connection_t *wc = fcgi->requst_con;
    http_request_t *r = wc->data;

    if (rev->timeout) {
        BUG_ON(rev->timeout && !rev->timeout_set);
        yhttp_debug2("read fcgi time out\n");
        goto free_fcgi;
    }

    for (;;) {
    }
    return;

free_fcgi:
    event_del(rev, EVENT_READ);
    event_del_timer(rev);
    close(rc->fd);
    connection_free(rc);
    event_free(rev);
    http_request_free(r);
    buffer_free(fcgi->req_buffer);
    buffer_free(fcgi->res_buffer);
    connection_free(wc);
    return;
}

