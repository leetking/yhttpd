#include <stdio.h>

#include "../src/log.h"

int main()
{
    printf("Log Default:\n");
    yhttp_log(LOG_ERROR, "log error\n");
    yhttp_log(LOG_WARN, "log warn\n");
    yhttp_log(LOG_INFO, "log info\n");
    yhttp_log(LOG_DEBUG, "log debug\n");
    yhttp_log(LOG_DEBUG2, "log debug2\n");
    printf("\n");

    for (int i = LOG_ERROR; i <= LOG_DEBUG2; i++) {
        yhttp_log_set(i);
        printf("Log level: %d\n", i);
        yhttp_log(LOG_ERROR, "log error\n");
        yhttp_log(LOG_WARN, "log warn\n");
        yhttp_log(LOG_INFO, "log info\n");
        yhttp_log(LOG_DEBUG, "log debug\n");
        yhttp_log(LOG_DEBUG2, "log debug2\n");
        printf("\n");
    }

    printf("Log DEBUG:\n");
    yhttp_error("log error %.*s\n", 2, "OK");
    yhttp_warn("log warn %s\n", "OK");
    yhttp_info("log info %s\n", "OK");
    yhttp_debug("log debug %s\n", "OK");
    yhttp_debug2("log debug2 %s\n", "OK");

    return 0;
}
