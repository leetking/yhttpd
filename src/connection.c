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
