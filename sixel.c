#include "sixel.h"
#include "utils.h"
#include <curl/curl.h>

#ifdef USE_SIXEL
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void print_picture(sbctx_t *sbctx, char *uri, int mul)
{
	CURL *curl;
    struct rawBuffer *buf;

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

	int ix, iy, ic;
    stbi_uc *ib = stbi_load_from_memory(buf->data, buf->data_size, &ix, &iy, &ic, 4);

	if(ix == 0 || iy == 0 || ib == (stbi_uc *)0) {
		ib = stbi_load("err.png", &ix, &iy, &ic, 4);
	}

	if(ix == 0 || iy == 0 || ib == (stbi_uc *)0) return;

	int sh = 6 * mul;
	int sw = ix * sh / iy;
	/*int sh = iy;
	int sw = ix;*/

	if(sw > 6 * mul * 2) {
		sw = 6 * mul * 2;
		sh = iy * sw / ix;
	}

	stbi_uc *sb = (stbi_uc *)malloc(sw * sh * 4);

	for(int y = 0; y < sh; y++) {
		for(int x = 0; x < sw; x++) {
			memcpy(sb + (y * sw + x) * 4, ib + ((y * iy / sh) * ix + (x * ix / sw)) * 4, 4);
		}
	}

	stbi_image_free(ib);

	int dither_bayer[4][4] = {		// Bayer型のディザ行列
		{ 0,  8,  2, 10},
		{12,  4, 14,  6},
		{ 3, 11,  1,  9},
		{15,  7, 13,  5}
	};

	naddstr(sbctx, "\ePq");	

#ifndef USE_RGB222
#ifndef MONO_SIXEL
	naddstr(sbctx, "#0;2;0;0;0");
	naddstr(sbctx, "#1;2;100;0;0");
	naddstr(sbctx, "#2;2;0;100;0");
	naddstr(sbctx, "#3;2;100;100;0");
	naddstr(sbctx, "#4;2;0;0;100");
	naddstr(sbctx, "#5;2;100;0;100");
	naddstr(sbctx, "#6;2;0;100;100");
	naddstr(sbctx, "#7;2;100;100;100");
#else 
	naddstr(sbctx, "#0;2;0;0;0");
	naddstr(sbctx, "#1;2;100;100;100");
#endif
#else
	for(int i = 0; i < 64; i++) {
		char str[256];

		int r = (i >> 4) & 3;
		int g = (i >> 2) & 3;
		int b = (i >> 0) & 3;

		int tbl[] = {0,33,66,100};

		sprintf(str, "#%d;2;%d;%d;%d", i, tbl[r], tbl[g], tbl[b]);
		naddstr(sbctx, str);
	}
#endif

#ifndef USE_RGB222
#ifndef MONO_SIXEL
	const int buf_siz = 8;
#else
	const int buf_siz = 1;
#endif
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
#ifndef MONO_SIXEL
				int d = 
					((sb[(i * sw + x) * 4 + 0] >> 4) >= dither_bayer[i&3][x&3] ? 1 : 0) |
					((sb[(i * sw + x) * 4 + 1] >> 4) >= dither_bayer[i&3][x&3] ? 2 : 0) |
					((sb[(i * sw + x) * 4 + 2] >> 4) >= dither_bayer[i&3][x&3] ? 4 : 0);
				dat[d * dw + x] |= 1 << j;
#else
				int r = sb[(i * sw + x) * 4 + 0];
				int g = sb[(i * sw + x) * 4 + 1];
				int b = sb[(i * sw + x) * 4 + 2];
				dat[x] |= (((((r + b) >> 1) + b) >> 1) >> 4) >= dither_bayer[i&3][x&3] ? 1 << j : 0;
#endif
#else
				int d = 
					((sb[(i * sw + x) * 4 + 0] >> 6) << 4) |
					((sb[(i * sw + x) * 4 + 1] >> 6) << 2) |
					((sb[(i * sw + x) * 4 + 2] >> 6) << 0);
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
#ifndef MONO_SIXEL
			naddch(sbctx, '0' + i);
#else
			naddstr(sbctx, "0!");
			char str[4];
			sprintf(str, "%d", sw);
			naddstr(sbctx, str);

			naddch(sbctx, '$');
			naddch(sbctx, '#');
			naddch(sbctx, '1');
#endif
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
    free(buf->data);
    free(buf);
}

#endif