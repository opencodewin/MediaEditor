#include "queue.h"

Queue *Queue_new(size_t size) {
    Queue *self = (Queue *)calloc(1, sizeof(Queue));
    self->queue = (double*)calloc(size, sizeof(double));
    self->size = size;
    return self;
}

void Queue_rotate(Queue *self) {
    for(size_t i = 1; i < self->size; i++)
        self->queue[i - 1] = self->queue[i];
    self->queue[self->size - 1] = 0.0;
}

void Queue_delete(Queue *self) {
    if(self) {
        free(self->queue);
        free(self);
        self = NULL;
    }
}
