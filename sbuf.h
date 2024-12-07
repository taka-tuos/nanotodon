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

void ninitbuf(sbctx_t *);
void nflushcache(sbctx_t *);
void nputbuf(sbctx_t *, const void *, int);
void nattron(sbctx_t *, int);
void nattroff(sbctx_t *, int);
void naddch(sbctx_t *, char);
void naddstr(sbctx_t *, const char *);

#endif
