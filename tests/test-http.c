#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "../src/http.h"

static void test_http_head(char const *reqstr, int len)
{
    struct http_head *req = http_head_malloc(HTTP_METADATA_LEN);
    assert(0 == http_head_parse(req, (uint8_t*)reqstr, len));
    assert(HTTP_200 == req->status_code);

    switch (req->method) {
    case HTTP_GET:    printf("method: GET\n"); break;
    case HTTP_POST:   printf("method: POST\n"); break;
    case HTTP_HEAD:   printf("method: HEAD\n"); break;
    case HTTP_PUT:    printf("method: PUT\n"); break;
    case HTTP_DELETE: printf("method: DELETE\n"); break;
    default:          printf("method: UNKNOWN(%d)\n", req->method); break;
    }

    printf("uri: %.*s\n", req->lines[HTTP_URI].len, req->lines[HTTP_URI].str);
    printf("Ver: %d\n", req->ver);
    printf("Host: %.*s\n", req->lines[HTTP_HOST].len, req->lines[HTTP_HOST].str);
    printf("Connection: %d\n", req->iscon);

    http_head_free(&req);
}

static void test_http_head1(void)
{
    printf("Testn http request 1\n");
    char reqstr[] = "GET / HTTP/1.1"CRLF
                    "Host: localhost:8080"CRLF
                    CRLF;
    struct http_head *req = http_head_malloc(HTTP_METADATA_LEN);
    assert(0 == http_head_parse(req, (uint8_t*)reqstr, sizeof(reqstr)-1));
    assert(HTTP_200 == req->status_code);

    switch (req->method) {
    case HTTP_GET:    printf("method: GET\n"); break;
    case HTTP_POST:   printf("method: POST\n"); break;
    case HTTP_HEAD:   printf("method: HEAD\n"); break;
    case HTTP_PUT:    printf("method: PUT\n"); break;
    case HTTP_DELETE: printf("method: DELETE\n"); break;
    default:          printf("method: UNKNOWN(%d)\n", req->method); break;
    }

    printf("uri: %.*s\n", req->lines[HTTP_URI].len, req->lines[HTTP_URI].str);
    printf("Ver: %d\n", req->ver);
    printf("Host: %.*s\n", req->lines[HTTP_HOST].len, req->lines[HTTP_HOST].str);
    assert(req->iscon);

    http_head_free(&req);
    printf("\n");
}

static void test_http_head2(void)
{
    printf("Testn http request 2\n");
    char reqstr1[] = "GET /vim HTTP/1.1"CRLF;
    char reqstr2[] = "Host: localhost"CRLF CRLF;

    struct http_head *req = http_head_malloc(HTTP_METADATA_LEN);

    assert(1 == http_head_parse(req, (uint8_t*)reqstr1, strlen(reqstr1)));
    assert(0 == http_head_parse(req, (uint8_t*)reqstr2, strlen(reqstr2)));
    assert(HTTP_200 == req->status_code);

    switch (req->method) {
    case HTTP_GET:    printf("method: GET\n"); break;
    case HTTP_POST:   printf("method: POST\n"); break;
    case HTTP_HEAD:   printf("method: HEAD\n"); break;
    case HTTP_PUT:    printf("method: PUT\n"); break;
    case HTTP_DELETE: printf("method: DELETE\n"); break;
    default:          printf("method: UNKNOWN(%d)\n", req->method); break;
    }

    printf("uri: %.*s\n", req->lines[HTTP_URI].len, req->lines[HTTP_URI].str);
    printf("Ver: %d\n", req->ver);
    printf("Host: %.*s\n", req->lines[HTTP_HOST].len, req->lines[HTTP_HOST].str);
    assert(req->iscon);

    http_head_free(&req);
    printf("\n");
}

static void test_http_head3(void)
{
    printf("Test http request 3\n");
    char reqstr[] = "GET / HTTP/1.1"CRLF
                    "Host: 119.23.218.41"CRLF
                    "Connection: keep-alive"CRLF
                    "Pragma: no-cache"CRLF
                    "Cache-Control: no-cache"CRLF
                    "Upgrade-Insecure-Requests: 1"CRLF
                    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.100 Safari/537.36"CRLF
                    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8"CRLF
                    "Accept-Encoding: gzip, deflate"CRLF
                    "Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.6,en;q=0.4,ja;q=0.2"CRLF
                    CRLF;
    test_http_head(reqstr, strlen(reqstr));
    printf("\n");
}

static void test_http_head4()
{
    printf("Test http request 4\n");
    char reqstr[] = "GET / HTTP/1.1"CRLF
                    "User-Agent: WebBench 1.5"CRLF
                    "Host: 127.0.0.1"CRLF
                    "Connection: close"CRLF
                    CRLF;
    test_http_head(reqstr, strlen(reqstr));
    printf("\n");
}

int main()
{
    test_http_head1();
    test_http_head2();
    test_http_head3();
    test_http_head4();
    return 0;
}
