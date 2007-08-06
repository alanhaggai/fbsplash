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
	spl_cfg_t *config;
	struct spl_theme *theme;

	config = splash_lib_init(spl_bootup);
	splash_theme_set("test");

	splashr_init(false);
	theme = splashr_theme_load();
	if (!theme) {
		fprintf(stderr, "Failed to load theme.\n");
		return 1;
	}

	splash_set_silent();
	splashr_tty_silent_init();
	splashr_tty_silent_update();
	splashr_render_screen(theme, true, false, 's', SPL_EFF_FADEIN);
	sleep(2);
	splashr_render_screen(theme, true, false, 's', SPL_EFF_FADEOUT);
	splashr_tty_silent_cleanup();
	splash_set_verbose();

	splashr_theme_free(theme);
	splashr_cleanup();
	splash_lib_cleanup();

	return 0;
}

