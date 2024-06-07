#ifndef __SBUF_H__
#define __SBUF_H__

#define SBCTX_CACHESIZE 512
typedef struct {
	char *buf;
	int bufptr;
	char cache[SBCTX_CACHESIZE];
	int cacheptr;
} sbctx_t;

#define COLOR_PAIR(n) (n)
#define A_BOLD 0x80

void ninitbuf(sbctx_t *sbctx);
void nflushcache(sbctx_t *sbctx);
void nputbuf(sbctx_t *sbctx, void *d, int l);
void nattron(sbctx_t *sbctx, int n);
void nattroff(sbctx_t *sbctx, int n);
void naddch(sbctx_t *sbctx, int c);
void naddstr(sbctx_t *sbctx, char *s);

#endif