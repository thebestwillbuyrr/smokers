#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "smokers_p3_common.h"

static volatile sig_atomic_t g_stop = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_stop = 1;
    write(STDOUT_FILENO, "\n[obs] SIGINT, выхожу...\n", 25);
}

int main(void) {
    // обработчик ctrl+c
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    //создаём FIFO, если ещё нет
    if (mkfifo(OBS_FIFO_PATH, 0666) == -1) {
        if (errno != EEXIST) {
            perror("[obs] mkfifo");
            return 1;
        }
    }

    printf("[obs] Наблюдатель запущен. FIFO: %s\n", OBS_FIFO_PATH);
    printf("[obs] Жду сообщения от процессов...\n");
    fflush(stdout);

    int fd = open(OBS_FIFO_PATH, O_RDONLY);
    if (fd == -1) {
        perror("[obs] open fifo");
        unlink(OBS_FIFO_PATH);
        return 1;
    }

    FILE *f = fdopen(fd, "r");
    if (!f) {
        perror("[obs] fdopen");
        close(fd);
        unlink(OBS_FIFO_PATH);
        return 1;
    }

    char buf[256];
    while (!g_stop && fgets(buf, sizeof(buf), f)) {
        printf("[obs] %s", buf);
        fflush(stdout);
    }

    printf("[obs] Закрываю FIFO и выхожу\n");
    fclose(f);
    unlink(OBS_FIFO_PATH);

    return 0;
}


