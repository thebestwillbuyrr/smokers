
#ifndef SMOKERS_P2_COMMON_H
#define SMOKERS_P2_COMMON_H
#define SHM_NAME         "/smokers_p2_shm"

#define SEM_MUTEX_NAME   "/smokers_p2_mutex"
#define SEM_AGENT_NAME   "/smokers_p2_agent"
#define SEM_SMOKER0_NAME "/smokers_p2_sm0"
#define SEM_SMOKER1_NAME "/smokers_p2_sm1"
#define SEM_SMOKER2_NAME "/smokers_p2_sm2"

enum {
    TOBACCO = 0,
    PAPER   = 1,
    MATCHES = 2
};

typedef struct {
    int table[2];   // ресурсы на столе
    int has_items;  // 1 - на столе что-то лежит
    int stop;       // 1 - все должны завершиться
} shared_t;

#endif // SMOKERS_P2_COMMON_H

