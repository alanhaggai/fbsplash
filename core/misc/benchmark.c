/*
 * benchmark.c
 *
 * A simple benchmarking program for the fbsplashrender library.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fbsplash.h>

int main(int argc, char **argv)
{
	int tty = 0;
	int i;
	spl_cfg_t *config;
	struct spl_theme *theme;

	config = splash_lib_init(spl_bootup);
	splash_acc_theme_set("test");

	splashr_init(false);
	theme = splashr_theme_load();
	if (!theme) {
		fprintf(stderr, "Failed to load theme.\n");
		return 1;
	}

	splashr_message_set(theme, "Benchmarking splashutils.. $progress%");

	tty = splash_set_silent();
	splashr_input_init();

	splashr_tty_silent_init();
	splashr_tty_silent_update();
	splashr_render_screen(theme, true, false, SPL_EFF_NONE);

	for (i = 0; i < 65536; i += 64) {
		char a;
		splashr_progress_set(theme, i);
		splashr_render_screen(theme, false, false, SPL_EFF_NONE);
		a = splashr_input_getkey(false);
		if (a == '\x1b')
			break;
	}

	splashr_tty_silent_cleanup();
	splashr_input_cleanup();
	splash_set_verbose(tty);

	splashr_theme_free(theme);
	splashr_cleanup();
	splash_lib_cleanup();

	return 0;
}

