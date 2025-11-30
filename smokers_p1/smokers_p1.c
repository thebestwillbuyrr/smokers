#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string.h>

#define SHM_NAME "/smokers_p1_shm"

enum { TOBACCO = 0, PAPER = 1, MATCHES = 2 };

const char *item_name(int x) {
    switch (x) {
        case TOBACCO: return "табак";
        case PAPER:   return "бумага";
        case MATCHES: return "спички";
        default: return "?";
    }
}

typedef struct {
    sem_t mutex;
    sem_t agent_sem;
    sem_t smoker_sem[3];
    int table[2];
    int has_items;
    int stop;
} shared_t;

static shared_t *g_sh = NULL;

static void sigint_handler(int sig) {
    (void)sig;
    if (!g_sh) return;

    g_sh->stop = 1;
    sem_post(&g_sh->agent_sem);
    for (int i = 0; i < 3; ++i)
        sem_post(&g_sh->smoker_sem[i]);
}

static void msleep_rand(int max_ms) {
    int ms = rand() % max_ms;
    usleep(ms * 1000);
}

static void agent_loop(void) {
    srand(time(NULL) ^ getpid());

    while (!g_sh->stop) {
        sem_wait(&g_sh->agent_sem);
        if (g_sh->stop) break;

        int pair = rand() % 3;
        int a, b, smoker;

        if (pair == 0) { a = TOBACCO; b = PAPER; smoker = 2; }
        else if (pair == 1) { a = TOBACCO; b = MATCHES; smoker = 1; }
        else { a = PAPER; b = MATCHES; smoker = 0; }

        sem_wait(&g_sh->mutex);
        g_sh->table[0] = a;
        g_sh->table[1] = b;
        g_sh->has_items = 1;
        printf("[посредник] кладёт: %s + %s - > курильщик %d\n",
               item_name(a), item_name(b), smoker);
        fflush(stdout);
        sem_post(&g_sh->smoker_sem[smoker]);
        sem_post(&g_sh->mutex);

        msleep_rand(400);
    }

    printf("[посредник] завершён\n");
}

static void smoker_loop(int id, int own) {
    srand(time(NULL) ^ getpid());

    while (!g_sh->stop) {
        sem_wait(&g_sh->smoker_sem[id]);
        if (g_sh->stop) break;

        sem_wait(&g_sh->mutex);

        if (!g_sh->has_items) {
            sem_post(&g_sh->mutex);
            continue;
        }

        int a = g_sh->table[0], b = g_sh->table[1];
        g_sh->has_items = 0;

        printf("[курильщик %d (%s)] взял: %s + %s\n",
               id, item_name(own), item_name(a), item_name(b));
        fflush(stdout);

        sem_post(&g_sh->mutex);

        printf("[курильщик %d] крутит...\n", id);
        msleep_rand(600);

        printf("[курильщик %d] курит...\n", id);
        msleep_rand(1000);

        printf("[курильщик %d] докурил\n", id);

        sem_post(&g_sh->agent_sem);
    }

    printf("[курильщик %d] завершён\n", id);
}

int main() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_t));

    shared_t *sh = mmap(NULL, sizeof(shared_t),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    g_sh = sh;
    memset(sh, 0, sizeof(*sh));

    sem_init(&sh->mutex, 1, 1);
    sem_init(&sh->agent_sem, 1, 1);
    for (int i = 0; i < 3; i++)
        sem_init(&sh->smoker_sem[i], 1, 0);

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    pid_t p[4];

    if ((p[0] = fork()) == 0) {
        agent_loop();
        exit(0);
    }

    int items[3] = { TOBACCO, PAPER, MATCHES };

    for (int i = 0; i < 3; i++) {
        if ((p[i+1] = fork()) == 0) {
            smoker_loop(i, items[i]);
            exit(0);
        }
    }

    printf("Запущено: посредник + 3 курильщика \n");
    printf("Нажмите Ctrl+C чтобы завершить.\n");

    for (int i = 0; i < 4; i++)
        waitpid(p[i], NULL, 0);

    sem_destroy(&sh->mutex);
    sem_destroy(&sh->agent_sem);
    for (int i = 0; i < 3; i++)
        sem_destroy(&sh->smoker_sem[i]);

    munmap(sh, sizeof(shared_t));
    shm_unlink(SHM_NAME);

    printf("[parent] все завершены, ресурсы очищены\n");
    return 0;
}
