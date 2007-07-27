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
	splash_theme_set("livecd-2007.0");

	splashr_init(false);
	theme = splashr_theme_load();

	splashr_message_set(theme, "Bechmarking splashutils.. $progress%");

	splash_set_silent();
	splashr_tty_silent_init();
	splashr_render_screen(theme, true, false, 's', EFF_NONE);

	for (i = 0; i < 65536; i += 16) {
		splashr_progress_set(theme, i);
		splashr_render_screen(theme, false, false, 's', EFF_NONE);
	}

	splashr_tty_silent_cleanup();
	splash_set_verbose();

	splashr_theme_free(theme);
	splashr_cleanup();
	splash_lib_cleanup();

	return 0;
}

