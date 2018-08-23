#ifndef _NEWALLOC_H
#define _NEWALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void newalloc_init(void);
void newalloc_exit(void);
void *newalloc(unsigned size);
int newfree(void *addr);
void newalloc_print(void);
void newalloc_wearcount_print(void);



#ifdef __cplusplus
}
#endif

#endif