#ifndef SETTING_H__
#define SETTING_H__

#include <linux/limits.h>

#include "common.h"

struct setting_t {
    string_t root_path;
    string_t cgi_path;
    string_t error_page_path;

    string_t fcgi_pattern;

    unsigned enable_fcgi:1;
};

extern struct setting_t SETTING;

#endif
