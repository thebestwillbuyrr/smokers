#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <string.h>
#include "smokers_p2_common.h"

static shared_t *g_sh = NULL;
static sem_t *g_mutex = NULL;
static sem_t *g_agent_sem = NULL;
static sem_t *g_sm0 = NULL;
static sem_t *g_sm1 = NULL;
static sem_t *g_sm2 = NULL;

static const char *item_name(int x) {
    switch (x) {
        case TOBACCO: return "табак";
        case PAPER:   return "бумага";
        case MATCHES: return "спички";
        default:      return "?";
    }
}

static void msleep_rand(int max_ms) {
    if (max_ms <= 0) {
      return;
    }
    int ms = rand() % max_ms;
    usleep(ms * 1000);
}

static void sigint_handler(int sig) {
    (void)sig;
    if (!g_sh) {
      return;
    }
    g_sh->stop = 1;

    if (g_agent_sem) {
      sem_post(g_agent_sem);
    }
    if (g_sm0) {
      sem_post(g_sm0);
    }
    if (g_sm1) {
      sem_post(g_sm1);
    }
    if (g_sm2) {
      sem_post(g_sm2);
    }

    write(STDOUT_FILENO, "\n[agent] SIGINT, завершаю...\n", 30);
}

int main(void) {
    srand(time(NULL) ^ getpid());
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_AGENT_NAME);
    sem_unlink(SEM_SMOKER0_NAME);
    sem_unlink(SEM_SMOKER1_NAME);
    sem_unlink(SEM_SMOKER2_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    if (ftruncate(shm_fd, sizeof(shared_t)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    shared_t *sh = mmap(NULL, sizeof(shared_t),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, shm_fd, 0);
    if (sh == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }
    close(shm_fd);

    g_sh = sh;
    memset(sh, 0, sizeof(*sh));

    //создаём именованные семафоры
    g_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0666, 1);
    g_agent_sem = sem_open(SEM_AGENT_NAME, O_CREAT | O_EXCL, 0666, 1);
    g_sm0 = sem_open(SEM_SMOKER0_NAME, O_CREAT | O_EXCL, 0666, 0);
    g_sm1 = sem_open(SEM_SMOKER1_NAME, O_CREAT | O_EXCL, 0666, 0);
    g_sm2 = sem_open(SEM_SMOKER2_NAME, O_CREAT | O_EXCL, 0666, 0);

    if (!g_mutex || g_mutex == SEM_FAILED ||
        !g_agent_sem || g_agent_sem == SEM_FAILED ||
        !g_sm0 || g_sm0 == SEM_FAILED ||
        !g_sm1 || g_sm1 == SEM_FAILED ||
        !g_sm2 || g_sm2 == SEM_FAILED) {
        perror("sem_open (agent)");
        return 1;
    }

    // обработчик ctrl+c
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[agent] запущен. Ждите активности курильщиков.\n");
    printf("[agent] завершить симуляцию: Ctrl+C в этом окне.\n");
    fflush(stdout);

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

        if (smoker == 0) {
            sem_post(g_sm0);
        } else if (smoker == 1)
            sem_post(g_sm1);
        else
            sem_post(g_sm2);

        sem_post(g_mutex);

        msleep_rand(400);
    }

    printf("[agent] выхожу, чищу ресурсы\n");

    sem_close(g_mutex);
    sem_close(g_agent_sem);
    sem_close(g_sm0);
    sem_close(g_sm1);
    sem_close(g_sm2);

    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_AGENT_NAME);
    sem_unlink(SEM_SMOKER0_NAME);
    sem_unlink(SEM_SMOKER1_NAME);
    sem_unlink(SEM_SMOKER2_NAME);

    munmap(sh, sizeof(shared_t));
    shm_unlink(SHM_NAME);

    return 0;
}

