/*
 * splashtest.c
 *
 * A simple program to demonstrate the use of the splashrender library.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <splash.h>

int main(int argc, char **argv)
{
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
	splash_render_screen(theme, true, false, 's', EFF_FADEIN);
	sleep(2);
	splash_render_screen(theme, true, false, 's', EFF_FADEOUT);
	splash_tty_silent_cleanup();
	splash_set_verbose();

	splash_theme_free(theme);
	splash_render_cleanup();
	splash_lib_cleanup();

	return 0;
}

