// server.c
// David Baloyi
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server, dummyfd, targetfd;
    struct message req;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  terminate);

    // ensure FIFO exists
    mkfifo("serverFIFO", 0666);

    server  = open("serverFIFO", O_RDONLY);
    dummyfd = open("serverFIFO", O_WRONLY);
    if (server < 0 || dummyfd < 0) {
        perror("opening serverFIFO failed");
        exit(1);
    }

    while (1) {
        if (read(server, &req, sizeof(req)) <= 0)
            continue;

        printf("Received a request from %s to send the message \"%s\" to %s.\n",
               req.source, req.msg, req.target);
        fflush(stdout);

        targetfd = open(req.target, O_WRONLY);
        if (targetfd < 0) {
            perror("open target FIFO failed");
            continue;
        }

        write(targetfd, &req, sizeof(req));
        close(targetfd);
    }

    close(server);
    close(dummyfd);
    return 0;
}
