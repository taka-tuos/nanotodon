#include <curl/curl.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h> // memmove
#include <time.h>   // strptime, strptime, timegm, localtime
#include <ctype.h>  // isspace
#include <locale.h> // setlocale
#include <sys/time.h>
#include <pthread.h>

#define SJSON_IMPLEMENT
#include "sjson.h"

#include "config.h"
#include "messages.h"
#include "sbuf.h"
#include "squeue.h"
#include "utils.h"
#include "sixel.h"

char *streaming_json = NULL;

#define URI_STREAM "api/v1/streaming/"
#define URI_TIMELINE "api/v1/timelines/"

char *selected_stream = "user";
char *selected_timeline = "home";

#define CURL_USERAGENT "curl/" LIBCURL_VERSION

pthread_mutex_t prompt_mutex;
int prompt_notify = 0;

// ストリーミングを受信する関数のポインタ
void (*streaming_received_handler)(void);

// 受信したストリーミングを処理する関数のポインタ
void (*stream_event_handler)(sbctx_t *sbctx, struct sjson_node *);

// インスタンスにクライアントを登録する
void do_create_client(char *, char *);

// Timelineの受信
void get_timeline(void);

// 承認コードを使ったOAuth処理
void do_oauth(char *code, char *ck, char *cs);

// Tootを行う
void do_toot(char *);

// ストリーミングでのToot受信処理,stream_event_handlerへ代入
void stream_event_update(sbctx_t *sbctx, struct sjson_node *);

// ストリーミングでの通知受信処理,stream_event_handlerへ代入
void stream_event_notify(sbctx_t *sbctx, struct sjson_node *);

// アクセストークン文字列
char access_token[256];

// ドメイン文字列
char domain_string[256];

// コンフィグファイルパス構造体
struct nanotodon_config config;

int monoflag = 0;
int hidlckflag = 1;
int noemojiflag = 0;

// curlから呼び出されるストリーミング受信関数
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
		// 改行が来たらデータ終端(一回の受信に収まるとは限らない)
		if(str[strlen(str)-1] == 0x0a) {
			if(*str == ':') {
				// ':'だけは接続維持用
				free(str);
				*json = NULL;
			} else {
				streaming_received_handler();
			}
		}
	}

	return realsize;
}

// ストリーミングでの通知受信処理,stream_event_handlerへ代入
void stream_event_notify(sbctx_t *sbctx, sjson_node *jobj_from_string)
{
	struct sjson_node *notify_type, *screen_name, *display_name, *status;
	const char *dname;
	if(!jobj_from_string) return;
	read_json_fom_path(jobj_from_string, "type", &notify_type);
	read_json_fom_path(jobj_from_string, "account/acct", &screen_name);
	read_json_fom_path(jobj_from_string, "account/display_name", &display_name);
	int exist_status = read_json_fom_path(jobj_from_string, "status", &status);
	
	putchar('\a');
	
	// 通知種別を表示に流用するので先頭を大文字化
	char *t = strdup(notify_type->string_);
	t[0] = toupper((int)(unsigned char)t[0]);
	
	// 通知種別と誰からか[ screen_name(display_name) ]を表示
	nattron(sbctx, COLOR_PAIR(4));
	if(!noemojiflag) naddstr(sbctx, strcmp(t, "Follow") == 0 ? "👥" : strcmp(t, "Favourite") == 0 ? "💕" : strcmp(t, "Reblog") == 0 ? "🔃" : strcmp(t, "Mention") == 0 ? "🗨" : "");
	naddstr(sbctx, t);
	free(t);
	naddstr(sbctx, " from ");
	naddstr(sbctx, screen_name->string_);
	
	dname = display_name->string_;
	
	// dname(display_name)が空の場合は括弧を表示しない
	if (dname[0] != '\0') {
		naddstr(sbctx, " (");
		naddstr(sbctx, dname);
		naddstr(sbctx, ")");
	}
	naddstr(sbctx, "\n");
	nattroff(sbctx, COLOR_PAIR(4));
	
	sjson_tag type;
	
	type = status->tag;
	
	// 通知対象のTootを表示,Follow通知だとtypeがNULLになる
	if(type != SJSON_NULL && exist_status) {
		stream_event_update(sbctx, status);
	}
	
	naddstr(sbctx, "\n");

	//wrefresh(scr);
	
	/*wmove(pad, pad_x, pad_y);
	wrefresh(pad);*/
}

// ストリーミングでのToot受信処理,stream_event_handlerへ代入
#define DATEBUFLEN	40
void stream_event_update(sbctx_t *sbctx, struct sjson_node *jobj_from_string)
{
	struct sjson_node *content, *screen_name, *display_name, *avatar, *reblog, *visibility;
	const char *sname, *dname, *vstr;
	struct sjson_node *created_at;
	struct tm tm;
	time_t time;
	char datebuf[DATEBUFLEN];
	if(!jobj_from_string) return;
	read_json_fom_path(jobj_from_string, "content", &content);
	read_json_fom_path(jobj_from_string, "account/acct", &screen_name);
	read_json_fom_path(jobj_from_string, "account/display_name", &display_name);
	read_json_fom_path(jobj_from_string, "account/avatar", &avatar);
	read_json_fom_path(jobj_from_string, "reblog", &reblog);
	read_json_fom_path(jobj_from_string, "created_at", &created_at);
	read_json_fom_path(jobj_from_string, "visibility", &visibility);
	memset(&tm, 0, sizeof(tm));
	strptime(created_at->string_, "%Y-%m-%dT%H:%M:%S", &tm);
	time = timegm(&tm);
	strftime(datebuf, sizeof(datebuf), "%x(%a) %X", localtime(&time));
	
	vstr = visibility->string_;
	
	if(hidlckflag) {
		if(!strcmp(vstr, "private") || !strcmp(vstr, "direct")) {
			return;
		}
	}
	
	sjson_tag type;
	
	type = reblog->tag;
	sname = screen_name->string_;
	dname = display_name->string_;
	
	// ブーストで回ってきた場合はその旨を表示
	if(type != SJSON_NULL) {
		nattron(sbctx,  COLOR_PAIR(3));
		if(!noemojiflag) naddstr(sbctx,  "🔃 ");
		naddstr(sbctx,  "Reblog by ");
		naddstr(sbctx,  sname);
		// dname(表示名)が空の場合は括弧を表示しない
		if (dname[0] != '\0') {
			naddstr(sbctx,  " (");
			naddstr(sbctx,  dname);
			naddstr(sbctx,  ")");
		}
		naddstr(sbctx,  "\n");
		nattroff(sbctx,  COLOR_PAIR(3));
		stream_event_update(sbctx, reblog);
		return;
	}
	
#ifdef USE_SIXEL
	print_picture(sbctx, avatar->string_, SIXEL_MUL_ICO);
	naddstr(sbctx, "\n");
#endif

	// 誰からか[ screen_name(display_name) ]を表示
	nattron(sbctx,  COLOR_PAIR(1)|A_BOLD);
	naddstr(sbctx,  sname);
	nattroff(sbctx,  COLOR_PAIR(1)|A_BOLD);
	
	// dname(表示名)が空の場合は括弧を表示しない
	if (dname[0] != '\0') {
		nattron(sbctx,  COLOR_PAIR(2));
		naddstr(sbctx,  " (");
		naddstr(sbctx,  dname);
		naddstr(sbctx,  ")");
		nattroff(sbctx,  COLOR_PAIR(2));
	}
	
	if(strcmp(vstr, "public")) {
		nattron(sbctx,  COLOR_PAIR(3)|A_BOLD);
		naddstr(sbctx,  " ");
		if(noemojiflag) {
			if(!strcmp(vstr, "unlisted")) {
				naddstr(sbctx,  "<UNLIST>");
			} else if(!strcmp(vstr, "private")) {
				naddstr(sbctx,  "<PRIVATE>");
			} else {
				naddstr(sbctx,  "<!DIRECT!>");
			}
		} else {
			if(!strcmp(vstr, "unlisted")) {
				naddstr(sbctx,  "🔓");
			} else if(!strcmp(vstr, "private")) {
				naddstr(sbctx,  "🔒");
			} else {
				naddstr(sbctx,  "✉");
			}
		}
		nattroff(sbctx,  COLOR_PAIR(3)|A_BOLD);
	}
	
	// 日付表示
	naddstr(sbctx,  " - ");
	nattron(sbctx,  COLOR_PAIR(5));
	naddstr(sbctx,  datebuf);
	nattroff(sbctx,  COLOR_PAIR(5));
	naddstr(sbctx,  "\n");
	
	const char *src = content->string_;
	
	/*naddstr(sbctx,  src);
	naddstr(sbctx,  "\n");*/
	
	// タグ消去処理、2個目以降のの<p>は改行に
	int ltgt = 0;
	int pcount = 0;
	while(*src) {
		// タグならタグフラグを立てる
		if(*src == '<') ltgt = 1;
		
		if(ltgt && strncmp(src, "<br", 3) == 0) naddch(sbctx,  '\n');
		if(ltgt && strncmp(src, "<p", 2) == 0) {
			pcount++;
			if(pcount >= 2) {
				naddstr(sbctx,  "\n\n");
			}
		}
		
		// タグフラグが立っていない(=通常文字)とき
		if(!ltgt) {
			// 文字実体参照の処理
			if(*src == '&') {
				if(strncmp(src, "&amp;", 5) == 0) {
					naddch(sbctx,  '&');
					src += 4;
				}
				else if(strncmp(src, "&lt;", 4) == 0) {
					naddch(sbctx,  '<');
					src += 3;
				}
				else if(strncmp(src, "&gt;", 4) == 0) {
					naddch(sbctx,  '>');
					src += 3;
				}
				else if(strncmp(src, "&quot;", 6) == 0) {
					naddch(sbctx,  '\"');
					src += 5;
				}
				else if(strncmp(src, "&apos;", 6) == 0) {
					naddch(sbctx,  '\'');
					src += 5;
				}
				else if(strncmp(src, "&#39;", 5) == 0) {
					naddch(sbctx,  '\'');
					src += 4;
				}
			} else {
				// 通常文字
				naddch(sbctx,  *((unsigned char *)src));
			}
		}
		if(*src == '>') ltgt = 0;
		src++;
	}
	
	naddstr(sbctx,  "\n");
	
	// 添付メディアのURL表示
	struct sjson_node *media_attachments;
	
	read_json_fom_path(jobj_from_string, "media_attachments", &media_attachments);
	
	if(media_attachments->tag == SJSON_ARRAY) {
		for (int i = 0; i < sjson_child_count(media_attachments); ++i) {
			struct sjson_node *obj = sjson_find_element(media_attachments, i);
			struct sjson_node *url;
			read_json_fom_path(obj, "url", &url);
			if(url->tag == SJSON_STRING) {
				naddstr(sbctx,  noemojiflag ? "<LINK>" : "🔗");
				naddstr(sbctx,  url->string_);
				naddstr(sbctx,  "\n");
#ifdef USE_SIXEL
				struct sjson_node *type;
				read_json_fom_path(obj, "type", &type);
				if(!strcmp(type->string_, "image")) {
					print_picture(sbctx, url->string_, SIXEL_MUL_PIC);
					naddstr(sbctx,  "\n");
				}
#endif
			}
		}
	}

	// 投稿アプリ名表示
	struct sjson_node *application_name;
	int exist_appname = read_json_fom_path(jobj_from_string, "application/name", &application_name);
	
	// 名前が取れたときのみ表示
	if(exist_appname) {
		type = application_name->tag;
		
		if(type != SJSON_NULL) {
			naddstr(sbctx,  " - ");
			
			nattron(sbctx,  COLOR_PAIR(1));
			naddstr(sbctx,  "via ");
			nattroff(sbctx,  COLOR_PAIR(1));
			nattron(sbctx,  COLOR_PAIR(2));
			naddstr(sbctx,  application_name->string_);
			naddstr(sbctx,  "\n");
			nattroff(sbctx,  COLOR_PAIR(2));
		}
	}
	
	naddstr(sbctx,  "\n");
	//wrefresh(scr);
	
	/*wmove(pad, pad_x, pad_y);
	wrefresh(pad);*/
}

// ストリーミングで受信したJSON(接続維持用データを取り除き一体化したもの)
void streaming_received(void)
{
	
	// イベント取得
	if(strncmp(streaming_json, "event", 5) == 0) {
		char *type = strdup(streaming_json + 7);
		if(strncmp(type, "update", 6) == 0) stream_event_handler = stream_event_update;
		else if(strncmp(type, "notification", 12) == 0) stream_event_handler = stream_event_notify;
		else stream_event_handler = NULL;
		
		char *top = type;
		while(*type != '\n') type++;
		type++;
		
		// 後ろにJSONが引っ付いていればJSONバッファへ
		if(*type != 0) {
			free(streaming_json);
			streaming_json = strdup(type);
		}
		free(top);
	}
	
	// JSON受信
	if(strncmp(streaming_json, "data", 4) == 0) {
		if(stream_event_handler) {
			sjson_context* ctx = sjson_create_context(0, 0, NULL);
			struct sjson_node *jobj_from_string = sjson_decode(ctx, streaming_json + 6);

			sbctx_t sb;
			ninitbuf(&sb);

			stream_event_handler(&sb, jobj_from_string);

			nflushcache(&sb);
			squeue_enqueue(sb);

			sjson_destroy_context(ctx);
			stream_event_handler = NULL;
		}
	}
	
	free(streaming_json);
	streaming_json = NULL;
}

// ストリーミング受信スレッド
void *stream_thread_func(void *param)
{
	get_timeline();
	
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;
	char errbuf[CURL_ERROR_SIZE], *uri;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);
	memset(errbuf, 0, sizeof errbuf);
	
	char *uri_stream = malloc(strlen(URI_STREAM) + strlen(selected_stream) + 1);
	
	strcpy(uri_stream, URI_STREAM);
	strcat(uri_stream, selected_stream);

	uri = create_uri_string(uri_stream);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, CURL_USERAGENT);
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 0);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)&streaming_json);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, streaming_callback);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);
	
	streaming_received_handler = streaming_received;
	stream_event_handler = NULL;
	
	ret = curl_easy_perform(hnd);
	if(ret != CURLE_OK) curl_fatal(ret, errbuf);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	free(uri_stream);
	free(uri);
	curl_slist_free_all(slist1);
	slist1 = NULL;
	
	return NULL;
}

void *prompt_thread_func(void *param)
{
	while(1) {
		if(prompt_notify == 0) {
			int c = fgetc(stdin);

			if(c == '\n') {
				prompt_notify = 1;
			}
		} else {
			const struct timespec req = {0, 100 * 1000000};
			nanosleep(&req, NULL);
		}
	}
}

// インスタンスにクライアントを登録する
void do_create_client(char *domain, char *dot_ckcs)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	char errbuf[CURL_ERROR_SIZE];
	
	char json_name[256], *uri;
	
	strcpy(json_name, dot_ckcs);
	
	uri = create_uri_string("api/v1/apps");
	
	// クライアントキーファイルをオープン
	FILE *f = fopen(json_name, "wb");

	post1 = NULL;
	postend = NULL;
	memset(errbuf, 0, sizeof errbuf);
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
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, CURL_USERAGENT);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);	// データの保存先ファイルポインタを指定
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);
	
	ret = curl_easy_perform(hnd);
	if(ret != CURLE_OK) curl_fatal(ret, errbuf);
	
	fclose(f);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	free(uri);
	curl_formfree(post1);
	post1 = NULL;
}

// 承認コードを使ったOAuth処理
void do_oauth(char *code, char *ck, char *cs)
{
	char fields[512];
	sprintf(fields, "client_id=%s&client_secret=%s&grant_type=authorization_code&code=%s&scope=read%%20write%%20follow", ck, cs, code);
	
	// トークンファイルをオープン
	FILE *f = fopen(config.dot_token, "wb");
	
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	char errbuf[CURL_ERROR_SIZE], *uri;

	post1 = NULL;
	postend = NULL;
	memset(errbuf, 0, sizeof errbuf);
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

	uri = create_uri_string("oauth/token");

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, CURL_USERAGENT);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);	// データの保存先ファイルポインタを指定
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);
	
	ret = curl_easy_perform(hnd);
	if(ret != CURLE_OK) curl_fatal(ret, errbuf);
	
	fclose(f);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	free(uri);
	curl_formfree(post1);
	post1 = NULL;
}

// Tootを行う
void do_toot(char *s)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	struct curl_slist *slist1;
	char errbuf[CURL_ERROR_SIZE], *uri;
	
	int is_locked = 0;
	int is_unlisted = 0;
	
	if(*s == '/') {
		if(s[1] != 0) {
			if(s[1] == '/') {
				s++;
			} else if(strncmp(s+1,"private",7) == 0) {
				is_locked = 1;
				s += 1+7;
			} else if(strncmp(s+1,"unlisted",8) == 0) {
				is_unlisted = 1;
				s += 1+8;
			}
		}
	}
	
	FILE *f = fopen("/dev/null", "wb");
	
	uri = create_uri_string("api/v1/statuses");

	post1 = NULL;
	postend = NULL;
	memset(errbuf, 0, sizeof errbuf);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "status",
				CURLFORM_COPYCONTENTS, s,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "visibility",
				CURLFORM_COPYCONTENTS, is_locked ? "private" : (is_unlisted ? "unlisted" : "public"),
				CURLFORM_END);
	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, CURL_USERAGENT);
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);

	ret = curl_easy_perform(hnd);
	if(ret != CURLE_OK) curl_fatal(ret, errbuf);

	fclose(f);
	
	curl_easy_cleanup(hnd);
	hnd = NULL;
	free(uri);
	curl_formfree(post1);
	post1 = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
}

// curlから呼び出されるHTL受信関数
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

// Timelineの受信
void get_timeline(void)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;
	char errbuf[CURL_ERROR_SIZE], *uri;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);
	memset(errbuf, 0, sizeof errbuf);
	
	char *uri_timeline = malloc(strlen(URI_TIMELINE) + strlen(selected_timeline) + 1);
	
	strcpy(uri_timeline, URI_TIMELINE);
	strcat(uri_timeline, selected_timeline);
	
	uri = create_uri_string(uri_timeline);

	char *json = NULL;

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, CURL_USERAGENT);
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)&json);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, htl_callback);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);
	
	ret = curl_easy_perform(hnd);
	if(ret != CURLE_OK) curl_fatal(ret, errbuf);


	sjson_context* ctx = sjson_create_context(0, 0, NULL);
	struct sjson_node *jobj_from_string = sjson_decode(ctx, json);
	sjson_tag type;
	
	type = jobj_from_string->tag;
	
	if(type == SJSON_ARRAY) {
		for (int i = sjson_child_count(jobj_from_string) - 1; i >= 0; i--) {
			struct sjson_node *obj = sjson_find_element(jobj_from_string, i);
			
			sbctx_t sb;
			ninitbuf(&sb);

			stream_event_update(&sb, obj);

			nflushcache(&sb);
			squeue_enqueue(sb);
		}
	}
	
	sjson_destroy_context(ctx);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	free(uri_timeline);
	free(uri);
	curl_slist_free_all(slist1);
	slist1 = NULL;
}

// メイン関数
int main(int argc, char *argv[])
{
	config.profile_name[0] = 0;
	
	// オプション解析
	for(int i=1;i<argc;i++) {
		if(!strcmp(argv[i],"-mono")) {
			monoflag = 1;
			printf("Monochrome mode.\n");
		} else if(!strcmp(argv[i],"-unlock")) {
			hidlckflag = 0;
			printf("Show DIRECT and PRIVATE.\n");
		} else if(!strcmp(argv[i],"-noemoji")) {
			noemojiflag = 1;
			printf("Hide UI emojis.\n");
		} else if(!strncmp(argv[i],"-profile",8)) {
			i++;
			if(i >= argc) {
				fprintf(stderr,"too few argments\n");
				return -1;
			} else {
				strcpy(config.profile_name,argv[i]);
				printf("Using profile: %s\n", config.profile_name);
			}
		} else if(!strncmp(argv[i],"-timeline",9)) {
			i++;
			if(i >= argc) {
				fprintf(stderr,"too few argments\n");
				return -1;
			} else {
				if(!strcmp(argv[i],"home")) {
					
				} else if(!strcmp(argv[i],"local")) {
					selected_stream = "public/local";
					selected_timeline = "public?local=true";
				} else if(!strcmp(argv[i],"public")) {
					selected_stream = "public";
					selected_timeline = "public?local=false";
				} else {
					fprintf(stderr,"Unknown timeline %s\n", argv[i]);
					return -1;
				}
				
				printf("Using timeline: %s\n", selected_stream);
			}
		} else {
			fprintf(stderr,"Unknown Option %s\n", argv[i]);
		}
	}
	
	nano_config_init(&config);
	
	char *env_lang = getenv("LANG");
	int msg_lang = 0;
	
	if(env_lang && !strcmp(env_lang,"ja_JP.UTF-8")) msg_lang = 1;
	
	// トークンファイルオープン
	FILE *fp = fopen(config.dot_token, "rb");
	if(fp) {
		// 存在すれば読み込む
		fclose(fp);
		struct sjson_context *ctx;
		char *json;
		struct sjson_node *token;
		struct sjson_node *jobj_from_file = read_json_from_file(config.dot_token, &json, &ctx);
		read_json_fom_path(jobj_from_file, "access_token", &token);
		sprintf(access_token, "Authorization: Bearer %s", token->string_);
		FILE *f2 = fopen(config.dot_domain, "rb");
		fscanf(f2, "%255s", domain_string);
		fclose(f2);
		sjson_destroy_context(ctx);
		free(json);
	} else {
		// ない場合は登録処理へ
		char domain[256];
		char *ck;
		char *cs;
		printf("%s", nano_msg_list[msg_lang][NANO_MSG_WELCOME]);
		printf("%s", nano_msg_list[msg_lang][NANO_MSG_WEL_FIRST]);
retry1:
		printf("%s", nano_msg_list[msg_lang][NANO_MSG_INPUT_DOMAIN]);
		printf(">");
		scanf("%255s", domain);
		printf("\n");
		
		// ドメイン名を保存する
		FILE *f2 = fopen(config.dot_domain, "wb");
		fprintf(f2, "%s", domain);
		fclose(f2);

		char dot_ckcs[256];
		if (nano_config_app_token_filename(&config, domain, dot_ckcs, sizeof(dot_ckcs)) >= sizeof(dot_ckcs)) {
			fprintf(stderr, "FATAL: Can't allocate memory. Too long filename.\n");
			exit(EXIT_FAILURE);
		}
		
		char json_name[256];
		strcpy(json_name, dot_ckcs);
		strcpy(domain_string, domain);
		
		// クライアントキーファイルをオープン
		FILE *ckcs = fopen(json_name, "rb");
		if(!ckcs) {
			// なければ作る
			do_create_client(domain, dot_ckcs);
		} else {
			// あったら閉じる
			fclose(ckcs);
		}
		
		// クライアントキーファイルを読む
		struct sjson_context *ctx;
		char *json;
		struct sjson_node *cko, *cso;
		struct sjson_node *jobj_from_file = read_json_from_file(json_name, &json, &ctx);
		int r1 = read_json_fom_path(jobj_from_file, "client_id", &cko);
		int r2 = read_json_fom_path(jobj_from_file, "client_secret", &cso);
		if(!r1 || !r2) {
			// もしおかしければ最初まで戻る
			printf("%s", nano_msg_list[msg_lang][NANO_MSG_SOME_WRONG_DOMAIN]);
			remove(json_name);
			remove(config.dot_domain);
			goto retry1;
		}
		ck = strdup(cko->string_);
		cs = strdup(cso->string_);

		sjson_destroy_context(ctx);
		free(json);
		
		char code[256];
		
		printf("%s", nano_msg_list[msg_lang][NANO_MSG_AUTHCATION]);
		printf("%s", nano_msg_list[msg_lang][NANO_MSG_OAUTH_URL]);
		
		// 認証用URLを表示、コードを入力させる
		printf("https://%s/oauth/authorize?client_id=%s&response_type=code&redirect_uri=urn:ietf:wg:oauth:2.0:oob&scope=read%%20write%%20follow\n", domain, ck);
		printf(">");
		scanf("%255s", code);
		printf("\n");

		// 改行読み飛ばし
		getchar();
		
		// 承認コードで認証
		do_oauth(code, ck, cs);
		free(ck);
		free(cs);

		// トークンファイルを読む
		struct sjson_node *token;
		jobj_from_file = read_json_from_file(config.dot_token, &json, &ctx);
		int r3 = read_json_fom_path(jobj_from_file, "access_token", &token);
		if(!r3) {
			// もしおかしければ最初まで戻る
			printf("%s", nano_msg_list[msg_lang][NANO_MSG_SOME_WRONG_OAUTH]);
			remove(json_name);
			remove(config.dot_domain);
			remove(config.dot_token);
			goto retry1;
		}

		// httpヘッダに添付する用の形式でコピーしておく
		sprintf(access_token, "Authorization: Bearer %s", token->string_);
		printf("%s", nano_msg_list[msg_lang][NANO_MSG_FINISH]);

		sjson_destroy_context(ctx);
		free(json);
	}

	setlocale(LC_ALL, "");
	
	pthread_mutex_init(&prompt_mutex, NULL);
	squeue_init();
	sixel_init();

	pthread_t stream_thread;
	pthread_t prompt_thread;
	
	// ストリーミングスレッド生成
	pthread_create(&stream_thread, NULL, stream_thread_func, NULL);
	pthread_create(&prompt_thread, NULL, prompt_thread_func, NULL);
	
	while (1)
	{
		sbctx_t sb;
		// queueに来ていたら表示する
		if(!squeue_dequeue(&sb)) {
			fwrite(sb.buf, sb.bufptr, 1, stdout);
			free(sb.buf);
		}

		// プロンプト通知が来てたらtoot処理
		if(prompt_notify != 0) {
			fputs("> ", stdout);
			char status[1024];
			fgets(status, 1024, stdin);

			char status2[1024];
			char *p1 = status, *p2 = status2;

			for(;*p1 != 0; p1++, p2++) {
				if(*p1 == '\\') {
					if(p1[1] == '\\') {
						*p2 = '\\';
						p1++;
					}
					if(p1[1] == 'n') {
						*p2 = '\n';
						p1++;
					} 
				} else {
					*p2 = *p1;
				}
			}

			*p2 = 0;

			do_toot(status2);
			prompt_notify = 0;
		} else {
			// あまり短いと謎マシンが死ぬので100ms
			const struct timespec req = {0, 100 * 1000000};
			nanosleep(&req, NULL);
		}
	}

	return 0;
}
