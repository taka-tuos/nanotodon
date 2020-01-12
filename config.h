#ifndef NANOTODON_CONFIG_H
#define NANOTODON_CONFIG_H

#include <stdlib.h>

struct nanotodon_config {
	char root_dir[256];
	char dot_token[256];
	char dot_domain[256];
};

int nano_config_init(struct nanotodon_config *config);

int nano_config_app_token_filename(struct nanotodon_config *config, const char* domain, char *buf, size_t buf_length);

#endif
