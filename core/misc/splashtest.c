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
	splash_theme_set("test");

	splashr_init(false);
	theme = splashr_theme_load();

	splash_set_silent();
	splashr_tty_silent_init();
	splashr_tty_silent_update();
	splashr_render_screen(theme, true, false, 's', EFF_FADEIN);
	sleep(2);
	splashr_render_screen(theme, true, false, 's', EFF_FADEOUT);
	splashr_tty_silent_cleanup();
	splash_set_verbose();

	splashr_theme_free(theme);
	splashr_cleanup();
	splash_lib_cleanup();

	return 0;
}

