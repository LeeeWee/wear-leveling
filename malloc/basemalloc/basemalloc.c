#include "basemalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ALIGNMENT 8 // must be a power of 2

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

struct s_block {
    size_t             size;
    struct s_block    *next;
    struct s_block    *prev;
    int                free;
    void              *data;
};

typedef struct s_block *t_block;

#define BLOCK_SIZE sizeof(struct s_block)

void *base = NULL;
void *temp = NULL;

// finding a chunk: the First Fit Algorithm
t_block firstfit_block(t_block *last, size_t size) {
    t_block b = base;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    }
    return b;
}

// finding a chunk: the Next Fit Algorithm
t_block nextfit_block(t_block *last, size_t size) {
    if (!temp) // initialize temp with base
        temp = base; 
    t_block b = temp;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    } 
    // if b is null, we do not find a fit block in the left blocks
    // find fit block from the base block to temp block
    if (!b) {
        b = base;
        while (b != temp && !(b->free && b->size >= size)) {
            b = b->next;
        }
    }
    if (b == temp) return NULL; // if b equals temp, no fit block is found in free blocks
    temp = b;
    return b;
}

// finding a chunk:  the Best Fit Alogorithm
t_block bestfit_block(t_block *last, size_t size) {
    t_block best_block = NULL;
    t_block b = base;
    while (b) {
        if (b->free && b->size >= size) { // find a fit block
            if (best_block) {
                if (b->size < best_block->size)
                    best_block = b;
            } else {
                best_block = b;
            }
        }
        *last = b;
        b = b->next;
    }
    return best_block;
}

/* Add a new block at the of heap */
/* return NULL if things go wrong */
t_block extend_heap(t_block last, size_t s) {
    t_block b;
    b = sbrk(BLOCK_SIZE + s);
    if (b == (void*) -1)
        return NULL;
    b->size = s;
    b->next = NULL;
    b->prev = last;
    if (last) 
        last->next = b;
    b->free = 0;
    b->data = b + 1;
    return (b);
}

// Splitting blocks
void split_block(t_block b, size_t s) {
    t_block new;
    new = (t_block)(b->data + s);
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->prev = b;
    new->free = 1;
    new->data = new + 1;
    b->size = s;
    b->next = new;
    if (new->next)
        new->next->prev = new;
}

void *basemalloc(size_t size, base_type type) {
    if (!size)
        return NULL;
    t_block b, last;
    size_t s = ALIGN(size);
    if (base) {
        /* find a block using specific finding type */
        last = base;
        switch(type) {
            case FIRST_FIT:
                b = firstfit_block(&last, s);
                break;
            case NEXT_FIT:
                b = nextfit_block(&last, s);
                break;
            case BEST_FIT:
                b = bestfit_block(&last, s);
                break;
        }
        if (b) {
            /* can we split */
            if ((b->size - s) >= (BLOCK_SIZE + ALIGNMENT))
                split_block(b, s);
            b->free = 0;
        } else {
            /* No fitting block, extend the heap */
            b = extend_heap(last, s);
            if (!b) 
                return (NULL);
        }
    } else {
        /* first time */
        b = extend_heap(NULL, s);
        if (!b) 
            return NULL;
        base = b;
    }
    return b->data;
}


void *basecalloc(size_t number, size_t size, base_type type) {
    void *new = basemalloc(number * size, type);
    size_t s4, i;
    /* put 0 on any bytes in the block */
    if (new) {
        char *dst = new;
        s4 = ALIGN(number * size);
        for (i = 0; i < s4; *dst = 0, ++dst, ++i);
    }
    return new;
}

t_block fusion(t_block b) {
    if (b->next && b->next->free) {
        // Modify temp
        if (temp == (void *)b->next)
            temp = (void *)b;
        // modify metadata
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next)
            b->next->prev = b;
    }
    return b;
}

/* Get the block from and addr */
t_block get_block(void *p) {
    char *tmp = p;
    return (t_block)(p = tmp -= BLOCK_SIZE);
}

/* Valid addr for free */
int valid_addr(void *p) {
    if (base) {
        if (p>base && p < sbrk(0)) {
            return (p == (get_block(p))->data);
        }
    }
    return 0;
}

/* free */
void basefree(void *p) {
    t_block b;
    if (valid_addr(p)) {
        b = get_block(p);
        b->free = 1;
        /* fusion with previous if possible */
        if (b->prev && b->prev->free) 
            b = fusion(b->prev);
        /* then fusion with next */
        if (b->next) 
            fusion(b);
        else {
            /* free the end of the heap */
            if (b->prev)
                b->prev->next = NULL;
            else 
                /* No more block! */
                base = NULL;
            brk(b);
        }
    }
}

/* Copy data from block to block */
void copy_block (t_block src , t_block dst) {
    int *sdata, *ddata;
    size_t i;
    sdata = src->data;
    ddata = dst->data;
    for (i=0; i*4 < src->size && i*4 < dst->size; i++)
        ddata[i] = sdata[i];
}


/* The realloc */
void *baserealloc(void *p, size_t size, base_type type)
{
    t_block b, new;
    void *newp;
    if (!p)
        return (basemalloc(size, type));
    if (valid_addr(p))
    {
        size_t s = ALIGN(size);
        b = get_block(p);
        if (b->size >= s)
        {
            if (b->size - s >= ( BLOCK_SIZE + ALIGNMENT))
            split_block(b, s);
        }
        else
        {
            /* Try fusion with next if possible */
            if (b->next && b->next ->free
                && (b->size + BLOCK_SIZE + b->next->size) >= s)
            {
                fusion(b);
                if (b->size - s >= ( BLOCK_SIZE + 4))
                    split_block (b,s);
            } else {
                /* good old realloc with a new block */
                newp = ffmalloc(s);
                if (!newp)
                    /* we’re doomed ! */
                    return NULL;
                new = get_block(newp);
                /* Copy data */
                copy_block (b, new);
                /* free the old one */
                free(p);
                return newp;
            }
        }
        return p;
    }
    return NULL;
}


void *ffmalloc(size_t size) {
    return basemalloc(size, FIRST_FIT);
} 

void *nfmalloc(size_t size) {
    return basemalloc(size, NEXT_FIT);
}

void *bfmalloc(size_t size) {
    return basemalloc(size, BEST_FIT);
}

void *ffcalloc(size_t number, size_t size) {
    return basecalloc(number, size, FIRST_FIT);
}

void *nfcalloc(size_t number, size_t size) {
    return basecalloc(number, size, NEXT_FIT);
}

void *bfcalloc(size_t number, size_t size) {
    return basecalloc(number, size, BEST_FIT);
}

void *ffrealloc(void *p, size_t size) {
    return baserealloc(p, size, FIRST_FIT);
}

void *nfrealloc(void *p, size_t size) {
    return baserealloc(p, size, NEXT_FIT);
}

void *bfrealloc(void *p, size_t size) {
    return baserealloc(p, size, BEST_FIT);
}

void printblocks() {
    if (!base) 
        return;
    int i = 0;
    t_block b = base;
    while (b) {
        i++;
        printf("%d block, address: %p, data_address: %p, size: %x, isfree: %d, temp: %p\n", i, b, b+1, b->size, b->free, temp);
        b = b->next;
    }
}

// int main() {
//     printf("BLOCK_SIZE: %ld\n", BLOCK_SIZE);
//     printf("initialize heap address: %p\n", sbrk(0));
//     void *p1 = sbrk(BLOCK_SIZE + 8);
//     printf("BLOCK_SIZE + 8: %x\n", BLOCK_SIZE + 8);
//     printf("p1 address: %p\n", p1);
//     void *p2 = sbrk(BLOCK_SIZE + 16);
//     printf("p2 address: %p\n", p2);
// }