#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

extern char domain_string[256];

// Unicode文字列の幅を返す(半角文字=1)
int ustrwidth(const char *str)
{
	int size, width, strwidth;

	strwidth = 0;
	while (*str != '\0') {
		uint8_t c;
		c = (uint8_t)*str;
		if (c >= 0x00 && c <= 0x7f) {
			size  = 1;
			width = 1;
		} else if (c >= 0xc2 && c <= 0xdf) {
			size  = 2;
			width = 2;
		} else if (c == 0xef) {
			uint16_t p;
			p = ((uint8_t)str[1] << 8) | (uint8_t)str[2];
			size  = 3;
			if (p >= 0xbda1 && p <= 0xbe9c) {
				/* Halfwidth CJK punctuation */
				/* Halfwidth Katakana variants */
				/* Halfwidth Hangul variants */
				width = 1;
			} else if (p >= 0xbfa8 && p <= 0xbfae) {
				/* Halfwidth symbol variants */
				width = 1;
			} else {
				/* other BMP */
				width = 2;
			}
		} else if ((c & 0xf0) == 0xe0) {
			/* other BMP */
			size  = 3;
			width = 2;
		} else if ((c & 0xf8) == 0xf0) {
			/* Emoji etc. */
			size  = 4;
			width = 2;
		} else {
			/* unexpected */
			size  = 1;
			width = 1;
		}
		strwidth += width;
		str += size;
	}
	return strwidth;
}

// curlのエラーを表示
void curl_fatal(CURLcode ret, const char *errbuf)
{
	size_t len = strlen(errbuf);
	//endwin();
	fprintf(stderr, "\n");
	if(len>0) {
		fprintf(stderr, "%s%s", errbuf, errbuf[len-1]!='\n' ? "\n" : "");
	}else{
		fprintf(stderr, "%s\n", curl_easy_strerror(ret));
	}
	exit(EXIT_FAILURE);
}

// domain_stringとapiエンドポイントを合成してURLを生成する
char *create_uri_string(char *api)
{
	char *s = malloc(256);
	sprintf(s, "https://%s/%s", domain_string, api);
	return s;
}

sjson_node *read_json_from_file(char *path, char **json_p, sjson_context **ctx_p)
{
	char *json;
	FILE *f = fopen(path, "rb");

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	json = malloc(fsize + 1);
	*json_p = json;

	fread(json, fsize, 1, f);
	fclose(f);

	json[fsize] = 0;

	sjson_context* ctx = sjson_create_context(0, 0, NULL);
	*ctx_p = ctx;

	struct sjson_node *jobj_from_string = sjson_decode(ctx, json);

	return jobj_from_string;
}

// jsonツリーをパス形式(ex. "account/display_name")で掘ってjson_objectを取り出す
int read_json_fom_path(struct sjson_node *obj, char *path, struct sjson_node **dst)
{
	char *dup = strdup(path);	// strtokは破壊するので複製
	struct sjson_node *dir = obj;
	int exist = 1;
	char *next_key;
	char last_key[256];

	char *tok = dup;

	// 現在地ノードが存在する限りループ
	while(exist) {
		// 次のノード名を取り出す
		next_key = strtok(tok, "/");
		tok = NULL;

		// パスの終端(=目的のオブジェクトに到達している)ならループを抜ける
		if(!next_key) break;
		strcpy(last_key, next_key);

		// 次のノードを取得する

		struct sjson_node *next = sjson_find_member(dir, next_key);

		exist = next != 0 ? 1 : 0;

		if(exist) {
			// 存在しているので現在地ノードを更新
			dir = next;
		}
	}

	// strtok用バッファ解放
	free(dup);

	// 現在地を結果ポインタに代入
	*dst = dir;

	// 見つかったかどうかを返却
	return exist;
}

// curlのコールバック
size_t buffer_writer(char *ptr, size_t size, size_t nmemb, void *stream) {
    struct rawBuffer *buf = (struct rawBuffer *)stream;
    int block = size * nmemb;
    if (!buf) {
        return block;
    }

    if (!buf->data) {
        buf->data = malloc(block);
    }
    else {
        buf->data = realloc(buf->data, buf->data_size + block);
    }

    if (buf->data) {
        memcpy(buf->data + buf->data_size, ptr, block);
        buf->data_size += block;
    }

    return block;
}
