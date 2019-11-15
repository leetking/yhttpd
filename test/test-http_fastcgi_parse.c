#include <assert.h>

#include "fastcgi.h"
#include "http_mime.h"
#include "http.h"
#include "http_mime.h"

static void test1(void)
{
    printf("Test1\n");
    char resstr[] = "Status: 200 OK"CRLF
                    "Set-Cookie: Cookie-is-from-fcgi.lua=1"CRLF
                    "Server: fcgi.lua"CRLF
                    "Content-type: text/html"CRLF
                    "extend-var: foo value"CRLF
                    CRLF
                    "<html><body>"CRLF;
    http_mime_init();
    http_request_t *r = http_request_malloc();
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    http_fastcgi_init(fcgi);

    assert(YHTTP_OK == http_fastcgi_parse_response(r, resstr, resstr+SSTR_LEN(resstr)));
    assert(res->code == HTTP_200);
    assert(0 == strncmp("Cookie-is-from-fcgi.lua=1", res->set_cookie.str, res->set_cookie.len));
    assert(com->content_type == MIME_TEXT_HTML);
    assert(0 == strncmp("fcgi.lua", res->server.str, res->server.len));
    assert(0 == strncmp("extend-var: foo value", fcgi->extend_hdr[0].str, fcgi->extend_hdr[0].len));

    http_fastcgi_destroy(fcgi);
    http_request_destroy(r);
    http_mime_destroy();
    printf("Test1 passed\n");
}

static void test2(void)
{
    printf("Test2\n");
    char resstr[] = "Status: 404 NOT FOUND"CRLF
                    "Content-Type: text/html"CRLF
                    "Content-Length: 233"CRLF
                    CRLF;
    http_mime_init();
    http_request_t *r = http_request_malloc();
    http_fastcgi_t *fcgi = &r->backend.fcgi;
    struct http_head_com *com = &r->hdr_com;
    struct http_head_res *res = &r->hdr_res;
    http_fastcgi_init(fcgi);

    assert(YHTTP_OK == http_fastcgi_parse_response(r, resstr, resstr+SSTR_LEN(resstr)));
    assert(res->code == HTTP_404);
    assert(com->content_type == MIME_TEXT_HTML);
    assert(233 == com->content_length);
    assert(0 == fcgi->extend_hdr_idx);

    http_fastcgi_destroy(fcgi);
    http_request_destroy(r);
    http_mime_destroy();
    printf("Test2 passed\n");
}

int main(int argc, char **argv)
{
    test1();
    test2();

    return 0;
}

