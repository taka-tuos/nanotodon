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

static char *errpic_six_pic;
static char *errpic_six_ico;
static char *palinit_six;

static char cpath_buffer[512];
static char *cpath_ptr;

static int indent_icon;
#define DEF_FONT_WIDTH	7
#define DEF_FONT_HEIGHT	14

#define CLIP_CH(n) ((n) > 255 ? 255 : (n) < 0 ? 0 : (n))

#ifdef USE_WEBP
static stbi_uc *webp_load_from_memory(stbi_uc const *, int, int *, int *, int *, int);
#endif
static void sixel_out(sbctx_t *, int, int, int, stbi_uc *, int);
static uint32_t crc32b(const uint8_t *, int);
static void generate_hash(const char *, char *);

void sixel_init(void)
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
static stbi_uc *webp_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
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

static const uint32_t crc32_tbl[256] = {
	0x00000000, 0x77073096,  0xEE0E612C, 0x990951BA,   0x076DC419, 0x706AF48F,  0xE963A535, 0x9E6495A3,
	0x0EDB8832, 0x79DCB8A4,  0xE0D5E91E, 0x97D2D988,   0x09B64C2B, 0x7EB17CBD,  0xE7B82D07, 0x90BF1D91,
	0x1DB71064, 0x6AB020F2,  0xF3B97148, 0x84BE41DE,   0x1ADAD47D, 0x6DDDE4EB,  0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0,  0xFD62F97A, 0x8A65C9EC,   0x14015C4F, 0x63066CD9,  0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E,  0xD56041E4, 0xA2677172,   0x3C03E4D1, 0x4B04D447,  0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C,  0xDBBBC9D6, 0xACBCF940,   0x32D86CE3, 0x45DF5C75,  0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A,  0xC8D75180, 0xBFD06116,   0x21B4F4B5, 0x56B3C423,  0xCFBA9599, 0xB8BDA50F,
	0x2802B89E, 0x5F058808,  0xC60CD9B2, 0xB10BE924,   0x2F6F7C87, 0x58684C11,  0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106,  0x98D220BC, 0xEFD5102A,   0x71B18589, 0x06B6B51F,  0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934,  0x9609A88E, 0xE10E9818,   0x7F6A0DBB, 0x086D3D2D,  0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162,  0x856530D8, 0xF262004E,   0x6C0695ED, 0x1B01A57B,  0x8208F4C1, 0xF50FC457,
	0x65B0D9C6, 0x12B7E950,  0x8BBEB8EA, 0xFCB9887C,   0x62DD1DDF, 0x15DA2D49,  0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE,  0xA3BC0074, 0xD4BB30E2,   0x4ADFA541, 0x3DD895D7,  0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A, 0x346ED9FC,  0xAD678846, 0xDA60B8D0,   0x44042D73, 0x33031DE5,  0xAA0A4C5F, 0xDD0D7CC9,
	0x5005713C, 0x270241AA,  0xBE0B1010, 0xC90C2086,   0x5768B525, 0x206F85B3,  0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998,  0xB0D09822, 0xC7D7A8B4,   0x59B33D17, 0x2EB40D81,  0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6,  0x03B6E20C, 0x74B1D29A,   0xEAD54739, 0x9DD277AF,  0x04DB2615, 0x73DC1683,
	0xE3630B12, 0x94643B84,  0x0D6D6A3E, 0x7A6A5AA8,   0xE40ECF0B, 0x9309FF9D,  0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2,  0x1E01F268, 0x6906C2FE,   0xF762575D, 0x806567CB,  0x196C3671, 0x6E6B06E7,
	0xFED41B76, 0x89D32BE0,  0x10DA7A5A, 0x67DD4ACC,   0xF9B9DF6F, 0x8EBEEFF9,  0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E,  0x38D8C2C4, 0x4FDFF252,   0xD1BB67F1, 0xA6BC5767,  0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C,  0x36034AF6, 0x41047A60,   0xDF60EFC3, 0xA867DF55,  0x316E8EEF, 0x4669BE79,
	0xCB61B38C, 0xBC66831A,  0x256FD2A0, 0x5268E236,   0xCC0C7795, 0xBB0B4703,  0x220216B9, 0x5505262F,
	0xC5BA3BBE, 0xB2BD0B28,  0x2BB45A92, 0x5CB36A04,   0xC2D7FFA7, 0xB5D0CF31,  0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226,  0x756AA39C, 0x026D930A,   0x9C0906A9, 0xEB0E363F,  0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14,  0x7BB12BAE, 0x0CB61B38,   0x92D28E9B, 0xE5D5BE0D,  0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242,  0x68DDB3F8, 0x1FDA836E,   0x81BE16CD, 0xF6B9265B,  0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70,  0x66063BCA, 0x11010B5C,   0x8F659EFF, 0xF862AE69,  0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE,  0x4E048354, 0x3903B3C2,   0xA7672661, 0xD06016F7,  0x4969474D, 0x3E6E77DB,
	0xAED16A4A, 0xD9D65ADC,  0x40DF0B66, 0x37D83BF0,   0xA9BCAE53, 0xDEBB9EC5,  0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A,  0x53B39330, 0x24B4A3A6,   0xBAD03605, 0xCDD70693,  0x54DE5729, 0x23D967BF,
	0xB3667A2E, 0xC4614AB8,  0x5D681B02, 0x2A6F2B94,   0xB40BBE37, 0xC30C8EA1,  0x5A05DF1B, 0x2D02EF8D
};

static uint32_t crc32b(const uint8_t *d, int maxlen)
{
	uint32_t crc = 0xFFFFFFFF;
	const uint8_t *p = d;

	for(int i = 0; *p && i < maxlen; i++, p++) {
		crc = crc32_tbl[(crc ^ *p) & 0xff] ^ (crc >> 8);
	}
	
	return ~crc;
}

// outはchar[>17]であること
static void generate_hash(const char *uri, char *out)
{
	const int len = strlen(uri) >> 1;
	const uint8_t *uribytes = (uint8_t *)uri;

	const uint32_t hash1 = crc32b(uribytes, len);
	const uint32_t hash2 = crc32b(uribytes + len, len);

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

	if((cfp = fopen(cpath_buffer, "rb")) != 0) {
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

static void sixel_out(sbctx_t *sbctx, int ix, int iy, int ic, stbi_uc *ib, int mul)
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
