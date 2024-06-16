#ifndef __SQUEUE_H__
#define __SQUEUE_H__

#include <pthread.h>
#include "sbuf.h"

#define QUEUE_SIZE 512

void squeue_init(void);
int squeue_enqueue(sbctx_t enq_data);
int squeue_dequeue(sbctx_t *deq_data);

#endif
