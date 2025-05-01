// David BAloyi

#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

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

// Sends a message struct to the server FIFO
void sendmsg_func(char *user, char *target, char *msg) {
    struct message m;
    int fd;

    strcpy(m.source, user);
    strcpy(m.target, target);
    strcpy(m.msg, msg);

    fd = open("serverFIFO", O_WRONLY);
    if (fd < 0) {
        perror("open serverFIFO failed");
        return;
    }

    write(fd, &m, sizeof(m));
    close(fd);
}

// Listens on the user's FIFO for incoming messages
void* messageListener(void *arg) {
    struct message incoming;
    int readfd, dummyfd;

    // Ensure the user's FIFO exists
    mkfifo(uName, 0666);

    // Open read-end and dummy write-end so read() won't return EOF
    readfd  = open(uName, O_RDONLY);
    dummyfd = open(uName, O_WRONLY);

    while (1) {
        ssize_t n = read(readfd, &incoming, sizeof(incoming));
        if (n <= 0) continue;  // no data or error

        // Print incoming message and re-display prompt
        printf("\nIncoming message from %s: %s\n", incoming.source, incoming.msg);
        fflush(stdout);
        fprintf(stderr, "rsh>");
        fflush(stderr);
    }

    close(readfd);
    close(dummyfd);
    return NULL;
}

int isAllowed(const char* cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);

    strcpy(uName, argv[1]);

    // Start the message listener thread
    pthread_t listener;
    if (pthread_create(&listener, NULL, messageListener, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_detach(listener);

    while (1) {
        fprintf(stderr, "rsh>");
        if (fgets(line, sizeof(line), stdin) == NULL) continue;
        if (strcmp(line, "\n") == 0) continue;
        line[strlen(line)-1] = '\0';  // remove trailing newline

        char cmd[256], line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            // Parse target and message (message may contain spaces)
            char *rest = line2 + strlen(cmd) + 1;
            char *target = strtok(rest, " ");
            if (!target) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }
            char *msg = rest + strlen(target) + 1;
            if (!*msg) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            sendmsg_func(uName, target, msg);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i+1, allowed[i]);
            }
            continue;
        }

        // External command: build argv array
        cargv = malloc(sizeof(char*));
        cargv[0] = malloc(strlen(cmd) + 1);
        strcpy(cargv[0], cmd);

        char *token = strtok(line2, " "); // skip cmd
        token = strtok(NULL, " ");
        int n = 1;
        while (token) {
            n++;
            cargv = realloc(cargv, sizeof(char*) * n);
            cargv[n-1] = malloc(strlen(token) + 1);
            strcpy(cargv[n-1], token);
            token = strtok(NULL, " ");
        }
        cargv = realloc(cargv, sizeof(char*) * (n+1));
        cargv[n] = NULL;

        // Spawn the process
        posix_spawnattr_init(&attr);
        if (posix_spawnp(&pid, cmd, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }
        posix_spawnattr_destroy(&attr);

        // Free argv
        for (int i = 0; i < n; i++) free(cargv[i]);
        free(cargv);
    }

    return 0;
}
