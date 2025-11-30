#ifndef SMOKERS_P3_COMMON_H
#define SMOKERS_P3_COMMON_H
#define SHM_NAME_P3         "/smokers_p3_shm"
#define SEM_MUTEX_P3        "/smokers_p3_mutex"
#define SEM_AGENT_P3        "/smokers_p3_agent"
#define SEM_SM0_P3          "/smokers_p3_sm0"
#define SEM_SM1_P3          "/smokers_p3_sm1"
#define SEM_SM2_P3          "/smokers_p3_sm2"

#define OBS_FIFO_PATH       "/tmp/smokers_p3_fifo"

enum {
    TOBACCO = 0,
    PAPER   = 1,
    MATCHES = 2
};

typedef struct {
    int table[2];   // два ресурса на столе
    int has_items;  // есть ли что-то на столе
    int stop;       // флаг остановки для всех процессов
} shared_p3_t;

#endif // SMOKERS_P3_COMMON_H

