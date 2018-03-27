#include "http_mime.h"
#include "hash.h"

static struct env {
    hash_t mimes;
} ENV;

extern int http_mime_init()
{
    ENV.mimes = hash_create(20, NULL, NULL);
    if (!ENV.mimes)
        return YHTTP_ERROR;
    for (int i = MIME_INVALID; i < MIME_ID_MAX; i++) {
        switch (i) {
        case MIME_TEXT_HTML:
            hash_add(ENV.mimes, "html", SSTR_LEN("html"), (void*)MIME_TEXT_HTML);
            hash_add(ENV.mimes, "htm", SSTR_LEN("htm"), (void*)MIME_TEXT_HTML);
            break;
        case MIME_TEXT_CSS:
            hash_add(ENV.mimes, "css", SSTR_LEN("css"), (void*)MIME_TEXT_CSS);
            break;
        case MIME_TEXT_X_C:
            hash_add(ENV.mimes, "c", SSTR_LEN("c"), (void*)MIME_TEXT_X_C);
            hash_add(ENV.mimes, "h", SSTR_LEN("h"), (void*)MIME_TEXT_X_C);
            break;
        case MIME_TEXT_JAVASCRIPT:
            hash_add(ENV.mimes, "js", SSTR_LEN("js"), (void*)MIME_TEXT_JAVASCRIPT);
            break;
        case MIME_FONT_TFF:
            hash_add(ENV.mimes, "ttf", SSTR_LEN("ttf"), (void*)MIME_FONT_TFF);
            break;
        case MIME_FONT_WOFF:
            hash_add(ENV.mimes, "woff", SSTR_LEN("woff"), (void*)MIME_FONT_WOFF);
            break;
        case MIME_APPLICATION_XML:
            hash_add(ENV.mimes, "xml", SSTR_LEN("xml"), (void*)MIME_APPLICATION_XML);
            break;
        case MIME_IMAGE_JPEG:
            hash_add(ENV.mimes, "jpg", SSTR_LEN("jpg"), (void*)MIME_IMAGE_JPEG);
            hash_add(ENV.mimes, "jpeg", SSTR_LEN("jpeg"), (void*)MIME_IMAGE_JPEG);
            break;
        case MIME_IMAGE_PNG:
            hash_add(ENV.mimes, "png", SSTR_LEN("png"), (void*)MIME_IMAGE_PNG);
            break;
        case MIME_AUDIO_MPEG3:
            hash_add(ENV.mimes, "mp3", SSTR_LEN("mp3"), (void*)MIME_AUDIO_MPEG3);
            break;
        case MIME_AUDIO_OGG:
            hash_add(ENV.mimes, "ogg", SSTR_LEN("ogg"), (void*)MIME_AUDIO_OGG);
            break;
        case MIME_VIDEO_OGG:
            hash_add(ENV.mimes, "ogv", SSTR_LEN("ogv"), (void*)MIME_VIDEO_OGG);
            break;
        case MIME_TEXT_PLAIN:
            hash_add(ENV.mimes, "txt", SSTR_LEN("txt"), (void*)MIME_TEXT_PLAIN);
            hash_add(ENV.mimes, "text", SSTR_LEN("text"), (void*)MIME_TEXT_PLAIN);
            break;
        case MIME_APPLICATION_OCTET_STREAM:
        default:
            break;
        }
    }
    return YHTTP_OK;
}

extern void http_mime_destroy()
{
    hash_destroy(&ENV.mimes);
}

extern int http_mime_get(char const *suffix, size_t len)
{
    intptr_t vlu = (intptr_t)hash_getk(ENV.mimes, suffix, len);
    if ((intptr_t)NULL == vlu || MIME_INVALID == vlu)
        return MIME_APPLICATION_OCTET_STREAM;
    return (int)vlu;
}

