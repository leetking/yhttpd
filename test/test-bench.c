#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * base line of testing performance
 */

void quit(int _)
{
    printf("exit\n");
    exit(0);
}

void run(int sfd)
{
    for (;;) {
        int cfd;
        struct sockaddr_in cip;
        socklen_t clen = sizeof(cip);
        
        cfd = accept(sfd, (struct sockaddr*)&cip, &clen);
        if (-1 == cfd) {
            perror("accept");
            continue;
        }
        /* just accpet and close */
        close(cfd);
    }
}

int main()
{
    int sfd;
    struct sockaddr_in sip;

    signal(SIGINT, quit);

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sfd) {
        perror("socket");
        return 1;
    }

    memset(&sip, 0, sizeof(sip));
    sip.sin_family = AF_INET;
    sip.sin_port  = htons(8080);
    sip.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("Listing on: 0.0.0.0:%d\n", 8080);
    int on = 1;
    if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
        perror("setsockopt");
        close(sfd);
        return 1;
    }

    if (-1 == bind(sfd, (struct sockaddr*)&sip, sizeof(sip))) {
        perror("bind");
        close(sfd);
        return 1;
    }

    if (-1 == listen(sfd, 7)) {
        perror("listen");
        close(sfd);
        return 1;
    }

    run(sfd);

    close(sfd);

    return 0;
}
