#include <string.h>

#include "../src/http_url.h"
#include "../src/common.h"

#define LNE(s)  (sizeof(s)-1)

int main()
{
    char pat1[] = "*.htm*";
    char pat2[] = "/cgi-bin/*";
    char pat3[] = "/cgi-bin/*.lua";

    string_t urls[] = {
        string_newstr("/www/fdsfdsf.html"),
        string_newstr("/cgi-bin/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong.html"),
        string_newstr("/cgi-bin/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong.lua"),
        string_newstr("/c/loong.lua"),
    };

    printf("pat1: %s\n", pat1);
    printf("pat2: %s\n", pat2);
    printf("pat3: %s\n", pat3);
    for (size_t i = 0; i < ARRSIZE(urls); i++) {
        printf("%.*s\n", urls[i].len, urls[i].str);
        printf("pat1: %d\n", http_url_match(urls[i].str, urls[i].len, pat1, LNE(pat1)));
        printf("pat2: %d\n", http_url_match(urls[i].str, urls[i].len, pat2, LNE(pat2)));
        printf("pat3: %d\n", http_url_match(urls[i].str, urls[i].len, pat3, LNE(pat3)));
        printf("\n");
    }

    return 0;
}
