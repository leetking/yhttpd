#include "setting.h"

struct setting_t SETTING = {
    string_newstr("."),             /* root_path */
    string_newstr("./cgi-bin/"),    /* cgi_path */
    string_newstr("./err-codes/"),  /* error_page_path */
    string_null,                    /* fcgi_pattern */

    0,                              /* enable_fcgi */
};
