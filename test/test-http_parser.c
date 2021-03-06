#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "log.h"
#include "http.h"
#include "common.h"
#include "http_parser.h"

static void test_http10(void)
{
    struct tm tm;
    time_t t;
    tm.tm_year = 118;
    tm.tm_mon = 2;
    tm.tm_mday = 25;
    tm.tm_wday = 0;
    tm.tm_hour = 9;
    tm.tm_min = 32;
    tm.tm_sec = 2;
    tm.tm_isdst = 0;
    t = mktime(&tm);

    printf("Test http request http/1.0\n");
    char reqstr[] = "GET http://example.com:8080/url.zip HTTP/1.0"CRLF
                    "From: leetking@test.com"CRLF
                    "If-Modified-Since: Sun, 25 Feb 2018 01:32:02 GMT"CRLF       /* 304 Not Modified */
                    /* Send from the server "Last-Modified: Sat, 12 Oct 1999 21:23:23 GMT"CRLF */
                    /* Location: absURI */
                    "Referer: http://example.com:8080/referer.html"CRLF         /* absURI or relativeURI (//xxxx/) */
                    "Pragma: no-cached"CRLF                                     /* Pragma: no-cached, xxx[=xxx] */
                    /* User-Agent: 1*(product | comment) */
                    /* Expiers: time */
                    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.100 Safari/537.36"CRLF
                    CRLF;
    http_parse_init();
    http_request_t *r = http_request_malloc();
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    if (!r) {
        printf("http_request_malloc error\n");
        printf("Test http request http/1.0 fail\n");
        return;
    }
    r->parse_state = HTTP_PARSE_INIT;
    r->parse_pos = NULL;

    assert(YHTTP_OK == http_parse_request_head(r, reqstr, reqstr+SSTR_LEN(reqstr)));
    assert(HTTP_GET == req->method);
    assert(0 == strncmp("/url.zip", req->uri.str, req->uri.len));
    assert(HTTP10 == com->version);
    assert(0 == strncmp("example.com", req->host.str, req->host.len));
    assert(8080 == req->port);
    assert(0 == strncmp("leetking@test.com", req->from.str, req->from.len));
    assert(t == req->if_modified_since);
    assert(0 == strncmp("http://example.com:8080/referer.html", req->referer.str, req->referer.len));
    assert(0 == strncmp("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.100 Safari/537.36",
                req->user_agent.str, req->user_agent.len));
    assert(HTTP_PRAGMA_NO_CACHE == com->pragma);
    assert(0 == strncmp("zip", req->suffix, req->suffix_len));

    assert(!com->connection);

    http_request_free(r);
    http_parse_destroy();
    printf("Test http request http/1.0 passed\n");
}

static void test_http_head1(void)
{
    printf("Test http request 1\n");
    char reqstr[] = "GET /vim HTTP/1.1"CRLF
                    "Host: localhost:8080"CRLF
                    CRLF;
    http_parse_init();
    http_request_t *r = http_request_malloc();
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    r->parse_state = HTTP_PARSE_INIT;
    r->parse_pos = NULL;
    assert(YHTTP_OK == http_parse_request_head(r, reqstr, reqstr+SSTR_LEN(reqstr)));
    assert(HTTP_200 == res->code);
    assert(HTTP_GET == req->method);
    assert(0 == strncmp("/vim", req->uri.str, req->uri.len));
    assert(HTTP11 == com->version);
    assert(0 == strncmp("localhost:8080", req->host.str, req->host.len));

    assert(com->connection);

    http_request_free(r);
    http_parse_destroy();

    printf("Test http request 1 passed\n");
}

static void test_http_head2(void)
{
    printf("Test http request 2\n");
    char reqstr[] = "POST /vim HTTP/1.1"CRLF
                    "Host: localhost"CRLF
                    "Connection: close"CRLF
                    CRLF;
    int split = 17;
    http_parse_init();
    http_request_t *r = http_request_malloc();
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    r->parse_state = HTTP_PARSE_INIT;
    r->parse_pos = NULL;

    assert(YHTTP_AGAIN == http_parse_request_head(r, reqstr, reqstr+split));
    assert(YHTTP_OK == http_parse_request_head(r, reqstr+split, reqstr+SSTR_LEN(reqstr)));
    assert(HTTP_POST == req->method);
    assert(0 == strncmp("/vim", req->uri.str, req->uri.len));
    assert(0 == strncmp("localhost", req->host.str, req->host.len));

    assert(!com->connection);

    http_request_free(r);
    http_parse_destroy();
    printf("Test http request 2 passed\n");
}

static void test_http_head3(void)
{
    printf("Test http request 3\n");
    char reqstr[] = "GET / HTTP/1.1"CRLF
                    "Host: 119.23.218.41:8081"CRLF
                    "Connection: keep-alive"CRLF
                    "Pragma: no-cache"CRLF
                    "Cache-Control: no-cache"CRLF
                    "Upgrade-Insecure-Requests: 1"CRLF
                    "User-Agent: Mozilla/5.0"CRLF
                    "Accept: text/html"CRLF
                    "Accept-Encoding: gzip, deflate"CRLF
                    "Accept-Language: zh-CN"CRLF
                    "If-None-Match: a2a04f8ee4e538cf"CRLF
                    "Range: bytes=10-200"CRLF
                    "Cookie: BAIDUID=31091F69F038261112B77BB72C452A21:FG=1; PSTM=1498448347; BIDUPSID=E7CC85ED5E1FB5D5E1609479932A931F; BD_UPN=123353; sug=0; sugstore=0; ORIGIN=0; bdime=0; BD_HOME=0; H_PS_PSSID=1429_19033_21087_22073"CRLF
                    CRLF;
    http_parse_init();
    http_request_t *r = http_request_malloc();
    struct http_head_req *req = &r->hdr_req;
    struct http_head_com *com = &r->hdr_com;
    r->parse_state = HTTP_PARSE_INIT;
    r->parse_pos = NULL;

    assert(YHTTP_OK == http_parse_request_head(r, reqstr, reqstr+SSTR_LEN(reqstr)));
    assert(HTTP_GET == req->method);
    assert(0 == strncmp("/", req->uri.str, req->uri.len));
    assert(0 == strncmp("119.23.218.41", req->host.str, req->host.len));
    assert(8081 == req->port);
    assert(com->connection);
    assert(HTTP_PRAGMA_NO_CACHE == com->pragma);
    assert((HTTP_GZIP|HTTP_DEFLATE|HTTP_IDENTITY) == req->accept_encoding);
    assert(0 == strncmp("a2a04f8ee4e538cf", req->if_none_match.str, req->if_none_match.len));
    assert(10 == req->range1);
    assert(200 == req->range2);
    assert(0 == strncmp("BAIDUID=31091F69F038261112B77BB72C452A21:FG=1; PSTM=1498448347;"
                " BIDUPSID=E7CC85ED5E1FB5D5E1609479932A931F; BD_UPN=123353; sug=0; sugstore=0; ORIGIN=0;"
                " bdime=0; BD_HOME=0; H_PS_PSSID=1429_19033_21087_22073", req->cookie.str, req->cookie.len));

    http_request_free(r);
    http_parse_destroy();
    printf("Test http request 3 passed\n");
}

static void test_http_head4(void)
{
    printf("Test http request 4\n");
    char reqstr[] = "GET /foo?p1=v1&p2=v2 HTTP/1.1"CRLF
                    "User-Agent: WebBench 1.5"CRLF
                    "Host: 127.0.0.1"CRLF
                    "Connection: close"CRLF
                    CRLF;
    http_parse_init();
    http_request_t *r = http_request_malloc();
    struct http_head_req *req = &r->hdr_req;
    r->parse_state = HTTP_PARSE_INIT;
    r->parse_pos = NULL;

    assert(YHTTP_OK == http_parse_request_head(r, reqstr, reqstr+SSTR_LEN(reqstr)));
    assert(0 == strncmp("/foo", req->uri.str, req->uri.len));
    assert(0 == strncmp("p1=v1&p2=v2", req->query.str, req->query.len));

    http_request_free(r);
    http_parse_destroy();
    printf("Test http request 4 passed\n");
}

#if 0
static void test_http_post1(void)
{
    printf("Test http request 5\n");
    char reqstr[] = "POST / HTTP/1.1"CRLF
                    "Host: 127.0.0.1:8080"CRLF
                    "User-Agent: curl/7.47.0"CRLF
                    "Accept: */*"CRLF
                    "Content-Length: 11"CRLF
                    "Content-Type: application/x-www-form-urlencoded"CRLF
                    CRLF
                    "p1=v1&p2=v2"CRLF;
    struct http_head *req = http_head_malloc(HTTP_METADATA_LEN);
    req->parse_state = HTTP_PARSE_INIT;
    req->parse_pos = NULL;
    assert(0 == http_head_parse(req, (uint8_t*)reqstr, strlen(reqstr)));
    assert(HTTP_200 == req->state_code);
    printf("uri: %.*s\n", req->lines[HTTP_URI].len, req->lines[HTTP_URI].str);
    printf("\n");

    http_head_free(&req);
}
    test_http_head3();
    test_http_head4();

    test_http_post1();
#endif

int main()
{
    yhttp_log_set(LOG_DEBUG2);

    test_http10();
    test_http_head1();
    test_http_head2();
    test_http_head3();
    test_http_head4();

    return 0;
}
