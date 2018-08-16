#ifndef _WALLOC_H
#define _WALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void walloc_init(void);
void walloc_exit(void);
void *walloc(unsigned size);
int wfree(void *addr);
void walloc_print(void);



#ifdef __cplusplus
}
#endif

#endif