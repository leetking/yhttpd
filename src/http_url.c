#include "http_url.h"
#include "log.h"

static int http_url_match_iner(char const *url, char const *URLEND,
        char const *pat, char const *PATEND, int dep)
{
    if (dep <= 0) {
        yhttp_debug("pattern %.*s to complex\n", PATEND-pat, pat);
        return 0;
    }

    for (;;) {
        switch (*pat) {
        case '*':
            if (URLEND == url)
                return 0;
            if (http_url_match_iner(url, URLEND, pat+1, PATEND, dep-1))
                return 1;
            url++;
            break;
        case '?':
            if (URLEND == url)
                return 0;
            url++;
            pat++;
            break;
        default:
            if (PATEND == pat)
                return (URLEND == url);

            if (URLEND == url)
                return 0;
            if (*pat != *url)
                return 0;
            url++;
            pat++;
            break;
        }
    }
    return 0;
}
extern int http_url_match(char const *url, ssize_t urln, char const *pat, ssize_t patn)
{
    return http_url_match_iner(url, url+urln, pat, pat+patn, 10);
}


