#ifndef _BASEMALLOC_H
#define _BASEMALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef enum {
    FIRST_FIT, NEXT_FIT, BEST_FIT
} base_type;

void *ffmalloc(size_t size);
void *ffcalloc(size_t number, size_t size);
void *ffrealloc(void *p, size_t size);

void *nfmalloc(size_t size);
void *nfcalloc(size_t number, size_t size);
void *nfrealloc(void *p, size_t size);

void *bfmalloc(size_t size);
void *bfcalloc(size_t number, size_t size);
void *bfrealloc(void *p, size_t size);

void *basemalloc(size_t size, base_type type);
void *basecalloc(size_t number, size_t size, base_type type);
void *baserealloc(void *p, size_t size, base_type type);
void basefree(void *p);

void printblocks();

#ifdef __cplusplus
}
#endif

#endif