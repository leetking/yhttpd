#include <assert.h>

#include "http.h"
#include "http_error_page.h"

int main()
{
    http_error_page_init(NULL);

    http_error_page_t const *errpage;

    errpage = http_error_page_get(HTTP_416);

    assert(errpage->code == HTTP_416);
    printf("%.*s\n", errpage->page.file.len, errpage->page.file.str);

    http_error_page_destroy();
    return 0;
}
