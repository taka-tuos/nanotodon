#include <curl/curl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // memmove
#include <ctype.h>  // isspace
#include <locale.h> // setlocale
#include <curses.h>
#include <ncurses.h>

char *streaming_json = NULL;

#define URI "api/v1/streaming/user"

void (*streaming_recieved_handler)(void);
void (*stream_event_handler)(struct json_object *);

WINDOW *scr;
WINDOW *pad;

char access_token[256];
char domain_string[256];

int term_w, term_h;
int pad_x = 0, pad_y = 0;

char *create_uri_string(char *api)
{
	char *s = malloc(256);
	sprintf(s, "https://%s/%s", domain_string, api);
	return s;
}

int read_json_fom_path(struct json_object *obj, char *path, struct json_object **dst)
{
	char *dup = strdup(path);
	struct json_object *dir = obj;
	int exist = 1;
	char *next_key;
	char last_key[256];
	
	char *tok = dup;
	
	while(exist) {
		next_key = strtok(tok, "/");
		tok = NULL;
		if(!next_key) break;
		strcpy(last_key, next_key);
		struct json_object *next;
		exist = json_object_object_get_ex(dir, next_key, &next);
		if(exist) {
			dir = next;
		}
	}
	
	free(dup);
	
	*dst = dir;
	
	return exist;
}

size_t streaming_callback(void* ptr, size_t size, size_t nmemb, void* data) {
	if (size * nmemb == 0)
		return 0;
	
	char **json = ((char **)data);
	
	size_t realsize = size * nmemb;
	
	size_t length = realsize + 1;
	char *str = *json;
	str = realloc(str, (str ? strlen(str) : 0) + length);
	if(*((char **)data) == NULL) strcpy(str, "");
	
	*json = str;
	
	if (str != NULL) {
		strncat(str, ptr, realsize);
		if(str[strlen(str)-1] == 0x0a) {
			if(*str == ':') {
				free(str);
				*json = NULL;
			} else {
				streaming_recieved_handler();
			}
		}
	}

	return realsize;
}

void stream_event_notify(struct json_object *jobj_from_string)
{
	struct json_object *notify_type, *screen_name, *display_name, *status;
	if(!jobj_from_string) return;
	read_json_fom_path(jobj_from_string, "type", &notify_type);
	read_json_fom_path(jobj_from_string, "account/acct", &screen_name);
	read_json_fom_path(jobj_from_string, "account/display_name", &display_name);
	int exist_status = read_json_fom_path(jobj_from_string, "status", &status);
	
	putchar('\a');
	
	//FILE *fp = fopen("json.log", "a+");
	
	//fputs(json_object_to_json_string(jobj_from_string), fp);
	//fputs("\n\n", fp);
	//fclose(fp);
	
	char *t = json_object_get_string(notify_type);
	t[0] = toupper(t[0]);
	
	wattron(scr, COLOR_PAIR(4));
	waddstr(scr, strcmp(t, "Follow") == 0 ? "👥" : strcmp(t, "Favourite") == 0 ? "💕" : strcmp(t, "Reblog") == 0 ? "🔃" : strcmp(t, "Mention") == 0 ? "🗨" : "");
	waddstr(scr, t);
	waddstr(scr, " from ");
	waddstr(scr, json_object_get_string(screen_name));
	waddstr(scr, "(");
	waddstr(scr, json_object_get_string(display_name));
	waddstr(scr, ")\n");
	wattroff(scr, COLOR_PAIR(4));
	
	enum json_type type;
	
	type = json_object_get_type(status);
	
	if(type != json_type_null && exist_status) {
		stream_event_update(status);
	}
	
	waddstr(scr, "\n");
	wrefresh(scr);
	
	wmove(pad, pad_x, pad_y);
	wrefresh(pad);
}

void stream_event_update(struct json_object *jobj_from_string)
{
	struct json_object *content, *screen_name, *display_name, *reblog;
	//struct json_object *jobj_from_string = json_tokener_parse(json);
	if(!jobj_from_string) return;
	read_json_fom_path(jobj_from_string, "content", &content);
	read_json_fom_path(jobj_from_string, "account/acct", &screen_name);
	read_json_fom_path(jobj_from_string, "account/display_name", &display_name);
	read_json_fom_path(jobj_from_string, "reblog", &reblog);
	
	//FILE *fp = fopen("json.log", "a+");
	
	//fputs(json_object_to_json_string(jobj_from_string), fp);
	//fputs("\n\n", fp);
	//fclose(fp);
	
	enum json_type type;
	
	type = json_object_get_type(reblog);
	
	if(type != json_type_null) {
		wattron(scr, COLOR_PAIR(3));
		waddstr(scr, "🔃 Reblog by ");
		waddstr(scr, json_object_get_string(screen_name));
		waddstr(scr, "(");
		waddstr(scr, json_object_get_string(display_name));
		waddstr(scr, ")\n");
		wattroff(scr, COLOR_PAIR(3));
		stream_event_update(reblog);
		return;
	}
	
	wattron(scr, COLOR_PAIR(1));
	waddstr(scr, json_object_get_string(screen_name));
	wattroff(scr, COLOR_PAIR(1));
	wattron(scr, COLOR_PAIR(2));
	waddstr(scr, "(");
	waddstr(scr, json_object_get_string(display_name));
	waddstr(scr, ")");
	wattroff(scr, COLOR_PAIR(2));
	waddstr(scr, "\n");
	
	char *src = json_object_get_string(content);
	
	/*waddstr(scr, src);
	waddstr(scr, "\n");*/
	
	int ltgt = 0;
	while(*src) {
		if(*src == '<') ltgt = 1;
		if(ltgt && strncmp(src, "<br", 3) == 0) waddch(scr, '\n');
		if(!ltgt) {
			if(*src == '&') {
				if(strncmp(src, "&amp;", 5) == 0) {
					waddch(scr, '&');
					src += 4;
				}
				else if(strncmp(src, "&lt;", 4) == 0) {
					waddch(scr, '<');
					src += 3;
				}
				else if(strncmp(src, "&gt;", 4) == 0) {
					waddch(scr, '>');
					src += 3;
				}
				else if(strncmp(src, "&quot;", 6) == 0) {
					waddch(scr, '\"');
					src += 5;
				}
				else if(strncmp(src, "&apos;", 6) == 0) {
					waddch(scr, '\'');
					src += 5;
				}
			} else {
				waddch(scr, *((unsigned char *)src));
			}
		}
		if(*src == '>') ltgt = 0;
		src++;
	}
	
	waddstr(scr, "\n");
	
	struct json_object *media_attachments;
	
	read_json_fom_path(jobj_from_string, "media_attachments", &media_attachments);
	
	if(json_object_is_type(media_attachments, json_type_array)) {
		for (int i = 0; i < json_object_array_length(media_attachments); ++i) {
			struct json_object *obj = json_object_array_get_idx(media_attachments, i);
			struct json_object *url;
			read_json_fom_path(obj, "url", &url);
			if(json_object_is_type(url, json_type_string)) {
				waddstr(scr, "🔗");
				waddstr(scr, json_object_get_string(url));
				waddstr(scr, "\n");
			}
		}
	}
	struct json_object *application_name;
	int exist_appname = read_json_fom_path(jobj_from_string, "application/name", &application_name);
	
	if(exist_appname) {
		type = json_object_get_type(application_name);
		
		if(type != json_type_null) {
			int l = strlen(json_object_get_string(application_name));
		
			for(int i = 0; i < term_w - (l + 4); i++) waddstr(scr, " ");
			
			wattron(scr, COLOR_PAIR(1));
			waddstr(scr, "via ");
			wattroff(scr, COLOR_PAIR(1));
			wattron(scr, COLOR_PAIR(2));
			waddstr(scr, json_object_get_string(application_name));
			//waddstr(scr, "\n");
			wattroff(scr, COLOR_PAIR(2));
		}
	}
	
	waddstr(scr, "\n");
	wrefresh(scr);
	
	wmove(pad, pad_x, pad_y);
	wrefresh(pad);
	
	//json_object_put(jobj_from_string);
}

char **json_recieved = NULL;
int json_recieved_len = 0;

void streaming_recieved(void)
{
	json_recieved = realloc(json_recieved, (json_recieved_len + 1) * sizeof(char *));
	json_recieved[json_recieved_len] = strdup(streaming_json);
	
	if(strncmp(streaming_json, "event", 5) == 0) {
		char *type = strdup(streaming_json + 7);
		if(strncmp(type, "update", 6) == 0) stream_event_handler = stream_event_update;
		else if(strncmp(type, "notification", 12) == 0) stream_event_handler = stream_event_notify;
		else stream_event_handler = NULL;
		
		char *top = type;
		while(*type != '\n') type++;
		type++;
		if(*type != 0) {
			free(streaming_json);
			streaming_json = strdup(type);
		}
		free(top);
	}
	if(strncmp(streaming_json, "data", 4) == 0) {
		if(stream_event_handler) {
			struct json_object *jobj_from_string = json_tokener_parse(streaming_json + 6);
			stream_event_handler(jobj_from_string);
			json_object_put(jobj_from_string);
			stream_event_handler = NULL;
		}
	}
	
	free(streaming_json);
	streaming_json = NULL;
}

void *stream_thread_func(void *param)
{
	do_htl();
	
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);

	char *uri = create_uri_string(URI);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 0);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)&streaming_json);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, streaming_callback);
	
	streaming_recieved_handler = streaming_recieved;
	stream_event_handler = NULL;
	
	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
	
	return NULL;
}

#define STB_TEXTEDIT_CHARTYPE   wchar_t
#define STB_TEXTEDIT_STRING     text_control

// get the base type
#include "stb_textedit.h"

// define our editor structure
typedef struct
{
	wchar_t *string;
	int stringlen;
} text_control;

// define the functions we need
void layout_func(StbTexteditRow *row, STB_TEXTEDIT_STRING *str, int start_i)
{
	int remaining_chars = str->stringlen - start_i;
	row->num_chars = remaining_chars > 20 ? 20 : remaining_chars; // should do real word wrap here
	row->x0 = 0;
	row->x1 = 20; // need to account for actual size of characters
	row->baseline_y_delta = 1.25;
	row->ymin = -1;
	row->ymax = 0;
}

int delete_chars(STB_TEXTEDIT_STRING *str, int pos, int num)
{
	memmove(&str->string[pos], &str->string[pos + num], (str->stringlen - (pos + num)) * sizeof(wchar_t));
	str->stringlen -= num;
	return 1;
}

int insert_chars(STB_TEXTEDIT_STRING *str, int pos, STB_TEXTEDIT_CHARTYPE *newtext, int num)
{
	str->string = (wchar_t *)realloc(str->string, (str->stringlen + num) * sizeof(wchar_t));
	memmove(&str->string[pos + num], &str->string[pos], (str->stringlen - pos) * sizeof(wchar_t));
	memcpy(&str->string[pos], newtext, (num) * sizeof(wchar_t));
	str->stringlen += num;
	return 1;
}

// define all the #defines needed 

#define STB_TEXTEDIT_STRINGLEN(tc)     ((tc)->stringlen)
#define STB_TEXTEDIT_LAYOUTROW         layout_func
#define STB_TEXTEDIT_GETWIDTH(tc,n,i)  (1) // quick hack for monospaced
#define STB_TEXTEDIT_KEYTOTEXT(key)    (key)
#define STB_TEXTEDIT_GETCHAR(tc,i)     ((tc)->string[i])
#define STB_TEXTEDIT_NEWLINE           '\n'
#define STB_TEXTEDIT_IS_SPACE(ch)      isspace(ch)
#define STB_TEXTEDIT_DELETECHARS       delete_chars
#define STB_TEXTEDIT_INSERTCHARS       insert_chars

#define STB_TEXTEDIT_K_SHIFT           0xffff0100
#define STB_TEXTEDIT_K_CONTROL         0xffff0200
#define STB_TEXTEDIT_K_LEFT            KEY_LEFT
#define STB_TEXTEDIT_K_RIGHT           KEY_RIGHT
#define STB_TEXTEDIT_K_UP              KEY_UP
#define STB_TEXTEDIT_K_DOWN            KEY_DOWN
#define STB_TEXTEDIT_K_LINESTART       KEY_HOME
#define STB_TEXTEDIT_K_LINEEND         KEY_END
#define STB_TEXTEDIT_K_TEXTSTART       KEY_SHOME
#define STB_TEXTEDIT_K_TEXTEND         KEY_SEND
#define STB_TEXTEDIT_K_DELETE          KEY_DC
#define STB_TEXTEDIT_K_BACKSPACE       0x7f
#define STB_TEXTEDIT_K_UNDO            KEY_UNDO
#define STB_TEXTEDIT_K_REDO            KEY_REDO
#define STB_TEXTEDIT_K_INSERT          0xffff0400
#define STB_TEXTEDIT_K_WORDLEFT        0xffff0800
#define STB_TEXTEDIT_K_WORDRIGHT       0xffff1000
#define STB_TEXTEDIT_K_PGUP            KEY_PPAGE
#define STB_TEXTEDIT_K_PGDOWN          KEY_NPAGE

#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

void do_create_client(char *domain)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	
	char json_name[256], *uri;
	
	sprintf(json_name, "%s.ckcs", domain);
	
	uri = create_uri_string("api/v1/apps");
	
	FILE *f = fopen(json_name, "wb");

	post1 = NULL;
	postend = NULL;
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "client_name",
				CURLFORM_COPYCONTENTS, "nanotodon",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "redirect_uris",
				CURLFORM_COPYCONTENTS, "urn:ietf:wg:oauth:2.0:oob",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "scopes",
				CURLFORM_COPYCONTENTS, "read write follow",
				CURLFORM_END);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);
	
	ret = curl_easy_perform(hnd);
	
	fclose(f);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
}

void do_oauth(char *code, char *ck, char *cs)
{
	char fields[512];
	sprintf(fields, "client_id=%s&client_secret=%s&grant_type=authorization_code&code=%s&scope=read%20write%20follow", ck, cs, code);
	
	FILE *f = fopen(".nanotter", "wb");
	
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;

	post1 = NULL;
	postend = NULL;
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "grant_type",
				CURLFORM_COPYCONTENTS, "authorization_code",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "redirect_uri",
				CURLFORM_COPYCONTENTS, "urn:ietf:wg:oauth:2.0:oob",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "client_id",
				CURLFORM_COPYCONTENTS, ck,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "client_secret",
				CURLFORM_COPYCONTENTS, cs,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "code",
				CURLFORM_COPYCONTENTS, code,
				CURLFORM_END);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, create_uri_string("oauth/token"));
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);
	
	ret = curl_easy_perform(hnd);
	
	fclose(f);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
}

void do_toot(char *s)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	struct curl_slist *slist1;
	
	FILE *f = fopen("/dev/null", "wb");
	
	char *uri = create_uri_string("api/v1/statuses");

	post1 = NULL;
	postend = NULL;
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "status",
				CURLFORM_COPYCONTENTS, s,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "visibility",
				CURLFORM_COPYCONTENTS, "public",
				CURLFORM_END);
	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);

	ret = curl_easy_perform(hnd);

	fclose(f);
	
	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
}

size_t htl_callback(void* ptr, size_t size, size_t nmemb, void* data) {
	if (size * nmemb == 0)
		return 0;
	
	char **json = ((char **)data);
	
	size_t realsize = size * nmemb;
	
	size_t length = realsize + 1;
	char *str = *json;
	str = realloc(str, (str ? strlen(str) : 0) + length);
	if(*((char **)data) == NULL) strcpy(str, "");
	
	*json = str;
	
	if (str != NULL) {
		strncat(str, ptr, realsize);
	}

	return realsize;
}

void do_htl()
{
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);
	
	char *json = NULL;

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, create_uri_string("api/v1/timelines/home"));
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)&json);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, htl_callback);
	
	ret = curl_easy_perform(hnd);
	
	struct json_object *jobj_from_string = json_tokener_parse(json);
	enum json_type type;
	
	type = json_object_get_type(jobj_from_string);
	
	if(type == json_type_array) {
		for (int i = json_object_array_length(jobj_from_string) - 1; i >= 0; i--) {
			struct json_object *obj = json_object_array_get_idx(jobj_from_string, i);
			
			stream_event_update(obj);
			
			char *p = strdup(json_object_to_json_string(obj));
			
			//FILE *fp = fopen("rest_json.log","wt");
			//fputs(p, fp);
			//fclose(fp);
		}
	}
	
	json_object_put(jobj_from_string);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
}

int main(int argc, char *argv[])
{
	FILE *fp = fopen(".nanotter", "rb");
	if(fp) {
		fclose(fp);
		struct json_object *token;
		struct json_object *jobj_from_file = json_object_from_file(".nanotter");
		read_json_fom_path(jobj_from_file, "access_token", &token);
		sprintf(access_token, "Authorization: Bearer %s", json_object_get_string(token));
		FILE *f2 = fopen(".nanotter2", "rb");
		fscanf(f2, "%255s", domain_string);
		fclose(f2);
	} else {
		char domain[256];
		char key[256];
		char *ck;
		char *cs;
		printf("はじめまして！ようこそnaotodonへ!\n");
		printf("最初に、");
retry1:
		printf("あなたのいるインスタンスを教えてね。\n(https://[ここを入れてね]/)\n");
		printf(">");
		scanf("%255s", domain);
		printf("\n");
		
		FILE *f2 = fopen(".nanotter2", "wb");
		fprintf(f2, "%s", domain);
		fclose(f2);
		
		char json_name[256];
		sprintf(json_name, "%s.ckcs", domain);
		strcpy(domain_string, domain);
		FILE *ckcs = fopen(json_name, "rb");
		if(!ckcs) {
			do_create_client(domain);
		} else {
			fclose(ckcs);
		}
		
		struct json_object *cko, *cso;
		struct json_object *jobj_from_file = json_object_from_file(json_name);
		int r1 = read_json_fom_path(jobj_from_file, "client_id", &cko);
		int r2 = read_json_fom_path(jobj_from_file, "client_secret", &cso);
		if(!r1 || !r2) {
			printf("何かがおかしいみたいだよ。\nもう一度やり直すね。");
			remove(json_name);
			remove(".nanotter2");
			goto retry1;
		}
		ck = strdup(json_object_get_string(cko));
		cs = strdup(json_object_get_string(cso));
		
		char code[256];
		
		printf("次に、アプリケーションの認証をするよ。\n");
		printf("下に表示されるURLにアクセスして承認をしたら表示されるコードを入力してね。\n");
		printf("https://%s/oauth/authorize?client_id=%s&response_type=code&redirect_uri=urn:ietf:wg:oauth:2.0:oob&scope=read%%20write%%20follow\n", domain, ck);
		printf(">");
		scanf("%255s", code);
		printf("\n");
		do_oauth(code, ck, cs);
		struct json_object *token;
		jobj_from_file = json_object_from_file(".nanotter");
		int r3 = read_json_fom_path(jobj_from_file, "access_token", &token);
		if(!r3) {
			printf("何かがおかしいみたいだよ。\n入力したコードはあっているかな？\nもう一度やり直すね。");
			remove(json_name);
			remove(".nanotter2");
			remove(".nanotter");
			goto retry1;
		}
		sprintf(access_token, "Authorization: Bearer %s", json_object_get_string(token));
		printf("これでおしまい!\nnanotodonライフを楽しんでね!\n");
	}
	
	setlocale(LC_ALL, "");
	
	WINDOW *term = initscr();
	
	start_color();
	
	use_default_colors();

	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_CYAN, -1);
	init_pair(3, COLOR_YELLOW, -1);
	init_pair(4, COLOR_RED, -1);
	
	getmaxyx(term, term_h, term_w);
	
	scr = newwin(term_h - 6, term_w, 6, 0);
	
	pad = newwin(5, term_w, 0, 0);
	
	scrollok(scr, 1);
	
	wrefresh(scr);
	
	pthread_t stream_thread;
	
	pthread_create(&stream_thread, NULL, stream_thread_func, NULL);
	
	STB_TexteditState state;
	text_control txt;

	txt.string = 0;
	txt.stringlen = 0;

	stb_textedit_initialize_state(&state, 0);
	
	keypad(pad, TRUE);
	noecho();
	
	attron(COLOR_PAIR(2));
	for(int i = 0; i < term_w; i++) mvaddch(5, i, '-');
	attroff(COLOR_PAIR(2));
	refresh();
	
	/*mvaddch(0, term_w/2, '[');
	attron(COLOR_PAIR(1));
	addstr("toot欄(escで投稿)");
	attroff(COLOR_PAIR(1));
	mvaddch(0, term_w-1, ']');
	mvaddch(0, 0, '[');
	attron(COLOR_PAIR(2));
	addstr("Timeline(");
	addstr(URI);
	addstr(")");
	attroff(COLOR_PAIR(2));
	mvaddch(0, term_w/2-1, ']');
	refresh();
	wmove(pad, 0, 0);*/
	
	while (1)
	{
		wchar_t c;
		wget_wch(pad, &c);
		if(c == KEY_RESIZE) {
			getmaxyx(term, term_h, term_w);
			attron(COLOR_PAIR(2));
			for(int i = 0; i < term_w; i++) mvaddch(5, i, '-');
			attroff(COLOR_PAIR(2));
			refresh();
			werase(scr);
			wresize(scr, term_h - 6, term_w);
			wresize(pad, 5, term_w);
			do_htl();
			wrefresh(pad);
			wrefresh(scr);
		} else if(c == 0x1b && txt.string) {
			werase(pad);
			wchar_t *text = malloc(sizeof(wchar_t) * (txt.stringlen + 1));
			memcpy(text, txt.string, sizeof(wchar_t) * txt.stringlen);
			text[txt.stringlen] = 0;
			char status[1024];
			wcstombs(status, text, 1024);
			do_toot(status);
			txt.string = 0;
			txt.stringlen = 0;
		} else {
			stb_textedit_key(&txt, &state, c);
		}
		werase(pad);
		wmove(pad, 0, 0);
		int cx=-1, cy=-1;
		for(int i = 0; i < txt.stringlen; i++) {
			if(i == state.cursor) getyx(pad, cx, cy);
			wchar_t s[2];
			char mb[8];
			s[0] = txt.string[i];
			s[1] = 0;
			wcstombs(mb, s, 8);
			waddstr(pad, mb);
		}
		if(cx>=0&&cy>=0) {
			wmove(pad, cx, cy);
			pad_x = cx;
			pad_y = cy;
		} else {
			pad_x = 0;
			pad_y = 0;
		}
		wrefresh(pad);
	}

	return 0;
}
