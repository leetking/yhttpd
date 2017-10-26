#include <stdio.h>

#include "../log.h"

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
    return 0;
}
