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
    MIME_APPLICATION_OCTET_STREAM,

    MIME_ID_MAX,
};

static string_t http_mimes[] = {
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
    string_newstr("application/octet-stream"),  /* default binary stream */
};

extern int http_mime_init();
extern void http_mime_destroy();
extern int http_mime_get(char const *suffix, size_t len);

#endif
