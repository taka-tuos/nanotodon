#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "config.h"

#define MAKE_CONFIG_DIR_OK 0x00
#define MAKE_CONFIG_DIR_CANT_CREATE 0x01
#define MAKE_CONFIG_DIR_FILE_EXISTS 0x02

#define APP_TOKEN_DIR "/app_token"

static int make_config_dir(const char* path);
static void make_config_dir_or_die(const char* path);
static int init_xdg(struct nanotodon_config *config, const char* xdg_config_home);

static int make_config_dir(const char* path)
{
	// existence check
	struct stat st;
	if (stat(path, &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			return MAKE_CONFIG_DIR_OK;
		}

		// exists other entry
		return MAKE_CONFIG_DIR_FILE_EXISTS;
	}

	if (errno == ENOENT && mkdir(path, 0755) == 0) {
		return MAKE_CONFIG_DIR_OK;
	}

	return MAKE_CONFIG_DIR_CANT_CREATE;
}

static void make_config_dir_or_die(const char* path)
{
	int result = make_config_dir(path);

	switch (result)
	{
	case MAKE_CONFIG_DIR_OK:
		return;
	case MAKE_CONFIG_DIR_CANT_CREATE:
		fprintf(stderr, "FATAL: Can't create directory '%s' (%s)\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	case MAKE_CONFIG_DIR_FILE_EXISTS:
		fprintf(stderr, "FATAL: File already exists '%s'\n", path);
		exit(EXIT_FAILURE);
	default:
		fprintf(stderr, "FATAL: Undefined error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static int init_xdg(struct nanotodon_config *config, const char* xdg_config_home)
{
	// make xdg config home
	int make_home_result = make_config_dir(xdg_config_home);
	if (make_home_result != MAKE_CONFIG_DIR_OK) {
		return make_home_result;
	}

	if (snprintf(config->root_dir, sizeof(config->root_dir), "%s/nanotodon", xdg_config_home) >= sizeof(config->root_dir)) {
		fprintf(stderr, "FATAL: Can't allocate memory. Too long filename.\n");
		exit(EXIT_FAILURE);
	}
	return make_config_dir(config->root_dir);
}

int nano_config_init(struct nanotodon_config *config)
{
	char *homepath = getenv("HOME");

	// TODO: マクロでXDG Base Directory使うか切り替えてもよさそう
	// XDG_CONFIG_HOME
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home != NULL && init_xdg(config, xdg_config_home) == MAKE_CONFIG_DIR_OK) {
		goto makepath;
	}

	// $HOME/.config
	char default_xdg_config_home[256];
	if (snprintf(default_xdg_config_home, sizeof(default_xdg_config_home), "%s/.config", homepath) >= sizeof(default_xdg_config_home)) {
		goto buffer_err;
	}
	if (init_xdg(config, default_xdg_config_home) == MAKE_CONFIG_DIR_OK) {
		goto makepath;
	}

	// $HOME/.nanotodon
	if (snprintf(config->root_dir, sizeof(config->root_dir), "%s/.nanotodon", homepath) >= sizeof(config->root_dir)) {
		goto buffer_err;
	}
	make_config_dir_or_die(config->root_dir);

makepath:

	if (snprintf(config->dot_token, sizeof(config->dot_token), "%s/token", config->root_dir) >= sizeof(config->dot_token)) {
		goto buffer_err;
	}
	if (snprintf(config->dot_domain, sizeof(config->dot_domain), "%s/domain", config->root_dir) >= sizeof(config->dot_domain)) {
		goto buffer_err;
	}

	return 1;

buffer_err:

	fprintf(stderr, "FATAL: Can't allocate memory. Too long filename.\n");
	exit(EXIT_FAILURE);
}

int nano_config_app_token_filename(struct nanotodon_config *config, const char* domain, char *buf, size_t buf_length)
{
	const size_t path_length = strlen(config->root_dir) + strlen(APP_TOKEN_DIR) + 1 + strlen(domain);
	if (path_length >= buf_length) {
		// 予測パス長がバッファに収まりきらない場合は、何もしないで予測パス長を返す
		return path_length;
	}

	strcpy(buf, config->root_dir);
	strcat(buf, APP_TOKEN_DIR);
	make_config_dir_or_die(buf);

	strcat(buf, "/");
	strcat(buf, domain);
	return path_length;
}
