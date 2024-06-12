#ifndef __SIXEL_H__
#define __SIXEL_H__

#include "sbuf.h"

#define SIXEL_MUL_PIC 24
#define SIXEL_MUL_ICO 4

void print_picture(sbctx_t *sbctx, char *uri, int mul);
void sixel_init();

#endif