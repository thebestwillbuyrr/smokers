#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <string.h>
#include "smokers_p3_common.h"

static shared_p3_t *g_sh = NULL;
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

    write(STDOUT_FILENO, "\n[agent] SIGINT, завершаю...\n", 29);
}

int main(void) {
    srand(time(NULL) ^ getpid());

    // на всякий случай чистим старые объекты
    shm_unlink(SHM_NAME_P3);
    sem_unlink(SEM_MUTEX_P3);
    sem_unlink(SEM_AGENT_P3);
    sem_unlink(SEM_SM0_P3);
    sem_unlink(SEM_SM1_P3);
    sem_unlink(SEM_SM2_P3);

    int shm_fd = shm_open(SHM_NAME_P3, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[agent] shm_open");
        return 1;
    }
    if (ftruncate(shm_fd, sizeof(shared_p3_t)) == -1) {
        perror("[agent] ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME_P3);
        return 1;
    }

    shared_p3_t *sh = mmap(NULL, sizeof(shared_p3_t),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED, shm_fd, 0);
    if (sh == MAP_FAILED) {
        perror("[agent] mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME_P3);
        return 1;
    }
    close(shm_fd);

    g_sh = sh;
    memset(sh, 0, sizeof(*sh));

    g_mutex = sem_open(SEM_MUTEX_P3, O_CREAT | O_EXCL, 0666, 1);
    g_agent_sem = sem_open(SEM_AGENT_P3, O_CREAT | O_EXCL, 0666, 1);
    g_sm0 = sem_open(SEM_SM0_P3, O_CREAT | O_EXCL, 0666, 0);
    g_sm1 = sem_open(SEM_SM1_P3, O_CREAT | O_EXCL, 0666, 0);
    g_sm2 = sem_open(SEM_SM2_P3, O_CREAT | O_EXCL, 0666, 0);

    if (!g_mutex || g_mutex == SEM_FAILED ||
        !g_agent_sem || g_agent_sem == SEM_FAILED ||
        !g_sm0 || g_sm0 == SEM_FAILED ||
        !g_sm1 || g_sm1 == SEM_FAILED ||
        !g_sm2 || g_sm2 == SEM_FAILED) {
        perror("[agent] sem_open");
        return 1;
    }

    int log_fd = open(OBS_FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (log_fd == -1) {
        fprintf(stderr, "[agent] не удалось открыть FIFO для логов (наблюдатель не запущен?)\n");
    } else {
        g_log = fdopen(log_fd, "w");
        if (!g_log) {
            perror("[agent] fdopen");
            close(log_fd);
        }
    }

    // обработчик ctrl+c
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[agent] Program 3 запущен. Ctrl+C для выхода.\n");
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

    sem_unlink(SEM_MUTEX_P3);
    sem_unlink(SEM_AGENT_P3);
    sem_unlink(SEM_SM0_P3);
    sem_unlink(SEM_SM1_P3);
    sem_unlink(SEM_SM2_P3);

    munmap(sh, sizeof(shared_p3_t));
    shm_unlink(SHM_NAME_P3);

    return 0;
}

