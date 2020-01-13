#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "config.h"

static int mkdir_or_die(const char* path);
static int init_xdg(struct nanotodon_config *config, const char* xdg_config_home);

static int mkdir_or_die(const char* path)
{
	// existence check
	struct stat st;
	if (stat(path, &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			return 1;
		}

		// exists other entry
		fprintf(stderr, "FATAL: File already exists '%s'\n", path);
		exit(EXIT_FAILURE);
	}

	if (errno == ENOENT && mkdir(path, 0755) == 0) {
		return 1;
	}

	fprintf(stderr, "FATAL: Can't create directory '%s' (errno=%d)\n", path, errno);
	exit(EXIT_FAILURE);
}

static int init_xdg(struct nanotodon_config *config, const char* xdg_config_home)
{
	// make xdg config home
	mkdir_or_die(xdg_config_home);
	sprintf(config->root_dir, "%s/nanotodon", xdg_config_home);

	return 1;
}

int nano_config_init(struct nanotodon_config *config)
{
	char *homepath = getenv("HOME");

	// TODO: マクロでXDG Base Directory使うか切り替えてもよさそう
	// XDG_CONFIG_HOME
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home != NULL) {
		if (init_xdg(config, xdg_config_home)) {
			goto makepath;
		} else {
			return 0;
		}
	}

	// $HOME/.config
	char default_xdg_config_home[256];
	sprintf(default_xdg_config_home, "%s/.config", homepath);
	if (init_xdg(config, default_xdg_config_home)) {
		goto makepath;
	} else {
		return 0;
	}

	// $HOME/.nanotodon
	sprintf(config->root_dir, "%s/.nanotodon", homepath);

makepath:

	mkdir_or_die(config->root_dir);
	sprintf(config->dot_token, "%s/token", config->root_dir);
	sprintf(config->dot_domain, "%s/domain", config->root_dir);

	return 1;
}

int nano_config_app_token_filename(struct nanotodon_config *config, const char* domain, char *buf, size_t buf_length)
{
	char app_token_dir[256];
	snprintf(app_token_dir, sizeof(app_token_dir), "%s/app_token", config->root_dir);
	mkdir_or_die(app_token_dir);

	return snprintf(buf, buf_length, "%s/%s", app_token_dir, domain);
}
