#include "memory.h"
#include "http.h"
#include "http_mime.h"
#include "http_time.h"
#include "http_error_page.h"

http_error_page_t pages[] = {
    {HTTP_200, "200", string_newstr("200 OK"), },
    {HTTP_202, "202", string_newstr("202 Accept"), },

    {HTTP_301, "301", string_newstr("301 OK"), },

    {HTTP_400, "400", string_newstr("400 Bad Request"), },
    {HTTP_403, "403", string_newstr("403 Forbidden"), },
    {HTTP_404, "404", string_newstr("404 Not Found"), },
    {HTTP_405, "405", string_newstr("405 Method Not Allowed"), },
    {HTTP_413, "413", string_newstr("413 Request Entity Too Large"), },
    {HTTP_414, "414", string_newstr("414 Request-URI Too Large"), },
    {HTTP_415, "415", string_newstr("415 Unsupported Media Type"), },
    {HTTP_416, "416", string_newstr("416 Requested range not statisfiable"), },

    {HTTP_500, "500", string_newstr("500 Internal Server Error"), },
    {HTTP_501, "501", string_newstr("501 Not Implemented"), },
    {HTTP_505, "505", string_newstr("505 HTTP Version not supported"), },
};
static struct env {
    char const *pool;
    char const *pos;
} ENV;

extern int http_error_page_init(char const *dir)
{
    char const *tempalte = {
        "<!DOCTYPE html>"
            "<html lang='en'>"
            "<head>"
            "<meta charset='UTF-8'>"
            "<title>%.*s</title>"
            "</head>"
            "<body>"
            "<h1>%.*s</h1>"
            "<p>Server: %.*s</p>"
            "</body>"
            "</html>"
    };
#define HTTP_TEMPLATE_PAGE_SIZE    (256)

    ENV.pool = yhttp_malloc(ARRSIZE(pages)*HTTP_TEMPLATE_PAGE_SIZE);
    if (!ENV.pool)
        return YHTTP_ERROR;
    ENV.pos = ENV.pool;

    /* TODO load error page from file */
    for (size_t i = 0; i < ARRSIZE(pages); i++, ENV.pos += HTTP_TEMPLATE_PAGE_SIZE) {
        int len;
        http_page_t *page = &(pages+i)->page;
        page->ctime = current_sec;
        page->mime = MIME_TEXT_HTML;
        page->file.str = ENV.pos;
            len = snprintf((char*)page->file.str, HTTP_TEMPLATE_PAGE_SIZE, tempalte,
                    pages[i].reason.len, pages[i].reason.str,
                    pages[i].reason.len, pages[i].reason.str,
                    SSTR_LEN("yhttpd/"VER), "yhttpd/"VER);
        page->file.len = len;
    }
    return YHTTP_OK;
}

extern void http_error_page_destroy()
{
    yhttp_free((void*)ENV.pool);
    ENV.pool = ENV.pos = NULL;
}

extern http_error_page_t const *http_error_page_get(int code)
{
    for (size_t i = 0; i < ARRSIZE(pages); i++) {
        if (code == pages[i].code)
            return pages+i;
    }
    BUG_ON("code is not in the range\n");
    return NULL;
}
