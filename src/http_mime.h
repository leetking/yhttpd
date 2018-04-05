#ifndef HTTP_MIME_H__
#define HTTP_MIME_H__

#include "common.h"

enum {
    MIME_INVALID = 0,
    MIME_TEXT_HTML = 1,
    MIME_TEXT_CSS,
    MIME_TEXT_X_C,
    MIME_TEXT_JAVASCRIPT,
    MIME_FONT_TFF,
    MIME_FONT_WOFF,
    MIME_APPLICATION_XML,
    MIME_IMAGE_JPEG,
    MIME_IMAGE_PNG,
    MIME_AUDIO_MPEG3,
    MIME_AUDIO_OGG,
    MIME_VIDEO_OGG,
    MIME_TEXT_PLAIN,
    MIME_APPLICATION_X_WWW_FORM_URLENCODED,
    MIME_APPLICATION_OCTET_STREAM,

    MIME_ID_MAX,
};

extern string_t http_mimes[];

extern int http_mime_init();
extern void http_mime_destroy();
extern int http_mime_get(char const *suffix, size_t len);
extern int http_mime_to_digit(char const *start, char const *end);

#endif
