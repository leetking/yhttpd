#ifndef SETTING_H__
#define SETTING_H__

#include <linux/limits.h>

typedef struct setting_t {
    char root_path[PATH_MAX];
    char cgi_path[PATH_MAX];
    char error_page_path[PATH_MAX];
} setting_t;

extern setting_t SETTING;

#endif
