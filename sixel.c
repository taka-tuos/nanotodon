#include "nanotodon.h"
#include "sixel.h"
#include "utils.h"
#include "config.h"
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef USE_WEBP
#include <webp/decode.h>
#include <webp/encode.h>
#endif

#ifdef USE_SIXEL
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "stb_image.h"

#include "err.png.h"

char *errpic_six_pic;
char *errpic_six_ico;
char *palinit_six;

extern struct nanotodon_config config;

char cpath_buffer[512];
char *cpath_ptr;

static int indent_icon;
#define DEF_FONT_WIDTH	7
#define DEF_FONT_HEIGHT	14

#define CLIP_CH(n) ((n) > 255 ? 255 : (n) < 0 ? 0 : (n))

void sixel_out(sbctx_t *sbctx, int ix, int iy, int ic, stbi_uc *ib, int mul);

void sixel_init()
{
	sbctx_t sb_errpic;
	sbctx_t sb_palinit;
	struct winsize ws;
	int fontwidth, fontheight;
	int err;

	// ウインドウサイズからフォントサイズサイズを算出
	fontwidth  = 0;
	fontheight = 0;
	err = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	if (err == 0) {
		if (ws.ws_col != 0) {
			fontwidth = ws.ws_xpixel / ws.ws_col;
		}
		if (ws.ws_row != 0) {
			fontheight = ws.ws_ypixel / ws.ws_row;
		}
	}
	if (fontwidth == 0) {
		fontwidth = DEF_FONT_WIDTH;
	}
	if (fontheight == 0) {
		fontheight = DEF_FONT_HEIGHT;
	}

	// アイコン横幅分相当のインデント文字数 (最低1ドットは離す)
	indent_icon = ((6 * SIXEL_MUL_ICO) / fontwidth) + 1;

	ninitbuf(&sb_palinit);

#ifndef USE_RGB222
	if (!monoflag) {
	naddstr(&sb_palinit, "#0;2;0;0;0");
	naddstr(&sb_palinit, "#1;2;100;0;0");
	naddstr(&sb_palinit, "#2;2;0;100;0");
	naddstr(&sb_palinit, "#3;2;100;100;0");
	naddstr(&sb_palinit, "#4;2;0;0;100");
	naddstr(&sb_palinit, "#5;2;100;0;100");
	naddstr(&sb_palinit, "#6;2;0;100;100");
	naddstr(&sb_palinit, "#7;2;100;100;100");
	} else {
	naddstr(&sb_palinit, "#0;2;0;0;0");
	naddstr(&sb_palinit, "#1;2;100;100;100");
	}
#else
	for(int i = 0; i < 64; i++) {
		char str[256];

		int r = (i >> 4) & 3;
		int g = (i >> 2) & 3;
		int b = (i >> 0) & 3;

		int tbl[] = {0,33,66,100};

		sprintf(str, "#%d;2;%d;%d;%d", i, tbl[r], tbl[g], tbl[b]);
		naddstr(&sb_palinit, str);
	}
#endif

	nflushcache(&sb_palinit);

	palinit_six = sb_palinit.buf;
	palinit_six[sb_palinit.bufptr] = 0;

	strcpy(cpath_buffer, config.cache_dir);
	cpath_ptr = cpath_buffer + strlen(config.cache_dir);

	{
		ninitbuf(&sb_errpic);

		int ix, iy, ic;
		stbi_uc *ib = stbi_load_from_memory((const stbi_uc *)err_png, err_png_len, &ix, &iy, &ic, 4);

		sixel_out(&sb_errpic, ix, iy, ic, ib, SIXEL_MUL_PIC);

		nflushcache(&sb_errpic);

		errpic_six_pic = sb_errpic.buf;
		errpic_six_pic[sb_errpic.bufptr] = 0;
	}

	{
		ninitbuf(&sb_errpic);

		int ix, iy, ic;
		stbi_uc *ib = stbi_load_from_memory((const stbi_uc *)err_png, err_png_len, &ix, &iy, &ic, 4);

		sixel_out(&sb_errpic, ix, iy, ic, ib, SIXEL_MUL_ICO);

		nflushcache(&sb_errpic);

		errpic_six_ico = sb_errpic.buf;
		errpic_six_ico[sb_errpic.bufptr] = 0;
	}
}

#ifdef USE_WEBP
stbi_uc *webp_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
	VP8StatusCode ret; /* webp関数の戻り値格納 */
	WebPBitstreamFeatures features; /* 入力webpファイルの情報 */

	/* WebPデータの情報取得 */
	ret = WebPGetFeatures(buffer, len, &features);
	if(ret != VP8_STATUS_OK) {
		return (stbi_uc *)0;
	}

	return WebPDecodeRGBA(buffer, len, x, y);
}
#endif

unsigned int crc32b(unsigned char *message, int maxlen)
{
	int i, j;
	unsigned int byte, crc, mask;

	i = 0;
	crc = 0xFFFFFFFF;
	while(message[i] && i < maxlen) {
		byte = message[i];            // Get next byte.
		crc = crc ^ byte;
		for (j = 7; j >= 0; j--) {    // Do eight times.
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
		i++;
	}
	return ~crc;
}

// outはchar[>17]であること
void generate_hash(char *uri, char *out)
{
	int len = strlen(uri) >> 1;

	unsigned int hash1 = crc32b(uri, len);
	unsigned int hash2 = crc32b(uri + len, len);

	for(int i = 0; i < 4; i++) {
		out[i*2+0] = "0123456789abcdef"[(hash1 >> (i * 8 + 0)) & 15];
		out[i*2+1] = "0123456789abcdef"[(hash1 >> (i * 8 + 4)) & 15];
	}

	for(int i = 0; i < 4; i++) {
		out[i*2+8] = "0123456789abcdef"[(hash2 >> (i * 8 + 0)) & 15];
		out[i*2+9] = "0123456789abcdef"[(hash2 >> (i * 8 + 0)) & 15];
	}
}

void print_picture(sbctx_t *sbctx, char *uri, int mul)
{
	CURL *curl;
	struct rawBuffer *buf;

	char cpath[] = "/                .six";

	generate_hash(uri, cpath + 1);

	strcpy(cpath_ptr, cpath);

	//printf("rel cpath : %s\n", cpath);
	//printf("abs cpath : %s\n", cpath_buffer);

	FILE *cfp;

	if(cfp = fopen(cpath_buffer, "rb")) {
		//nputbuf(sbctx, "cache HIT!\n", 12);

		int len;

		fread(&len, 4, 1, cfp);

		void *p = malloc(len);

		fread(p, len, 1, cfp);

		fclose(cfp);

		nputbuf(sbctx, p, len);

		free(p);

		return;
	}

	//nputbuf(sbctx, "cache MISS.\n", 12);

	buf = (struct rawBuffer *)malloc(sizeof(struct rawBuffer));
	buf->data = NULL;
	buf->data_size = 0;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, uri);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buffer_writer);

	curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	int sl = strlen(uri);

	int ix = 0, iy = 0, ic = 0;
	stbi_uc *ib = (stbi_uc *)0;
	if(uri[sl - 1] != 'p') {
		ib = stbi_load_from_memory(buf->data, buf->data_size, &ix, &iy, &ic, 4);
	} else {
#ifdef USE_WEBP
		ib = webp_load_from_memory(buf->data, buf->data_size, &ix, &iy, &ic, 4);
#endif
	}

	sbctx_t sb2;
	ninitbuf(&sb2);

	if(ix == 0 || iy == 0 || ib == (stbi_uc *)0) {
		if(mul == SIXEL_MUL_PIC) naddstr(&sb2, errpic_six_pic);
		else if(mul == SIXEL_MUL_ICO) naddstr(&sb2, errpic_six_ico);
	} else {
		sixel_out(&sb2, ix, iy, ic, ib, mul);
	}

	nflushcache(&sb2);

	nputbuf(sbctx, sb2.buf, sb2.bufptr);

	FILE *fp = fopen(cpath_buffer, "wb");
	if(fp != 0) {
		fwrite(&(sb2.bufptr), 4, 1, fp);
		fwrite(sb2.buf, sb2.bufptr, 1, fp);
		fclose(fp);
	}

	free(sb2.buf);

	free(buf->data);
	free(buf);
}

void sixel_out(sbctx_t *sbctx, int ix, int iy, int ic, stbi_uc *ib, int mul)
{
	if(ix == 0 || iy == 0 || ib == (stbi_uc *)0) return;

	uint32_t mnsfw = mul & 0x100 ? 0xfffffffc : 0xffffffff;
	uint32_t delta = mul & 0x100 ? 4 : 1;
	mul &= 0xff;

	int sh = 6 * mul;
	int sw = ix * sh / iy;

	if(sh > iy) {
		sh = iy;
		sw = ix;
	}

	if(sw > 6 * mul * 2) {
		sw = 6 * mul * 2;
		sh = iy * sw / ix;
	}

	stbi_uc *sb = (stbi_uc *)malloc(sw * sh * 4);

#ifndef USE_RGB222
	for(int y = 0; y < sh; y++) {
		for(int x = 0; x < sw; x++) {
			memcpy(sb + (y * sw + x) * 4, ib + (((y & mnsfw) * iy / sh) * ix + ((x & mnsfw) * ix / sw)) * 4, 4);
		}
	}
#else
	for(int y = 0; y < sh; y++) {
		for(int x = 0; x < sw; x++) {
			int ay = (y & mnsfw) * iy / sh;
			int by = ((y & mnsfw) + delta) * iy / sh;

			int ax = (x & mnsfw) * ix / sw;
			int bx = ((x & mnsfw) + delta) * ix / sw;

			int cnt = 0;
			int rgba[4] = { 0 };

			for(int v = ay; v < by && v < iy; v++) {
				for(int u = ax; u < bx && u < ix; u++) {
					rgba[0] += (int)ib[(v * ix + u) * 4 + 0];
					rgba[1] += (int)ib[(v * ix + u) * 4 + 1];
					rgba[2] += (int)ib[(v * ix + u) * 4 + 2];
					rgba[3] += (int)ib[(v * ix + u) * 4 + 3];
					cnt++;
				}
			}

			sb[(y * sw + x) * 4 + 0] = CLIP_CH((rgba[0] * 100 + 50) / cnt / 100);
			sb[(y * sw + x) * 4 + 1] = CLIP_CH((rgba[1] * 100 + 50) / cnt / 100);
			sb[(y * sw + x) * 4 + 2] = CLIP_CH((rgba[2] * 100 + 50) / cnt / 100);
			sb[(y * sw + x) * 4 + 3] = CLIP_CH((rgba[3] * 100 + 50) / cnt / 100);
		}
	}
#endif

	stbi_image_free(ib);

	static const int dither_bayer[8][8] = {		// Bayer型のディザ行列
		{0, 32, 8, 40, 2, 34, 10, 42},
		{48, 16, 56, 24, 50, 18, 58, 26},
		{12, 44, 4, 36, 14, 46, 6, 38},
		{60, 28, 52, 20, 62, 30, 54, 22},
		{3, 35, 11, 43, 1, 33, 9, 41},
		{51, 19, 59, 27, 49, 17, 57, 25},
		{15, 47, 7, 39, 13, 45, 5, 37},
		{63, 31, 55, 23, 61, 29, 53, 21}
	};

	naddstr(sbctx, "\eP7;q");

	naddstr(sbctx, palinit_six);

#ifndef USE_RGB222
	int buf_siz;
	if (!monoflag) 
	buf_siz = 8;
	else
	buf_siz = 1;
#else
	const int buf_siz = 64;
#endif
	char *dat;
	int dw = sw + 1;
	dat = (char *)malloc(dw * buf_siz);

	for(int y = 0; y < sh / 6; y++) {
		memset(dat, 0, dw * buf_siz);

		for(int x = 0; x < sw; x++) {
			for(int i = y * 6, j = 0; j < 6; i++, j++) {
#ifndef USE_RGB222
				if (!monoflag) {
					int d =
						((sb[(i * sw + x) * 4 + 0] >> 2) >= dither_bayer[i&7][x&7] ? 1 : 0) |
						((sb[(i * sw + x) * 4 + 1] >> 2) >= dither_bayer[i&7][x&7] ? 2 : 0) |
						((sb[(i * sw + x) * 4 + 2] >> 2) >= dither_bayer[i&7][x&7] ? 4 : 0);
					dat[d * dw + x] |= 1 << j;
				} else {
					int r = sb[(i * sw + x) * 4 + 0];
					int g = sb[(i * sw + x) * 4 + 1];
					int b = sb[(i * sw + x) * 4 + 2];
					dat[x] |= (((((r + b) >> 1) + g) >> 1) >> 2) >= dither_bayer[i&7][x&7] ? 0 : 1 << j;
				}
#else
				int d =
					((CLIP_CH(sb[(i * sw + x) * 4 + 0] + (dither_bayer[i&7][x&7] - 32)) >> 6) << 4) |
					((CLIP_CH(sb[(i * sw + x) * 4 + 1] + (dither_bayer[i&7][x&7] - 32)) >> 6) << 2) |
					((CLIP_CH(sb[(i * sw + x) * 4 + 2] + (dither_bayer[i&7][x&7] - 32)) >> 6) << 0);
				dat[d * dw + x] |= 1 << j;
#endif
			}

			for(int i = 0; i < buf_siz; i++) {
				dat[i * dw + x] += '?';
			}
		}

		for(int i = 0; i < buf_siz; i++) {
			naddch(sbctx, '#');
#ifndef USE_RGB222
			if (!monoflag) {
				naddch(sbctx, '0' + i);
			} else {
				char str[8];
				sprintf(str, "1!%d~$", sw);
				naddstr(sbctx, str);

				naddstr(sbctx, "#0");
			}
#else
			char str[4];
			sprintf(str, "%d", i);
			naddstr(sbctx, str);
#endif

			dat[i * dw + sw] = 0;
			naddstr(sbctx, dat + i * dw);
			naddch(sbctx, '$');
		}
		naddch(sbctx, '-');
	}
	naddstr(sbctx, "\e\\");

	free(dat);
	free(sb);
}

void move_cursor_to_avatar(sbctx_t *sbctx)
{
	char escbuf[2 + 2 + 1 + 1];	/* ESC + [ + 2digits + C + NUL */

	// 1行上に移動
	naddstr(sbctx, "\e[1A");
	// アイコン幅分だけ右に移動
	snprintf(escbuf, sizeof escbuf, "\e[%dC", indent_icon);
	naddstr(sbctx, escbuf);
}

#endif
