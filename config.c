#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "config.h"

static int exists_dir(const char* path)
{
	struct stat st;
	int ret = stat(path, &st);
	if (ret == 0 && (st.st_mode & S_IFMT) == S_IFDIR) {
		return 1;
	} else {
		return 0;
	}
}

static int init_xdg(struct nanotodon_config *config, const char* xdg_config_home)
{
	// make xdg config home
	if (!exists_dir(xdg_config_home) && mkdir(xdg_config_home, 0755)) {
		return 0;
	}

	sprintf(config->root_dir, "%s/nanotodon", xdg_config_home);

	return 1;
}

int nano_init_config(struct nanotodon_config *config)
{
	char *homepath = getenv("HOME");

	// TODO: マクロでXDG Base Directory使うか切り替えてもよさそう
	// XDG_CONFIG_HOME
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home != NULL && init_xdg(config, xdg_config_home)) {
		goto makepath;
	}

	// $HOME/.config
	char default_xdg_config_home[256];
	sprintf(default_xdg_config_home, "%s/.config", homepath);
	if (init_xdg(config, default_xdg_config_home)) {
		goto makepath;
	}

	// $HOME/.nanotodon
	sprintf(config->root_dir, "%s/.nanotodon", homepath);

makepath:

	if (!exists_dir(config->root_dir) && mkdir(config->root_dir, 0755)) {
		return 0;
	}
	sprintf(config->dot_token, "%s/token", config->root_dir);
	sprintf(config->dot_domain, "%s/domain", config->root_dir);

	return 1;
}

int nano_app_token_filename(struct nanotodon_config *config, const char* domain, char *buf, size_t buf_length)
{
	char app_token_dir[256];
	snprintf(app_token_dir, sizeof(app_token_dir), "%s/app_token", config->root_dir);
	if (!exists_dir(app_token_dir) && mkdir(app_token_dir, 0755)) {
		return 0;
	}

	return snprintf(buf, buf_length, "%s/%s", app_token_dir, domain);
}
