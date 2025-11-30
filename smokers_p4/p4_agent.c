#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "smokers_p4_common.h"

static shared_p4_t *g_sh = NULL;
static sem_t *g_mutex = NULL;
static sem_t *g_agent_sem = NULL;
static sem_t *g_sm0 = NULL;
static sem_t *g_sm1 = NULL;
static sem_t *g_sm2 = NULL;

static FILE *g_log = NULL;

static const char *item_name(int x) {
    switch (x) {
        case TOBACCO: return "табак";
        case PAPER:   return "бумага";
        case MATCHES: return "спички";
        default:      return "?";
    }
}

static void msleep_rand(int max_ms) {
    if (max_ms <= 0) return;
    int ms = rand() % max_ms;
    usleep(ms * 1000);
}

static void obs_log(const char *msg) {
    if (g_log) {
        fputs(msg, g_log);
        fflush(g_log);
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    if (!g_sh) return;
    g_sh->stop = 1;

    if (g_agent_sem) sem_post(g_agent_sem);
    if (g_sm0) sem_post(g_sm0);
    if (g_sm1) sem_post(g_sm1);
    if (g_sm2) sem_post(g_sm2);

    write(STDOUT_FILENO, "\n[agent] SIGINT\n", 16);
}

int main(void) {
    srand(time(NULL) ^ getpid());

    shm_unlink(SHM_NAME_P4);
    sem_unlink(SEM_MUTEX_P4);
    sem_unlink(SEM_AGENT_P4);
    sem_unlink(SEM_SM0_P4);
    sem_unlink(SEM_SM1_P4);
    sem_unlink(SEM_SM2_P4);
    unlink(OBS_FIFO_P4);

    int shm_fd = shm_open(SHM_NAME_P4, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[agent] shm_open");
        return 1;
    }
    if (ftruncate(shm_fd, sizeof(shared_p4_t)) == -1) {
        perror("[agent] ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME_P4);
        return 1;
    }

    shared_p4_t *sh = mmap(NULL, sizeof(shared_p4_t),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED, shm_fd, 0);
    if (sh == MAP_FAILED) {
        perror("[agent] mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME_P4);
        return 1;
    }
    close(shm_fd);

    g_sh = sh;
    memset(sh, 0, sizeof(*sh));

    g_mutex = sem_open(SEM_MUTEX_P4, O_CREAT | O_EXCL, 0666, 1);
    g_agent_sem = sem_open(SEM_AGENT_P4, O_CREAT | O_EXCL, 0666, 1);
    g_sm0 = sem_open(SEM_SM0_P4, O_CREAT | O_EXCL, 0666, 0);
    g_sm1 = sem_open(SEM_SM1_P4, O_CREAT | O_EXCL, 0666, 0);
    g_sm2 = sem_open(SEM_SM2_P4, O_CREAT | O_EXCL, 0666, 0);

    if (!g_mutex || g_mutex == SEM_FAILED ||
        !g_agent_sem || g_agent_sem == SEM_FAILED ||
        !g_sm0 || g_sm0 == SEM_FAILED ||
        !g_sm1 || g_sm1 == SEM_FAILED ||
        !g_sm2 || g_sm2 == SEM_FAILED) {
        perror("[agent] sem_open");
        return 1;
    }

    if (mkfifo(OBS_FIFO_P4, 0666) == -1) {
        if (errno != EEXIST) {
            perror("[agent] mkfifo");
            return 1;
        }
    }

    int log_fd = open(OBS_FIFO_P4, O_WRONLY | O_NONBLOCK);
    if (log_fd == -1) {
        fprintf(stderr, "[agent] не удалось открыть FIFO для наблюдателей\n");
    } else {
        g_log = fdopen(log_fd, "w");
        if (!g_log) {
            perror("[agent] fdopen");
            close(log_fd);
        }
    }

    // обработчик ctrl+
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[agent] Program 4: запущен. Ctrl+C для выхода.\n");
    printf("[agent] FIFO для наблюдателей: %s\n", OBS_FIFO_P4);
    fflush(stdout);

    char buf[256];

    while (!g_sh->stop) {
        sem_wait(g_agent_sem);
        if (g_sh->stop) break;

        int pair = rand() % 3;
        int a, b, smoker;

        if (pair == 0) { a = TOBACCO; b = PAPER;   smoker = 2; }
        else if (pair == 1) { a = TOBACCO; b = MATCHES; smoker = 1; }
        else { a = PAPER; b = MATCHES; smoker = 0; }

        sem_wait(g_mutex);
        if (g_sh->stop) {
            sem_post(g_mutex);
            break;
        }

        g_sh->table[0] = a;
        g_sh->table[1] = b;
        g_sh->has_items = 1;

        printf("[agent] кладёт: %s + %s → smoker %d\n",
               item_name(a), item_name(b), smoker);
        fflush(stdout);

        snprintf(buf, sizeof(buf),
                 "agent: кладёт %s + %s → smoker %d\n",
                 item_name(a), item_name(b), smoker);
        obs_log(buf);

        if (smoker == 0) sem_post(g_sm0);
        else if (smoker == 1) sem_post(g_sm1);
        else sem_post(g_sm2);

        sem_post(g_mutex);
        msleep_rand(400);
    }

    printf("[agent] выхожу, чищу IPC\n");

    if (g_log) fclose(g_log);

    sem_close(g_mutex);
    sem_close(g_agent_sem);
    sem_close(g_sm0);
    sem_close(g_sm1);
    sem_close(g_sm2);

    sem_unlink(SEM_MUTEX_P4);
    sem_unlink(SEM_AGENT_P4);
    sem_unlink(SEM_SM0_P4);
    sem_unlink(SEM_SM1_P4);
    sem_unlink(SEM_SM2_P4);

    munmap(sh, sizeof(shared_p4_t));
    shm_unlink(SHM_NAME_P4);

    unlink(OBS_FIFO_P4);

    return 0;
}

