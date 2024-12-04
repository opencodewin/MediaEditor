#pragma once
#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stdbool.h>

/* a simple hash / dictionary class */

/* Private - do not use */
typedef struct PHash PHash;
typedef struct PHashDataItem PHashDataItem;
struct PHashDataItem {
    int key;
    void* value;
};
size_t next_key(const PHash* self, int start, int *key);

/* Public */
struct PHash {
    size_t size; // size of the hash
    size_t items; // number of items in hash
    PHashDataItem **hashArray;
};

PHash*  PHash_new(int size);
void    PHash_delete(PHash* self);
void*   PHash_search(const PHash* self, int key);
bool    PHash_insert(PHash* self, int key, const void* value);
#define PHash_forEachKey(hash, key) key=0;for(int i=next_key(hash, 0, &(key)); i<(hash)->size; i=next_key(hash, (int)(i+1), &(key)))

#endif  // HASH_H
