#pragma once
#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

/* a simple queue of floating point numbers that can be rotated */

struct Queue {
    double *queue;
    size_t size;
};
typedef struct Queue Queue;

Queue *Queue_new(size_t size);
void Queue_rotate(Queue *self);
void Queue_delete(Queue *self);

#endif  // QUEUE_H
