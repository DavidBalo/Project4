// rsh.c
// David Baloyi
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

char *allowed[N] = {
    "cp","touch","mkdir","ls","pwd","cat","grep","chmod",
    "diff","cd","exit","help","sendmsg"
};

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

void sendmsg(char *user, char *target, char *msg) {
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

void* messageListener(void *arg) {
    struct message m;
    int readfd, dummyfd;

    /* make sure our FIFO exists */
    mkfifo(uName, 0666);

    readfd  = open(uName, O_RDONLY);
    dummyfd = open(uName, O_WRONLY);
    if (readfd < 0 || dummyfd < 0) {
        perror("opening user FIFO failed");
        pthread_exit((void*)1);
    }

    while (1) {
        if (read(readfd, &m, sizeof(m)) > 0) {
            /* NO leading newline here */
            printf("Incoming message from %s: %s\n", m.source, m.msg);
            fflush(stdout);
        }
    }

    /* unreachable */
    close(readfd);
    close(dummyfd);
    pthread_exit((void*)0);
}

int isAllowed(const char* cmd) {
    for (int i = 0; i < N; i++)
        if (strcmp(cmd, allowed[i]) == 0)
            return 1;
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256], line2[256];
    int status;
    posix_spawnattr_t attr;

    if (argc != 2) {
        fprintf(stderr, "Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);
    strcpy(uName, argv[1]);

    /* spin up listener */
    pthread_t tid;
    pthread_create(&tid, NULL, messageListener, NULL);
    pthread_detach(tid);

    while (1) {
        fprintf(stderr, "rsh>");
        if (!fgets(line, sizeof(line), stdin)) continue;
        if (line[0] == '\n') continue;
        line[strlen(line)-1] = '\0';

        strcpy(line2, line);
        char *tok = strtok(line, " ");
        char cmd[256];
        strcpy(cmd, tok);

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            if (!target) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }
            char *msg = strtok(NULL, "");
            if (!msg) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            sendmsg(uName, target, msg);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *dir = strtok(NULL, " ");
            if (strtok(NULL, " "))
                printf("-rsh: cd: too many arguments\n");
            else
                chdir(dir);
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++)
                printf("%d: %s\n", i+1, allowed[i]);
            continue;
        }

        /* external command */
        cargv = malloc(sizeof(char*));
        cargv[0] = strdup(cmd);
        int n = 1;
        char *arg = strtok(line2, " ");
        while ((arg = strtok(NULL, " ")) != NULL) {
            n++;
            cargv = realloc(cargv, sizeof(char*) * n);
            cargv[n-1] = strdup(arg);
        }
        cargv = realloc(cargv, sizeof(char*) * (n+1));
        cargv[n] = NULL;

        posix_spawnattr_init(&attr);
        if (posix_spawnp(&pid, cmd, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(1);
        }
        waitpid(pid, &status, 0);
        posix_spawnattr_destroy(&attr);

        for (int i = 0; i < n; i++) free(cargv[i]);
        free(cargv);
    }
    return 0;
}
