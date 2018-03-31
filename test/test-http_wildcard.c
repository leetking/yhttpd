#include <string.h>

#include "../src/http_wildcard.h"
#include "../src/common.h"

#define LNE(s)  (sizeof(s)-1)

int main()
{
    char pat0[] = "?";
    char pat1[] = "*";
    char pat2[] = "/*";
    char pat3[] = "/*.html";
    char pat4[] = "/cgi-bin/*";
    char pat5[] = "/cgi-bin/*.lua";

    string_t urls[] = {
        string_newstr(""),
        string_newstr("/"),
        string_newstr(".htm"),
        string_newstr("/www/fdsfdsf.html"),
        string_newstr("/cgi-bin/loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong.lua"),
        string_newstr("/c/loong.lua"),
    };

    printf("pat0: %s\n", pat0);
    printf("pat1: %s\n", pat1);
    printf("pat2: %s\n", pat2);
    printf("pat3: %s\n", pat3);
    printf("pat4: %s\n", pat4);
    printf("pat5: %s\n", pat5);
    for (size_t i = 0; i < ARRSIZE(urls); i++) {
        printf("%.*s\n", urls[i].len, urls[i].str);
        printf("pat0: %d\n", YHTTP_OK == http_wildcard_match(urls[i].str, urls[i].len, pat0, LNE(pat0)));
        printf("pat1: %d\n", YHTTP_OK == http_wildcard_match(urls[i].str, urls[i].len, pat1, LNE(pat1)));
        printf("pat2: %d\n", YHTTP_OK == http_wildcard_match(urls[i].str, urls[i].len, pat2, LNE(pat2)));
        printf("pat3: %d\n", YHTTP_OK == http_wildcard_match(urls[i].str, urls[i].len, pat3, LNE(pat3)));
        printf("pat4: %d\n", YHTTP_OK == http_wildcard_match(urls[i].str, urls[i].len, pat4, LNE(pat4)));
        printf("pat5: %d\n", YHTTP_OK == http_wildcard_match(urls[i].str, urls[i].len, pat5, LNE(pat5)));
        printf("\n");
    }

    return 0;
}
