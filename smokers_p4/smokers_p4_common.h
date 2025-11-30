#ifndef SMOKERS_P4_COMMON_H
#define SMOKERS_P4_COMMON_H

#define SHM_NAME_P4         "/smokers_p4_shm"

#define SEM_MUTEX_P4        "/smokers_p4_mutex"
#define SEM_AGENT_P4        "/smokers_p4_agent"
#define SEM_SM0_P4          "/smokers_p4_sm0"
#define SEM_SM1_P4          "/smokers_p4_sm1"
#define SEM_SM2_P4          "/smokers_p4_sm2"

// путь к именованному каналу для наблюдателей
#define OBS_FIFO_P4         "/tmp/smokers_p4_fifo"

enum {
    TOBACCO = 0,
    PAPER   = 1,
    MATCHES = 2
};

typedef struct {
    int table[2];   // два ресурса на столе
    int has_items;  // есть ли что-то на столе
    int stop;       // флаг остановки
} shared_p4_t;

#endif // SMOKERS_P4_COMMON_H

