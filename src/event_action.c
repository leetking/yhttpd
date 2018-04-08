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
#include "http_wildcard.h"
#include "http_page.h"
#include "http_file.h"
#include "http_error_page.h"
#include "setting.h"
#include "fastcgi.h"

#undef B0
#undef B1
#define B0(x)   ((x)&0xff)
#define B1(x)   (B0((x)>>8))

static int connection_cnt = 0;

static char* chunk_data(char *buff, int size)
{
    static char t[] = "0123456789ABCDEF";
    /* xxxx CRLF .... CRLF */
    buff[size] = CR;
    buff[size+1] = LF;
    *(--buff) = LF;
    *(--buff) = CR;
    int i = 4;
    do {
        *(--buff) = t[size&0x0f];
        size >>= 4;
        --i;
    } while (size && i > 0);
    return buff;
}

static int hex_len(int n)
{
    int i = 0;
    while (n) {
        i ++;
        n >>= 4;
    }
    return i;
}

extern void event_accept_request(event_t *rev)
{
    connection_t *c = rev->data;
    struct sockaddr_in cip;
    socklen_t ciplen = sizeof(cip);
    int cfd;
    event_t *ev;
    
    if (connection_cnt > SETTING.vars.connection_max/SETTING.vars.worker) {
        yhttp_debug("I have too many client %d, give up this request\n", connection_cnt);
        return;
    }
    
    cfd = accept(c->fd, (struct sockaddr*)&cip, &ciplen);
    if (-1 == cfd) {
        yhttp_debug("Accept a new request error(%s)\n", strerror(errno));
        return;
    }

    ++connection_cnt;
    yhttp_debug("%d connection: %d\n", getpid(), connection_cnt);
    
    set_nonblock(cfd);
    set_nondelay(cfd);

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
    connection_event_add_now(c, EVENT_READ, ev);
    connection_read_timeout(c, current_msec+SETTING.vars.timeout);

    yhttp_debug("Accepted a new request %d tmstamp: %ld\n", c->fd, c->tmstamp);
    return; /* the end of function */

con_err:
    event_free(ev);
ev_err:
    --connection_cnt;
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
        --connection_cnt;
        return;
    }

    /* An actualy reqeust */
    r = http_request_malloc();
    if (!r) {
        yhttp_debug2("http request malloc error\n");
        connection_destroy(c);
        --connection_cnt;
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

    if (unlikely(sev->timeout)) {
        BUG_ON(sev->timeout && !sev->timeout_set);
        yhttp_debug("A client(%d)'s request timeouts\n", c->fd);
        http_request_free(r);
        connection_destroy(c);
        --connection_cnt;
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
            --connection_cnt;
            return;
        }
        if (rdn == YHTTP_BLOCK)
            return;

        /* Update read  timeout */
        connection_read_timeout(c, current_msec+SETTING.vars.timeout);
        rb->last += rdn;
        if (0 >= buffer_rest(rb)) {
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

        state = http_parse_request_head(r, r->parse_p, rb->last);
        if (unlikely(state == YHTTP_ERROR)) {
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

            http_print_request(r);
            /* url route */
            for (url_map = ser->map; url_map; url_map = url_map->next) {
                if (YHTTP_OK == http_wildcard_match(req->uri.str, req->uri.len,
                            url_map->uri.str, url_map->uri.len)) {
                    switch (url_map->type) {
                    case YHTTP_SETTING_STATIC:
                        yhttp_debug("match static router\n");
                        if (YHTTP_OK == http_check_request(r)) {
                            yhttp_debug("Http request checking 1 passed\n");
                            http_init_response(sev, url_map->setting);
                        } else {
                            yhttp_debug("Http request checking 1 can't pass\n");
                            http_init_error_page(ev);
                        }
                        return;
                        break;
                    case YHTTP_SETTING_FASTCGI:
                        yhttp_debug("match fastcgi router\n");
                        http_fastcgi_respond(ev, url_map->setting);
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
    connection_event_add_now(sc, EVENT_WRITE, sev);
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
    connection_event_add_now(sc, EVENT_WRITE, sev);
}

extern void event_respond_page(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    struct http_head_com *com = &r->hdr_com;
    http_page_t const *page = r->backend.page;
    buffer_t *wbuffer = r->res_buffer;
    int wrn;

    if (unlikely(sev->timeout)) {
        BUG_ON(sev->timeout && !sev->timeout_set);
        yhttp_debug2("client %d responsd timeout\n", sc->fd);
        goto finish_request;
    }

    for (;;) {
        /* write header */
        if (likely(wbuffer->pos < wbuffer->last)) {
            wrn = connection_write(sc->fd, wbuffer->pos, wbuffer->last);
            if (YHTTP_AGAIN == wrn)
                continue;
            if (YHTTP_BLOCK == wrn)
                return;
            if (YHTTP_ERROR == wrn) {
                yhttp_debug2("write header error\n");
                goto finish_request;
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
                connection_event_add_now(sc, EVENT_READ, sev);
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
                goto finish_request;
            }
            r->pos += wrn;
        }
    }

finish_request:
    http_request_destroy(r);
    connection_destroy(sc);
    --connection_cnt;
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

    fd = open(fname->str, O_RDONLY);
    if (unlikely(-1 == fd)) {
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
    if (unlikely(!fev)) {
        yhttp_debug2("event_malloc error\n");
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }
    fc = connection_malloc();
    if (unlikely(!fc)) {
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
    connection_event_add_now(fc, EVENT_READ, fev);
    /* connection_read_timeout(fc, current_msec+SETTING.vars.timeout); */

    http_build_response_head(r);

    /* reuse @sev as a write event */
    sev->handle = event_send_file;
    connection_event_add(sc, EVENT_WRITE, sev);
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

    if (1 == req->uri.len && '/' == req->uri.str[0])
        fname = &sta->index;

    uri_len = snprintf(uri, PATH_MAX, "%s/%.*s/%.*s",
            '/' == sta->root.str[0]? "": SETTING.cwd,
            sta->root.len, sta->root.str, fname->len, fname->str);

    yhttp_debug2("uri: %s\n", uri);
    if (-1 == stat(uri, st)) {
        yhttp_debug2("get stat url(%s) %s\n", uri, strerror(errno));
        res->code = HTTP_404;
        http_init_error_page(sev);
        return;
    }

    http_parse_file_suffix(r, uri, uri+uri_len);

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

extern void http_fastcgi_respond(event_t *sev, struct setting_fastcgi *setting_fcgi)
{
    event_t *fev;
    connection_t *fc;
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    http_fastcgi_t *fcgi = &r->backend.fcgi;

    fev = event_malloc();
    if (unlikely(!fev)) {
        yhttp_debug("event malloc error\n");
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }

    if (YHTTP_OK != http_fastcgi_init(fcgi)) {
        yhttp_debug("init fastcgi error\n");
        event_free(fev);
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }

    fc = fastcgi_connection_get(setting_fcgi);
    if (unlikely(!fc)) {
        yhttp_debug("fastcgi get a connection error\n");
        http_fastcgi_destroy(fcgi);
        event_free(fev);
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }

    r->ety_buffer = buffer_malloc(SETTING.vars.buffer_size);
    if (unlikely(!r->ety_buffer)) {
        yhttp_debug("alloc memory for body buffer error\n");
        connection_destroy(fc);
        http_fastcgi_destroy(fcgi);
        event_free(fev);
        res->code = HTTP_500;
        http_init_error_page(sev);
        return;
    }

    r->hdr_buffer->pos = r->parse_p;
    r->pos = buffer_len(r->hdr_buffer);
    r->last = com->content_length;
    if (r->pos >= r->last)
        sc->rd_eof = 1;

    /* begin request, params and part reqeust body */
    http_fastcgi_build(r);

    fcgi->fastcgi_con = fc; /* the network connection cant find me through @fcgi->fastcgi_con */
    fc->data = sc;          /* hold my peer(the network connection) */
    fev->data = fc;
    fev->handle = event_send_to_fcgi;
    connection_event_add_now(fc, EVENT_WRITE, fev);

    /* switch to read request body */
    sev->handle = event_read_request_body;
    connection_event_add(sc, EVENT_READ, sev);
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

    for (;;) {
        char *end = b->last+MIN(buffer_rest(b), r->last - r->pos);
        int rdn = connection_read(fc->fd, b->last, end);
        if (YHTTP_AGAIN == rdn)
            continue;
        if (unlikely(YHTTP_ERROR == rdn)) {
            yhttp_debug2("read file error YHTTP_ERROR: %s\n", strerror(errno));
            connection_destroy(fc);
            sev = connection_event_del(fc, EVENT_WRITE);
            res->code = HTTP_500;
            http_init_error_page(sev);
            return;
        }
        if (YHTTP_BLOCK == rdn || 0 == buffer_rest(b)) {
            BUG_ON(b->pos >= b->last);
            yhttp_debug2("read file(%d)\n", b->last - b->pos);
            connection_pause(fc, EVENT_READ);
            connection_revert(sc, EVENT_WRITE);
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

    for (;;) {
        int wrn = connection_write(sc->fd, b->pos, b->last);
        if (YHTTP_AGAIN == wrn)
            continue;
        if (YHTTP_BLOCK == wrn) {
            yhttp_debug2("write return YHTTP_BLOCK\n");
            return;
        }
        if (unlikely(YHTTP_ERROR == wrn)) {
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
                connection_event_add_now(sc, EVENT_READ, sev);
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
    --connection_cnt;
}

extern void event_send_to_fcgi(event_t *fev)
{
    connection_t *fc = fev->data;
    connection_t *sc = fc->data;
    http_request_t *r = sc->data;
    buffer_t *b = r->ety_buffer;

    /* if it is empty */
    BUG_ON(b->pos >= b->last);

    for (;;) {
        int wrn = connection_write(fc->fd, b->pos, b->last);

        if (YHTTP_AGAIN == wrn)
            continue;
        if (YHTTP_BLOCK == wrn) {
            yhttp_debug2("write fastcgi return YHTTP_BLOCK\n");
            return;
        }
        if (unlikely(YHTTP_ERROR == wrn)) {
            yhttp_debug2("write fastcgi error YHTTP_ERROR\n");
            http_fastcgi_t *fcgi = &r->backend.fcgi;
            http_fastcgi_destroy(fcgi);
            http_request_destroy(r);
            connection_destroy(sc);
            connection_destroy(fc);
            --connection_cnt;
            return;
        }
        b->pos += wrn;

        if (b->pos == b->last) {
            yhttp_debug2("drain over the write buffer\n");
            if (sc->rd_eof) {
                yhttp_debug2("Client %d request body finish\n", fc->fd);
                event_t *sev = connection_event_del(sc, EVENT_READ);
                BUG_ON(sev->data != sc);
                sev->handle = event_send_fcgi_to_client;
                connection_event_add(sc, EVENT_WRITE, sev);

                fev = connection_event_del(fc, EVENT_WRITE);
                BUG_ON(fev->data != fc);
                fev->handle = event_read_fcgi_packet_hdr;
                connection_event_add_now(fc, EVENT_READ, fev);
                connection_read_timeout(fc, current_msec+SETTING.vars.timeout);
                return;
            }
            buffer_init(b);
            connection_pause(fc, EVENT_WRITE);
            connection_revert(sc, EVENT_READ);
            connection_read_timeout(sc, current_msec+SETTING.vars.timeout);
            return;
        }
    }
}

extern void event_read_request_body(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    connection_t *fc = fcgi->fastcgi_con;
    buffer_t *b = r->ety_buffer;
    FCGI_Header *fcgi_hdr = (FCGI_Header*)b->last;
    b->last += FCGI_HEADER_LEN;     /* reserve space of header */

    /* NOTE As FCGI_PADDING_MAXLEN is 8, we can ... */
    /* there are least two fastcgi header and padding */
    BUG_ON(((buffer_rest(b) - FCGI_HEADER_LEN)>>3) <= 0);
    BUG_ON(r->pos >= r->last);

    for (;;) {
        int maxrdn = MIN((buffer_rest(b) - FCGI_HEADER_LEN) & ~0x07 , FCGI_RECORD_MAXLEN);
        maxrdn = MIN(maxrdn, r->last - r->pos);
        int rdn = connection_read(sc->fd, b->last, b->last + maxrdn);
        if (rdn == YHTTP_AGAIN)
            continue;
        if (unlikely(rdn == YHTTP_ERROR)) {
            yhttp_debug2("read request body error\n");
            http_fastcgi_t *fcgi = &r->backend.fcgi;
            http_fastcgi_destroy(fcgi);
            http_request_destroy(r);
            connection_destroy(sc);
            connection_destroy(fc);
            --connection_cnt;
            return;
        }
        /* read until fill the buffer */
        if (rdn == YHTTP_BLOCK) {
            yhttp_debug2("read fastcgi return YHTTP_BLOCK\n");
            return;
        }
        b->last += rdn;
        r->pos += rdn;

        /* read finish or buffer is full */
        if (r->pos >= r->last || buffer_rest(b) == FCGI_HEADER_LEN) {
            yhttp_debug2("read body (%d)", buffer_len(b));
            fcgi_hdr->version = FCGI_VERSION_1;
            fcgi_hdr->type = FCGI_STDIN;
            fcgi_hdr->requsetIdB0 = B0(fcgi->request_id);
            fcgi_hdr->requsetIdB1 = B1(fcgi->request_id);
            fcgi_hdr->contentLengthB0 = B0(buffer_len(b));
            fcgi_hdr->contentLengthB1 = B1(buffer_len(b));
            fcgi_hdr->paddingLength = (~buffer_len(b) + 1) & 0x07;
            b->last += fcgi_hdr->paddingLength;
            if (r->pos >= r->last) {
                sc->rd_eof = 1;
                fcgi_hdr = (FCGI_Header*)b->last;
                fcgi_hdr->version = FCGI_VERSION_1;
                fcgi_hdr->type = FCGI_STDIN;
                fcgi_hdr->requsetIdB0 = B0(fcgi->request_id);
                fcgi_hdr->requsetIdB1 = B1(fcgi->request_id);
                fcgi_hdr->contentLengthB0 = 0;
                fcgi_hdr->contentLengthB1 = 0;
                fcgi_hdr->paddingLength = 0;
                b->last += FCGI_HEADER_LEN;
                sev = connection_event_del(sc, EVENT_READ);
                sev->handle = event_send_to_fcgi;
                connection_event_add(sc, EVENT_WRITE, sev);
            } else {
                connection_pause(sc, EVENT_READ);
            }
            connection_revert(fc, EVENT_WRITE);
            return;
        }
    }
}

extern void event_read_fcgi_packet_hdr(event_t *fev)
{
    connection_t *fc = fev->data;
    connection_t *sc = fc->data;
    http_request_t *r = sc->data;
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    struct http_head_res *res = &r->hdr_res;
    buffer_t *b = fcgi->res_buffer;
    FCGI_Header *hdr;
    int rdn;
    int rdmax;

    if (fev->timeout) {
        BUG_ON(fev->timeout && !fev->timeout_set);
        yhttp_debug2("read fcgi time out\n");
        connection_destroy(fc);
        connection_destroy(sc);
        http_fastcgi_destroy(fcgi);
        http_request_destroy(r);
        --connection_cnt;
        return;
    }

    for (;;) {
        rdmax = MIN(buffer_rest(b), FCGI_HEADER_LEN - fcgi->hdr_pos);
        rdn = connection_read(fc->fd, b->last, b->last + rdmax);
        if (rdn == YHTTP_AGAIN)
            continue;
        if (rdn == YHTTP_ERROR) {
            event_t *sev;
            yhttp_debug2("read fcgi error or finish\n");
            connection_destroy(fc);
            http_fastcgi_destroy(fcgi);
            sev = connection_event_del(sc, EVENT_WRITE);
            res->code = HTTP_500;
            http_init_error_page(sev);
            return;
        }
        if (rdn == YHTTP_BLOCK) {
            yhttp_debug2("read fastcgi packet header return YHTTP_BLOCK\n");
            return;
        }
        b->last += rdn;
        fcgi->hdr_pos += rdn;

        if (fcgi->hdr_pos >= FCGI_HEADER_LEN) {
            hdr = (FCGI_Header*)(b->last - FCGI_HEADER_LEN);
            fcgi->hdr_pos = 0;
            fcgi->block_pos = 0;
            fcgi->block_size = (hdr->contentLengthB1<<8) | hdr->contentLengthB0;
            fcgi->block_padding_size = hdr->paddingLength;
            /* roll back, remove header */
            b->last -= FCGI_HEADER_LEN;

            if (hdr->type == FCGI_END_REQUEST) {
                fc->rd_eof = 1;
                *r->res_buffer->last++ = '0';   /* append '0CRLFCRLF' */
                *r->res_buffer->last++ = CR;
                *r->res_buffer->last++ = LF;
                *r->res_buffer->last++ = CR;
                *r->res_buffer->last++ = LF;
                fev = connection_event_del(fc, EVENT_READ);
                BUG_ON(fev->data != fc);
                event_free(fev);
                connection_revert(sc, EVENT_WRITE);
                return;
            }

            /* read over */
            if (hdr->type == FCGI_STDOUT && fcgi->block_size > 0) {
                if (fcgi->read_body)
                    r->res_buffer->last += 4+2;     /* 0xffff+CRLF */
                fcgi->read_padding = 0;
                fev->handle = event_read_fcgi_packet_bdy;
                event_read_fcgi_packet_bdy(fev);
                return;
            }
        }
    }
}

extern void event_read_fcgi_packet_bdy(event_t *fev)
{
    connection_t *fc = fev->data;
    connection_t *sc = fc->data;
    http_request_t *r = sc->data;
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    struct http_head_res *res = &r->hdr_res;
    struct http_head_com *com = &r->hdr_com;
    buffer_t *b = (fcgi->read_body)? r->res_buffer: fcgi->res_buffer;
    int rdn;
    int maxrd;
    int reserve;

    for (;;) {
        reserve = fcgi->read_body? 2: FCGI_HEADER_LEN;
        maxrd = fcgi->block_size - fcgi->block_pos + fcgi->block_padding_size;
        maxrd = MIN(maxrd, buffer_rest(b) - reserve);
        rdn = connection_read(fc->fd, b->last, b->last + maxrd);
        if (rdn == YHTTP_AGAIN)
            continue;
        if (rdn == YHTTP_ERROR) {
            yhttp_debug2("read fcgi packet body error or finish\n");
            connection_destroy(fc);
            http_fastcgi_destroy(fcgi);
            connection_destroy(sc);
            http_request_destroy(r);
            --connection_cnt;
            return;
        }
        if (rdn == YHTTP_BLOCK) {
            yhttp_debug2("read fastcgi packet body return YHTTP_BLOCK\n");
            return;
        }

        b->last += rdn;
        fcgi->block_pos += rdn;

        if (fcgi->read_padding) {
            if (fcgi->block_pos >= fcgi->block_size + fcgi->block_padding_size) {
                fcgi->read_padding = 0;
                fev->handle = event_read_fcgi_packet_hdr;
            }
            return;
        }
        /* we have to interrupt read */
        if (fcgi->block_pos >= fcgi->block_size || reserve == buffer_rest(b)) {
            if (fcgi->block_pos == fcgi->block_size + fcgi->block_padding_size) {
                fcgi->read_padding = 0;
                fev->handle = event_read_fcgi_packet_hdr;
            } else if (fcgi->block_pos >= fcgi->block_size) {
                fcgi->read_padding = 1;
            }
            /* remove padding */
            if (fcgi->block_pos > fcgi->block_size)
                b->last = b->last - (fcgi->block_pos - fcgi->block_size);

            if (fcgi->read_body) {
                b->pos = chunk_data(b->pos+4+2, buffer_len(b)-4-2);
                b->last += 2;
                connection_pause(fc, EVENT_READ);
                connection_revert(sc, EVENT_WRITE);
            } else {
                int status = http_fastcgi_parse_response(r, b->pos, b->last);
                if (unlikely(YHTTP_ERROR == status
                            || (YHTTP_AGAIN == status && reserve == buffer_rest(b)))) {
                    yhttp_debug2("parse response error\n");
                    event_t *sev;
                    connection_destroy(fc);
                    http_fastcgi_destroy(fcgi);
                    sev = connection_event_del(sc, EVENT_WRITE);
                    res->code = HTTP_500;
                    http_init_error_page(sev);
                } else if (YHTTP_OK == status) {
                    com->transfer_encoding = HTTP_CHUNKED;
                    com->last_modified = current_sec;

                    http_build_response_head(r);
                    http_fastcgi_build_extend_head(r);
                    b->pos = fcgi->parse_p;
                    if (buffer_len(b) > 0) {
                        r->res_buffer->last += (hex_len(buffer_len(b))+2);
                        buffer_copy(r->res_buffer, b);
                        chunk_data(r->res_buffer->last - buffer_len(b), buffer_len(b));
                        r->res_buffer->last += 2;
                    }
                    fcgi->read_body = 1;
                    connection_pause(fc, EVENT_READ);
                    connection_revert(sc, EVENT_WRITE);
                }
            }
            return;
        }
    }
}

extern void event_send_fcgi_to_client(event_t *sev)
{
    connection_t *sc = sev->data;
    http_request_t *r = sc->data;
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    connection_t *fc = fcgi->fastcgi_con;
    buffer_t *b = r->res_buffer;

    BUG_ON(buffer_len(b) == 0);

    for (;;) {
        int wrn = connection_write(sc->fd, b->pos, b->last);
        if (YHTTP_AGAIN == wrn)
            continue;

        if (YHTTP_ERROR == wrn) {
            yhttp_debug2("send res_buffer error\n");
            connection_destroy(fc);
            connection_destroy(sc);
            http_fastcgi_destroy(fcgi);
            http_request_destroy(r);
            --connection_cnt;
            return;
        }

        if (YHTTP_BLOCK == wrn) {
            yhttp_debug2("send res_buffer return YHTTP_BLOCK\n");
            connection_pause(sc, EVENT_WRITE);
            connection_revert(fc, EVENT_READ);
            return;
        }

        b->pos += wrn;

        if (b->pos == b->last) {
            yhttp_debug2("drain res_buffer buffer over\n");
            if (fc->rd_eof) {
                yhttp_debug2("send res_buffer over\n");
                http_fastcgi_destroy(fcgi);
                http_request_destroy(r);
                connection_destroy(fc);
                connection_destroy(sc);
                --connection_cnt;
                return;
            }
            connection_pause(sc, EVENT_WRITE);
            connection_revert(fc, EVENT_READ);
            return;
        }
    }
}

