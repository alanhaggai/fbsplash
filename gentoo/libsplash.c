#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/kd.h>

#include "splash.h"

int splash_config_init(scfg_t *cfg)
{
	cfg->tty_s = TTY_SILENT;
	cfg->tty_v = TTY_VERBOSE;
	cfg->theme = strdup(DEFAULT_THEME);
	cfg->kdmode = KD_TEXT;
	cfg->profile = 0;
	cfg->reqmode = 'o';

	return 0;
}

/* Parse the kernel command line to get splash settings. */
int splash_parse_kcmdline(scfg_t *cfg)
{
	FILE *fp;
	char *p, *t, *opt;
	char *buf = malloc(1024);
	int err = 0;

	if (!buf)
		return -1;

	fp = fopen("/proc/cmdline", "r");
	if (!fp)
		goto fail;

	fgets(buf, 1024, fp);
	fclose(fp);

	/* FIXME: add support for multiple splash= settings */
	t = strstr(buf, "splash=");
	if (!t)
		goto fail;

	t += 7; p = t;
	for (p = t; *p != ' ' && *p != 0; p++);
	*p = 0;

	while ((opt = strsep(&t, ",")) != NULL) {

		if (!strncmp(opt, "tty:", 4)) {
			cfg->tty_v = strtol(opt+4, NULL, 0);
//		} else if (!strncmp(opt, "fadein", 6)) {
//			arg_effects |= EFF_FADEIN;
		} else if (!strncmp(opt, "verbose", 7)) {
			cfg->reqmode = 'v';
		} else if (!strncmp(opt, "silent", 6)) {
			cfg->reqmode = 's';
		} else if (!strncmp(opt, "off", 3)) {
			cfg->reqmode = 'o';
		} else if (!strncmp(opt, "theme:", 6)) {
			if (cfg->theme)
				free(cfg->theme);
			cfg->theme = strdup(opt+6);
		} else if (!strncmp(opt, "kdgraphics", 10)) {
			cfg->kdmode = KD_GRAPHICS;
		} else if (!strncmp(opt, "profile", 7)) {
			cfg->profile = 1;
		}
	}

out:
	free(buf);
	return err;

fail:
	err = -1;
	goto out;
}

