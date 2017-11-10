#ifndef LOCK_H__
#define LOCK_H__

#include <semaphore.h>

typedef struct lock_t {
    sem_t *sem;
} lock_t;




#endif
