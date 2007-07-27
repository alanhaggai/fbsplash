/*
 * benchmark.c
 *
 * A simple benchmarking program for the splashrender library.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <splash.h>

int main(int argc, char **argv)
{
	int i;
	scfg_t *config;
	struct stheme *theme;

	config = splash_lib_init(bootup);

	if (config->theme)
		free(config->theme);
	config->theme = strdup("livecd-2007.0");

	splash_render_init(false);
	theme = splash_theme_load();

	splash_set_silent();
	splash_tty_silent_init();
	splash_render_screen(theme, true, false, 's', EFF_NONE);

	for (i = 0; i < 65536; i += 16) {
		config->progress = i;
		splash_render_screen(theme, false, false, 's', EFF_NONE);
	}

	splash_tty_silent_cleanup();
	splash_set_verbose();

	splash_theme_free(theme);
	splash_render_cleanup();
	splash_lib_cleanup();

	return 0;
}

