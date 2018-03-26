#include <errno.h>

#include "common.h"
#include "memory.h"
#include "connection.h"

extern connection_t *connection_malloc()
{
    return yhttp_malloc(sizeof(connection_t));
}

extern void connection_free(connection_t *c)
{
    yhttp_free(c);
}

extern ssize_t connection_read(int fd, char *start, char *end)
{
    int rdn = read(fd, start, end-start);
    if (rdn > 0)
        return rdn;
    if (rdn == -1 && (EAGAIN == errno || EWOULDBLOCK == errno))
        return YHTTP_BLOCK;
    if (rdn == -1 && EINTR == errno)
        return YHTTP_AGAIN;
    return YHTTP_ERROR;
}

extern ssize_t connection_write(int fd, char const *start, char const *end)
{
    int wrn = write(fd, start, end-start);
    if (wrn > 0)
        return wrn;
    if (wrn == -1 && (EAGAIN == errno || EWOULDBLOCK == errno))
        return YHTTP_BLOCK;
    if (wrn == -1 && EINTR == errno)
        return YHTTP_AGAIN;
    return YHTTP_ERROR;
}
