#ifndef __SIXEL_H__
#define __SIXEL_H__

#include "sbuf.h"

#define SIXEL_MUL_PIC 48
#define SIXEL_MUL_ICO 8

void print_picture(sbctx_t *sbctx, char *uri, int mul);
void sixel_init(void);

#endif
