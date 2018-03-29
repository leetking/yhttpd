#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <sched.h>
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

extern void set_cpu_affinity(int cpuid)
{
    int s;
    cpuid %= CPU_ID_MAX;
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(cpuid, &set);

    s = sched_setaffinity(0, sizeof(set), &set);
    if (0 == s)
        yhttp_debug("Process %d bind to CPU %d#\n", getpid(), cpuid);
    else
        yhttp_debug("Process %d bind to CPU %d#, FAILE\n", getpid(), cpuid);
}

#ifdef YHTTP_DEBUG
# include <execinfo.h>
#define YHTTP_DEBUG_BACKTRACE_MAX   30
extern void bug_on(void)
{
    void *arr[YHTTP_DEBUG_BACKTRACE_MAX];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace(arr, 10);
    strings = backtrace_symbols(arr, size);

    printf("Obtained %zd stack frames.\n", size);

    for (size_t i = 0; i < size; i++)
        printf("%s\n", strings[i]);

    free(strings);

    exit(-1);
}
#endif
