#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "smokers_p2_common.h"

static shared_t *g_sh = NULL;
static sem_t *g_mutex = NULL;
static sem_t *g_agent_sem = NULL;
static sem_t *g_sm0 = NULL;
static sem_t *g_sm1 = NULL;
static sem_t *g_sm2 = NULL;

static int g_id = -1;
static int g_own_item = -1;

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

static void sigint_handler(int sig) {
    (void)sig;
    if (!g_sh) return;
    g_sh->stop = 1;

    if (g_agent_sem) sem_post(g_agent_sem);
    if (g_sm0) sem_post(g_sm0);
    if (g_sm1) sem_post(g_sm1);
    if (g_sm2) sem_post(g_sm2);

    write(STDOUT_FILENO, "\n[smoker] SIGINT, выхожу...\n", 28);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <id_курильщика: 0/1/2>\n", argv[0]);
        return 1;
    }

    g_id = atoi(argv[1]);
    if (g_id < 0 || g_id > 2) {
        fprintf(stderr, "id должен быть 0, 1 или 2\n");
        return 1;
    }

    if (g_id == 0) g_own_item = TOBACCO;
    else if (g_id == 1) g_own_item = PAPER;
    else g_own_item = MATCHES;

    srand(time(NULL) ^ getpid());
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (shm_fd == -1) {
        perror("shm_open (smoker)");
        fprintf(stderr, "Возможно, агент ещё не запущен.\n");
        return 1;
    }

    shared_t *sh = mmap(NULL, sizeof(shared_t),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, shm_fd, 0);
    if (sh == MAP_FAILED) {
        perror("mmap (smoker)");
        close(shm_fd);
        return 1;
    }
    close(shm_fd);
    g_sh = sh;
    g_mutex = sem_open(SEM_MUTEX_NAME, 0);
    g_agent_sem = sem_open(SEM_AGENT_NAME, 0);
    g_sm0 = sem_open(SEM_SMOKER0_NAME, 0);
    g_sm1 = sem_open(SEM_SMOKER1_NAME, 0);
    g_sm2 = sem_open(SEM_SMOKER2_NAME, 0);

    if (g_mutex == SEM_FAILED || g_agent_sem == SEM_FAILED ||
        g_sm0 == SEM_FAILED || g_sm1 == SEM_FAILED || g_sm2 == SEM_FAILED) {
        perror("sem_open (smoker)");
        fprintf(stderr, "Похоже, агент ещё не успел создать семафоры.\n");
        return 1;
    }

    // обработчик ctrl+C
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[smoker %d, у него %s] запущен\n", g_id, item_name(g_own_item));
    fflush(stdout);

    sem_t *my_sem = (g_id == 0 ? g_sm0 : (g_id == 1 ? g_sm1 : g_sm2));

    while (!g_sh->stop) {
        sem_wait(my_sem);
        if (g_sh->stop) break;

        sem_wait(g_mutex);
        if (g_sh->stop) {
            sem_post(g_mutex);
            break;
        }

        if (!g_sh->has_items) {
            sem_post(g_mutex);
            continue;
        }

        int a = g_sh->table[0];
        int b = g_sh->table[1];
        g_sh->has_items = 0;

        printf("[smoker %d (%s)] берёт: %s + %s\n",
               g_id, item_name(g_own_item), item_name(a), item_name(b));
        fflush(stdout);

        sem_post(g_mutex);

        printf("[smoker %d] крутит сигарету...\n", g_id);
        fflush(stdout);
        msleep_rand(700);

        printf("[smoker %d] курит...\n", g_id);
        fflush(stdout);
        msleep_rand(1200);

        printf("[smoker %d] докурил\n", g_id);
        fflush(stdout);

        sem_post(g_agent_sem);
    }

    printf("[smoker %d] выхожу\n", g_id);

    sem_close(g_mutex);
    sem_close(g_agent_sem);
    sem_close(g_sm0);
    sem_close(g_sm1);
    sem_close(g_sm2);

    munmap(sh, sizeof(shared_t));
    // shm_unlink не делаем тут - это работа агента

    return 0;
}

