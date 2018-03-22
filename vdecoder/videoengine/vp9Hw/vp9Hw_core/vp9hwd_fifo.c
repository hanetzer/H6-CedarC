/* Copyright 2013 Google Inc. All Rights Reserved. */

#include "vp9hwd_fifo.h"
#include <semaphore.h>

/* Container for instance. */
struct Fifo
{
    sem_t cs_semaphore;    /* Semaphore for critical section. */
    sem_t read_semaphore;  /* Semaphore for readers. */
    sem_t write_semaphore; /* Semaphore for writers. */
    u32 num_of_slots;
    u32 num_of_objects;
    u32 tail_index;
    FifoObject* nodes;
};

#if 0
enum FifoRet FifoReset(FifoInst inst)
{
    struct Fifo* instance = (struct Fifo*)inst;
    instance->tail_index=0;
    instance->num_of_objects=0;
    return FIFO_OK;
}
#endif

enum FifoRet FifoInit(u32 num_of_slots, FifoInst* instance)
{
    struct Fifo* inst = calloc(1, sizeof(struct Fifo));
    if(inst == NULL)
        return FIFO_ERROR_MEMALLOC;
    inst->num_of_slots = num_of_slots;
    /* Allocate memory for the objects. */
    inst->nodes = calloc(num_of_slots, sizeof(FifoObject));
    if(inst->nodes == NULL)
    {
        free(inst);
        return FIFO_ERROR_MEMALLOC;
    }
    *instance = inst;
    return FIFO_OK;
}

enum FifoRet FifoPush(FifoInst inst, FifoObject object, enum FifoException e)
{
    struct Fifo* instance = (struct Fifo*)inst;
    //int value;
    e;

    instance->nodes[(instance->tail_index + instance->num_of_objects) %
                  instance->num_of_slots] = object;
    instance->num_of_objects++;
    return FIFO_OK;
}

enum FifoRet FifoPopRevert(FifoInst inst)
{
    struct Fifo* instance = (struct Fifo*)inst;
    instance->tail_index--;
    instance->num_of_objects++;
    return FIFO_OK;
}

enum FifoRet FifoPop(FifoInst inst, FifoObject* object, enum FifoException e)
{
    struct Fifo* instance = (struct Fifo*)inst;
    e;

    *object = instance->nodes[instance->tail_index % instance->num_of_slots];
    instance->tail_index++;
    instance->num_of_objects--;
    return FIFO_OK;
}

#if 0
u32 FifoCount(FifoInst inst)
{
    u32 count;
    struct Fifo* instance = (struct Fifo*)inst;
    count = instance->num_of_objects;
    return count;
}
#endif

void FifoRelease(FifoInst inst)
{
    struct Fifo* instance = (struct Fifo*)inst;
    free(instance->nodes);
    free(instance);
}

