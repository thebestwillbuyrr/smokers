#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "smokers_p4_common.h"

static volatile sig_atomic_t g_stop = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_stop = 1;
    write(STDOUT_FILENO, "\n[obs] SIGINT\n", 14);
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[obs] Наблюдатель Program 4 запущен.\n");
    printf("[obs] FIFO: %s\n", OBS_FIFO_P4);
    fflush(stdout);

    int fd;
    while (1) {
        fd = open(OBS_FIFO_P4, O_RDONLY);
        if (fd == -1) {
            perror("[obs] open fifo");
            printf("[obs] Жду, пока агент создаст FIFO...\n");
            fflush(stdout);
            sleep(1);
            continue;
        }
        break;
    }

    FILE *f = fdopen(fd, "r");
    if (!f) {
        perror("[obs] fdopen");
        close(fd);
        return 1;
    }

    char buf[256];
    while (!g_stop && fgets(buf, sizeof(buf), f)) {
        printf("[obs] %s", buf);
        fflush(stdout);
    }

    printf("[obs] Закрываю FIFO и выхожу\n");
    fclose(f);

    return 0;
}


