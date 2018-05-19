#include <errno.h>
#include <ctype.h>

#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "http.h"
#include "buffer.h"
#include "fastcgi.h"
#include "log.h"
#include "http_mime.h"
#include "http_error_page.h"
#include "common.h"

#undef B0
#undef B1
#undef B2
#undef B3
#define B0(x)   ((x)&0xff)
#define B1(x)   (B0((x)>>8))
#define B2(x)   (B0((x)>>16))
#define B3(x)   (B0((x)>>24))

static struct env {
    struct sockaddr_storage skaddr;
    int solved;
} ENV;

string_t http_mimes[] = {
    string_null,
    string_newstr("text/html"),
    string_newstr("text/css"),
    string_newstr("text/x-c"),
    string_newstr("text/javascript"),
    string_newstr("font/tff"),
    string_newstr("font/woff"),
    string_newstr("application/xml"),
    string_newstr("image/jpeg"),
    string_newstr("image/png"),
    string_newstr("audio/mpeg3"),
    string_newstr("audio/ogg"),         /* oga */
    string_newstr("video/ogg"),         /* ogv */
    string_newstr("text/plain"),                /* default textual stream */
    string_newstr("application/x-www-form-urlencoded"),
    string_newstr("application/octet-stream"),  /* default binary stream */
};

enum {
    HDR_STATUS,
    HDR_SET_COOKIE,
    HDR_SERVER,
    HDR_CONTENT_TYPE,
    HDR_CONTENT_LENGTH,
};

static int get_tcp_connection(string_t *host, int16_t port)
{
    int skt;
    struct sockaddr_in *sip = (struct sockaddr_in*)&ENV.skaddr;

    if (!ENV.solved) {
        char buff[12];
        memcpy(buff, host->str, host->len);
        buff[host->len] = '\0';
        memset(sip, 0, sizeof(*sip));
        int s = inet_pton(AF_INET, buff, &sip->sin_addr.s_addr);
        if (1 != s) {
            yhttp_debug("inet pton error(%d): %s\n", s, strerror(errno));
            return -1;
        }
        sip->sin_family = AF_INET;
        sip->sin_port = htons(port);
        /* FIXME Optimize ENV.solved = 0; */
    }
    skt = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == skt) {
        yhttp_debug("create socket error: %s\n", strerror(errno));
        return -1;
    }
    if (-1 == connect(skt, (struct sockaddr*)sip, sizeof(*sip))) {
        yhttp_debug("connect to %.*s:%d error: %s\n", host->len, host->str, port, strerror(errno));
        return -1;
    }
    return skt;
}

static int get_unix_connection(char const *path, size_t len)
{
    int skt;
    struct sockaddr_un *unpath = (struct sockaddr_un*)&ENV.skaddr;
    if (!ENV.solved) {
        memset(unpath, 0, sizeof(*unpath));
        memcpy(unpath->sun_path, path, len);
        unpath->sun_path[len] = '\0';
        unpath->sun_family = AF_LOCAL;
        ENV.solved = 1;
    }
    skt = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (-1 == skt) {
        yhttp_debug("create socket error: %s\n", strerror(errno));
        return -1;
    }
    if (-1 == connect(skt, (struct sockaddr*)unpath, sizeof(*unpath))) {
        yhttp_debug("connect to %s error: %s\n", path, strerror(errno));
        return -1;
    }

    return skt;
}

extern connection_t *fastcgi_connection_get(struct setting_fastcgi *setting)
{
    int skt = -1;
    int rcvlowat = FCGI_HEADER_LEN;
    connection_t *c;
    switch (setting->scheme) {
    case YHTTP_SETTING_FASTCGI_TCP:
        skt = get_tcp_connection(&setting->host, setting->port);
        break;
    case YHTTP_SETTING_FASTCGI_UNIX:
        skt = get_unix_connection(setting->host.str, setting->host.len);
        break;
    default:
        BUG_ON("unkown FastCGI scheme");
        break;
    }
    if (-1 == skt) {
        yhttp_debug("fastcgin get a connection error\n");
        return NULL;
    }
    c = connection_malloc();
    if (!c) {
        yhttp_debug("connection malloc error\n");
        close(skt);
        return NULL;
    }
    set_nonblock(skt);
    if (-1 == setsockopt(skt, SOL_SOCKET, SO_RCVLOWAT, &rcvlowat, sizeof(rcvlowat))) {
        yhttp_warn("set @rcvlowat for fastcgi socket error: %s\n", strerror(errno));
    }
    c->fd = skt;
    return c;
}

extern int http_fastcgi_init(http_fastcgi_t *fcgi)
{
    fcgi->res_buffer = buffer_malloc(SETTING.vars.buffer_size);
    if (!fcgi->res_buffer)
        return YHTTP_ERROR;
    fcgi->request_id = 1;
    fcgi->extend_hdr_idx = 0;
    fcgi->parse_p = NULL;
    fcgi->parse_state = HTTP_FASTCGI_PARSE_INIT;
    return YHTTP_OK;
}

extern void http_fastcgi_destroy(http_fastcgi_t *fcgi)
{
    BUG_ON(NULL == fcgi);
    buffer_free(fcgi->res_buffer);
}

static int http_fastcgi_parse_key_of_hdr(http_request_t *r, char *start, char *end)
{
    static const struct {
        string_t key;
        int id;
    } hdr_keys[] = {
        {string_newstr("status"),           HDR_STATUS},
        {string_newstr("set-cookie"),       HDR_SET_COOKIE},
        {string_newstr("server"),           HDR_SERVER},
        {string_newstr("content-type"),     HDR_CONTENT_TYPE},
        {string_newstr("content-length"),   HDR_CONTENT_LENGTH},
    };
    string_tolower(start, end-start);
    for (size_t i = 0; i < ARRSIZE(hdr_keys); i++) {
        if (0 == strncmp(hdr_keys[i].key.str, start, hdr_keys[i].key.len)) {
            r->backend.fcgi.hdr_type = hdr_keys[i].id;
            return YHTTP_AGAIN;
        }
    }
    return YHTTP_FAILE;
}

static int http_fastcgi_parse_content_type(char const *start, char const *end)
{
    char const *semicolon;
    for (semicolon = start; semicolon < end && ';' != *semicolon; semicolon++)
        ;
    return http_mime_to_digit(start, semicolon);
}

static int http_fastcgi_parse_vlu_of_hdr(http_request_t *r, char const *start, char const *end)
{
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    int v;

    switch (fcgi->hdr_type) {
    case HDR_STATUS:
        if (1 == sscanf(start, "%d", &v)) {
            if (!http_error_code_support(v))
                return YHTTP_ERROR;
            res->code = v;
            break;
        }
        return YHTTP_ERROR;
        break;
    case HDR_SET_COOKIE:
        res->set_cookie.str = start;
        res->set_cookie.len = end-start;
        break;
    case HDR_SERVER:
        res->server.str = start;
        res->server.len = end-start;
        break;
    case HDR_CONTENT_TYPE:
        com->content_type = http_fastcgi_parse_content_type(start, end);
        break;
    case HDR_CONTENT_LENGTH:
        if (1 == sscanf(start, "%d", &v)) {
            if (v >= 0)
                com->content_length = v;
            break;
        }
        return YHTTP_ERROR;
        break;
    default:
        BUG_ON("unkown fcgi header type");
        return YHTTP_ERROR;
    }
    return YHTTP_AGAIN;
}

extern int http_fastcgi_parse_response(http_request_t *r, char const *start, char const *end)
{
    enum {
        PS_START = HTTP_FASTCGI_PARSE_INIT,

        PS_HEADER_BF,
        PS_HEADER_KEY_IN,
        PS_HEADER_VALUE_BF,     PS_HEADER_VALUE_IN,

        PS_EXTEND_HEADR,
        PS_ALMOST_END,
        PS_END,
    };

    http_fastcgi_t *fcgi = &r->backend.fcgi;
    char const *pos;
    char const *p;
    pos = fcgi->parse_p? fcgi->parse_pos: start;
    for (p = start; p < end; p++) {
        switch (fcgi->parse_state) {
        case PS_START:
            if (CR == *p || LF == *p)
                break;
            if (isalpha(*p)) {
                pos = p;
                fcgi->parse_state = PS_HEADER_BF;
                break;
            }
            return YHTTP_ERROR;

        case PS_HEADER_BF:
            if (isalpha(*p) || '-' == *p)
                break;
            if (':' == *p) {
                int s = http_fastcgi_parse_key_of_hdr(r, (char*)pos,(char*)p);
                fcgi->parse_state = (s == YHTTP_FAILE)? PS_EXTEND_HEADR: PS_HEADER_VALUE_BF;
                break;
            }
            return YHTTP_ERROR;

        case PS_HEADER_VALUE_BF:
            if (' ' == *p)
                break;
            if (isalnum(*p)) {
                pos = p;
                fcgi->parse_state = PS_HEADER_VALUE_IN;
                break;
            }
            return YHTTP_ERROR;

        case PS_HEADER_VALUE_IN:
            if (isprint(*p) && (CR != *p || LF == *p))
                break;
            if (CR == *p) {
                int s = http_fastcgi_parse_vlu_of_hdr(r, pos, p);
                if (s == YHTTP_ERROR)
                    return YHTTP_ERROR;
                fcgi->parse_state = PS_ALMOST_END;
                break;
            }
            return YHTTP_ERROR;

        case PS_EXTEND_HEADR:
            if (CR != *p && LF != *p)
                break;
            if (CR == *p) {
                if (fcgi->extend_hdr_idx >= HTTP_FASTCGI_EXTEND_MAX)
                    return YHTTP_ERROR;
                fcgi->extend_hdr[fcgi->extend_hdr_idx].str = pos;
                fcgi->extend_hdr[fcgi->extend_hdr_idx].len = p-pos;
                fcgi->extend_hdr_idx++;
                fcgi->parse_state = PS_ALMOST_END;
                break;
            }
            break;

        case PS_ALMOST_END:
            if (LF == *p)
                break;
            if (CR == *p) {
                fcgi->parse_state = PS_END;
                break;
            }
            if (isalnum(*p)) {
                pos = p;
                fcgi->parse_state = PS_HEADER_BF;
                break;
            }
            return YHTTP_ERROR;
        case PS_END:
            if (LF == *p) {
                fcgi->parse_pos = (char*)pos;
                fcgi->parse_p = (char*)p+1;
                return YHTTP_OK;
            }
            return YHTTP_ERROR;
        default:
            BUG_ON("unkown fcgi parse state");
            return YHTTP_ERROR;
        }
    }

    fcgi->parse_pos = (char*)pos;
    fcgi->parse_p = (char*)p;
    return YHTTP_AGAIN;
}

extern void http_fastcgi_build(http_request_t *r)
{
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    buffer_t *b = r->ety_buffer;
    FCGI_BeginRequestRecord *bgn_pkt = (FCGI_BeginRequestRecord*)b->last;
    FCGI_Header *hdr;
    int len;

    /* Begin Request Record */
    bgn_pkt->header.version = FCGI_VERSION_1;
    bgn_pkt->header.type = FCGI_BEGIN_REQUEST;
    bgn_pkt->header.requsetIdB0 = B0(fcgi->request_id);
    bgn_pkt->header.requsetIdB1 = B1(fcgi->request_id);
    bgn_pkt->header.contentLengthB0 = 8;  /* sizeof(FCGI_BeginRequestBody) */
    bgn_pkt->header.contentLengthB1 = 0;
    bgn_pkt->header.paddingLength = 0;
    bgn_pkt->body.roleB0 = FCGI_RESPONDER;
    bgn_pkt->body.roleB1 = 0;
    bgn_pkt->body.flags  = 0;   /* don't keep connection */
    b->last += sizeof(*bgn_pkt);

    /* Fill params */
    hdr = (FCGI_Header*)b->last;
    hdr->version = FCGI_VERSION_1;
    hdr->type = FCGI_PARAMS;
    hdr->requsetIdB0 = B0(fcgi->request_id);
    hdr->requsetIdB1 = B1(fcgi->request_id);
    b->last += FCGI_HEADER_LEN;

#define fastcgi_add_digit(k, v) do { \
    *(b->last) = SSTR_LEN(k); \
    int len = snprintf(b->last+2, buffer_rest(b)-2, "%s%d", k, v); \
    *(b->last+1) = len - SSTR_LEN(k); \
    b->last += len+2; \
} while (0)

#define fastcgi_add_str(k, v, l) do { \
    *(b->last) = SSTR_LEN(k); \
    int offset = (l<128)? 2: 5; \
    int len = snprintf(b->last+offset, buffer_rest(b)-offset, "%s%.*s", k, l, (v? v: "")); \
    if (l < 128) { \
        *(b->last+1) = (unsigned char)B0(l); \
    } else { \
        *(b->last+1) = (unsigned char)B3(l) | (1<<7); \
        *(b->last+2) = (unsigned char)B2(l); \
        *(b->last+3) = (unsigned char)B1(l); \
        *(b->last+4) = (unsigned char)B0(l); \
    } \
    b->last += len+offset; \
} while (0)

    fastcgi_add_digit("CONTENT_LENGTH", com->content_length);
    fastcgi_add_str("CONTENT_TYPE", http_mimes[com->content_type].str, http_mimes[com->content_type].len);
    fastcgi_add_str("GATEWAY_INTERFACE", "CGI/1.1", (int)SSTR_LEN("CGI/1.1"));
    fastcgi_add_str("QUERY_STRING", req->query.str, req->query.len);
    switch (req->method) {
    case HTTP_GET:
        fastcgi_add_str("REQUEST_METHOD", "GET",  3);
        break;
    case HTTP_HEAD:
        fastcgi_add_str("REQUEST_METHOD", "HEAD",  4);
        break;
    case HTTP_POST:
        fastcgi_add_str("REQUEST_METHOD", "POST",  4);
        break;
    case HTTP_PUT:
        fastcgi_add_str("REQUEST_METHOD", "PUT",  3);
        break;
    case HTTP_DELETE:
        fastcgi_add_str("REQUEST_METHOD", "DELETE",  6);
        break;
    case HTTP_OPTIONS:
        fastcgi_add_str("REQUEST_METHOD", "OPTIONS",  7);
        break;
    case HTTP_TRACE:
        fastcgi_add_str("REQUEST_METHOD", "TRACE", 5);
    case HTTP_CONNECT:
        fastcgi_add_str("REQUEST_METHOD", "CONNECT", 7);
        break;
    default:
        BUG_ON("unkown method");
        break;
    }

    if (com->version == HTTP10) {
        fastcgi_add_str("SERVER_PROTOCOL", "HTTP/1.0", (int)SSTR_LEN("HTTP/1.0"));
    } else {
        fastcgi_add_str("SERVER_PROTOCOL", "HTTP/1.1", (int)SSTR_LEN("HTTP/1.1"));
    }
    fastcgi_add_str("SCRIPT_NAME", req->uri.str, req->uri.len);
    fastcgi_add_str("PATH_INFO", req->uri.str, req->uri.len);
    fastcgi_add_str("SERVER_SOFTWARE", "yhttpd/"VER, (int)SSTR_LEN("yhttpd/"VER));
    fastcgi_add_str("SERVER_NAME", SETTING.server.host.str, SETTING.server.host.len);
    fastcgi_add_digit("SERVER_PORT", SETTING.server.port);
    if (!string_isnull(&req->cookie))
        fastcgi_add_str("HTTP_COOKIE", req->cookie.str, req->cookie.len);
    len = b->last - (char*)hdr - FCGI_HEADER_LEN;
    hdr->contentLengthB0 = B0(len);
    hdr->contentLengthB1 = B1(len);
    hdr->paddingLength = (~len + 1) & 0x07;
    b->last += hdr->paddingLength;

    /* empty params */
    hdr = (FCGI_Header*)b->last;
    hdr->version = FCGI_VERSION_1;
    hdr->type = FCGI_PARAMS;
    hdr->requsetIdB0 = B0(fcgi->request_id);
    hdr->requsetIdB1 = B1(fcgi->request_id);
    hdr->contentLengthB0 = 0;
    hdr->contentLengthB1 = 0;
    hdr->paddingLength = 0;
    b->last += FCGI_HEADER_LEN;

    hdr = (FCGI_Header*)b->last;
    len = buffer_len(r->hdr_buffer);
    if (len > 0 && com->content_length > 0) {
        hdr->version = FCGI_VERSION_1;
        hdr->type = FCGI_STDIN;
        hdr->requsetIdB0 = B0(fcgi->request_id);
        hdr->requsetIdB1 = B1(fcgi->request_id);
        hdr->contentLengthB0 = B0(len);
        hdr->contentLengthB1 = B1(len);
        hdr->paddingLength = (~len + 1) & 0x07;
        b->last += FCGI_HEADER_LEN;
        buffer_copy(b, r->hdr_buffer);
    }

    hdr = (FCGI_Header*)b->last;
    if (len >= com->content_length) {
        hdr->version = FCGI_VERSION_1;
        hdr->type = FCGI_STDIN;
        hdr->requsetIdB0 = B0(fcgi->request_id);
        hdr->requsetIdB1 = B1(fcgi->request_id);
        hdr->contentLengthB0 = 0;
        hdr->contentLengthB1 = 0;
        hdr->paddingLength = 0;
        b->last += FCGI_HEADER_LEN;
    }
}

extern void http_fastcgi_build_extend_head(http_request_t *r)
{
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    buffer_t *b = r->res_buffer;

    b->last -= 2;   /* remove the last CRLF */
    for (int i = 0; i < fcgi->extend_hdr_idx; i++) {
        int len = MIN(buffer_rest(b), fcgi->extend_hdr[i].len);
        memcpy(b->last, fcgi->extend_hdr[i].str, len);
        *b->last++ = CR;
        *b->last++ = LF;
    }

    *b->last++ = CR;
    *b->last++ = LF;
}

