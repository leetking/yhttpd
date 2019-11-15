#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <unistd.h>

#include "log.h"
#include "event.h"

static int loop_quit = 0;

static void echo(event_t *ev)
{
    char buffer[512];
    int rdn = read(STDIN_FILENO, buffer, 512);
    write(STDOUT_FILENO, buffer, rdn);
}

static void tick(void *s)
{
    yhttp_info("tick %s\n", (char*)s);
}

static void quit(int _)
{
    printf("cache SIGINT\n");
    loop_quit = 1;
}

int main()
{
    loop_quit = 0;
    yhttp_log_set(LOG_DEBUG2);

    signal(SIGINT, quit);

    event_init();

    event_t ev;
    ev.handle = echo;
    event_add(&ev, EVENT_READ);

    for (;loop_quit;) {
        int ret = event_process((int)1e5);
        printf("ret: %d\n", ret);
    }

    event_quit();
}
