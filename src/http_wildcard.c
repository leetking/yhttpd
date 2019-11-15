#include "http_wildcard.h"
#include "log.h"
#include "common.h"

static int http_wildcard_match_inner(char const *str, char const *STREND,
        char const *pat, char const *PATEND, int dep)
{
    if (dep <= 0) {
        yhttp_debug("pattern %.*s is too complex\n", PATEND-pat, pat);
        return YHTTP_FAILE;
    }

    for (;pat < PATEND;) {
        switch (*pat) {
        case '*':
            if (YHTTP_OK == http_wildcard_match_inner(str, STREND, pat+1,
                        PATEND, dep-1)) {
                return YHTTP_OK;
            }
            if (STREND == str)
                return YHTTP_FAILE;
            str++;
            break;
        case '?':
            if (YHTTP_OK == http_wildcard_match_inner(str, STREND, pat+1,
                        PATEND, dep)) {
                return YHTTP_OK;
            }
            if (STREND == str)
                return YHTTP_FAILE;
            str++;
            pat++;
            break;
        default:
            if (STREND == str)
                return YHTTP_FAILE;
            if (*pat != *str)
                return YHTTP_FAILE;
            str++;
            pat++;
            break;
        }
    }
    return (STREND == str)? YHTTP_OK: YHTTP_FAILE;
}
extern int http_wildcard_match(char const *str, ssize_t strn, char const *pat,
        ssize_t patn)
{
    return http_wildcard_match_inner(str, str+strn, pat, pat+patn,
                   HTTP_WILDCARD_MAXDEPTH);
}


