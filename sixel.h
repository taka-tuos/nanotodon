#ifndef __SIXEL_H__
#define __SIXEL_H__

#include "sbuf.h"

void print_picture(sbctx_t *sbctx, char *uri, int mul);
void sixel_init();

#endif