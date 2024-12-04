#include <stdlib.h>
#include "hash.h"

/* Constructor */
PHash* PHash_new(int size) {
    PHash* self = (PHash*)calloc(1, sizeof(PHash));
    self->size = size;
    self->items = 0;
    self->hashArray = (PHashDataItem**)calloc((size_t)size, sizeof(PHashDataItem));
    return self;
}

/* Destructor */
void PHash_delete(PHash* self) {
    if(self) {
        for(size_t i = 0; i < self->size; i++)
            if(self->hashArray[i] != NULL)
                free(self->hashArray[i]);
        free(self->hashArray);
        free(self);
        self = NULL;
    }
}

void* PHash_search(const PHash* self, int key) {
    size_t i = key % self->size;  // get the hash
    size_t j = 0; // number of visited elements
    while(j < self->size) {
        if(self->hashArray[i]->key == key)
            return self->hashArray[i]->value;
        i++; // go to next cell
        i %= self->size; // wrap around the table
        j++;
    }
    return NULL; // not found
}

bool PHash_insert(PHash* self, int key, const void* value) {
    if(self->items == self->size)
        return false; // hash is full - cannot insert new items
    PHashDataItem* item = (PHashDataItem*)calloc(1, sizeof(PHashDataItem));
    item->value = (void*)value;
    item->key = key;
    size_t i = key % self->size;
    while(self->hashArray[i] != NULL) {  // move in array until an empty cell found
        i++;  // go to next cell
        i %= self->size;  // wrap around the table
    }
    self->hashArray[i] = item;
    self->items++;
    return true;
}

/* returns the index of the next entry in hashArray that is not NULL and the entry's key */
inline size_t next_key(const PHash* self, int start, int *key) {
    for(size_t i=start; i<self->size; i++)
        if(self->hashArray[i]!=NULL) {
            *key = self->hashArray[i]->key;
            return i;
        }
    return self->size;
}
