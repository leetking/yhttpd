#include <unistd.h>
#include "common.h"

char root_path[PATH_MAX] = ".";
char cgi_path[PATH_MAX] = ".";
char err_codes_path[PATH_MAX] = ".";

ssize_t write_s(int fd, uint8_t const *buff, ssize_t n)
{
    int wrn = write(fd, buff, n);
    return wrn;
}
ssize_t read_s(int fd, uint8_t *buff, ssize_t n)
{
    int rdn = read(fd, buff, n);
    return rdn;
}
