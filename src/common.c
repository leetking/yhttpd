#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>

#include "log.h"
#include "common.h"

extern void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags) {
        yhttp_warn("Get fd(%d) status error: %s\n", fd, strerror(errno));
        return;
    }
    if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
        yhttp_warn("Set fd(%d) as nonblock error: %s\n", fd, strerror(errno));
}

extern void string_tolower(char *str, size_t len)
{
    BUG_ON(str == NULL);

    for (char *end = str+len; str < end; str++)
        *str = tolower(*str);
}
