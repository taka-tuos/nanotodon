#include "nanotodon.h"
#include "sbuf.h"
#include <string.h>
#include <stdlib.h>

// Stringバッファ初期化
void ninitbuf(sbctx_t *sbctx)
{
	sbctx->buf = (void *)0;
	sbctx->bufptr = 0;

	sbctx->cacheptr = 0;
}

void nflushcache(sbctx_t *sbctx)
{
	sbctx->buf = realloc(sbctx->buf, sbctx->bufptr + sbctx->cacheptr + 1);
	memcpy(sbctx->buf + sbctx->bufptr, sbctx->cache, sbctx->cacheptr);
	sbctx->bufptr += sbctx->cacheptr;
	sbctx->cacheptr = 0;
}

// バッファへ追記
void nputbuf(sbctx_t *sbctx, const void *d, int l)
{
	// l > n * SBCTX_CACHESIZE (n > 1) でも勝手に再帰してくれる
	if(l > SBCTX_CACHESIZE) {
		nputbuf(sbctx, d, SBCTX_CACHESIZE);
		nputbuf(sbctx, d + SBCTX_CACHESIZE, l - SBCTX_CACHESIZE);
		return;
	}

	if(sbctx->cacheptr + l > SBCTX_CACHESIZE) {
		nflushcache(sbctx);
	}

	memcpy(sbctx->cache + sbctx->cacheptr, d, l);
	sbctx->cacheptr += l;
}

/*
MEMO :

init_pair(1, COLOR_GREEN, -1);
init_pair(2, COLOR_CYAN, -1);
init_pair(3, COLOR_YELLOW, -1);
init_pair(4, COLOR_RED, -1);
init_pair(5, COLOR_BLUE, -1);
*/

// アトリビュートON
void nattron(sbctx_t *sbctx, int n)
{
	if(n & A_BOLD) nputbuf(sbctx, "\e[1m", 4);

	if(monoflag) return;

	if((n & 7) > 0 && (n & 7) < 5) {
		switch(n & 7){
			case 1:
			nputbuf(sbctx, "\e[32m", 5);
			break;

			case 2:
			nputbuf(sbctx, "\e[36m", 5);
			break;

			case 3:
			nputbuf(sbctx, "\e[33m", 5);
			break;

			case 4:
			nputbuf(sbctx, "\e[31m", 5);
			break;

			case 5:
			nputbuf(sbctx, "\e[34m", 5);
			break;
		}
	}
	//wattron(sbctx, n);
}

// アトリビュートOFF
void nattroff(sbctx_t *sbctx, int n)
{
	nputbuf(sbctx, "\e[0m", 4);
	//wattroff(sbctx, n);
}

// 1文字出力
void naddch(sbctx_t *sbctx, char c)
{
	//waddch(sbctx, c);
	nputbuf(sbctx, &c, 1);
}

// 文字列出力
void naddstr(sbctx_t *sbctx, const char *s)
{
	//waddstr(sbctx, s);
	nputbuf(sbctx, s, strlen(s));
}
