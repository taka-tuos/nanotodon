#ifndef __UTILS_H__
#define __UTILS_H__

#include <curl/curl.h>
#include <stdint.h>
#include "sjson.h"

int ustrwidth(const char *str);

void curl_fatal(CURLcode ret, const char *errbuf);

char *create_uri_string(char *api);

sjson_node *read_json_from_file(char *path, char **json_p, sjson_context **ctx_p);
int read_json_fom_path(struct sjson_node *obj, char *path, struct sjson_node **dst);


struct rawBuffer {
    unsigned char *data;
    int data_size;
};

size_t buffer_writer(char *ptr, size_t size, size_t nmemb, void *stream);

#endif
