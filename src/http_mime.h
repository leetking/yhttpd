#ifndef HTTP_MIME_H__
#define HTTP_MIME_H__

#include "common.h"

static string_t http_mimes[] = {
    string_newstr("text/html"),
    string_newstr("text/css"),
    string_newstr("text/plain"),
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
};

enum {
    MIME_TEXT_HTML = 0,
    MINE_TEXT_CSS,
    MIME_TEXT_PLAIN,
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
};

#endif
