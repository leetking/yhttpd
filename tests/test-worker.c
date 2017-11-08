#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <semaphore.h>

#include "../worker.h"
#include "../common.h"
#include "../log.h"

int main()
{
    yhttp_log_set(LOG_DEBUG2);
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sfd) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in sip;
    memset(&sip, 0, sizeof(sip));
    sip.sin_family = AF_INET;
    sip.sin_port = htons(8080);
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

    sem_t *sem = sem_open(ACCEPT_LOCK, O_CREAT, 0600, 1);
    if (SEM_FAILED == sem) {
        perror("init lock");
        goto err;
    }

    run_worker(sfd);

err:
    sem_unlink(ACCEPT_LOCK);

    return 0;
}

