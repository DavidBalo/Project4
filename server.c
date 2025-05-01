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
    printf("Server shutting down...\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server_fd, dummy_fd, target_fd;
    struct message req;

    // Ignore SIGPIPE so writes to closed FIFOs don't kill us
    signal(SIGPIPE, SIG_IGN);
    // Handle Ctrl+C to exit cleanly
    signal(SIGINT, terminate);

    // Ensure the server FIFO exists
    mkfifo("serverFIFO", 0666);

    // Open server FIFO for reading and also open a dummy write end
    server_fd = open("serverFIFO", O_RDONLY);
    if (server_fd < 0) {
        perror("open serverFIFO for read failed");
        exit(1);
    }
    dummy_fd = open("serverFIFO", O_WRONLY);
    if (dummy_fd < 0) {
        perror("open serverFIFO for dummy write failed");
        close(server_fd);
        exit(1);
    }

    while (1) {
        // Read the next message request
        ssize_t n = read(server_fd, &req, sizeof(req));
        if (n <= 0) {
            // Either no data or FIFO closed; keep looping
            continue;
        }

        // Log receipt
        printf("Received a request from %s to send the message \"%s\" to %s.\n",
               req.source, req.msg, req.target);
        fflush(stdout);

        // Open the target user's FIFO and forward the message
        target_fd = open(req.target, O_WRONLY);
        if (target_fd < 0) {
            perror("open target FIFO failed");
            continue;
        }

        write(target_fd, &req, sizeof(req));
        close(target_fd);
    }

    // Cleanup (unreachable in this loop, but good practice)
    close(server_fd);
    close(dummy_fd);
    return 0;
}
