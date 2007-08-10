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
	struct fbspl_theme *theme;

	fbsplash_lib_init(fbspl_bootup);
	fbsplash_acc_theme_set("test");

	fbsplashr_init(false);
	theme = fbsplashr_theme_load();
	if (!theme) {
		fprintf(stderr, "Failed to load theme.\n");
		return 1;
	}

	fbsplashr_message_set(theme, "Benchmarking fbsplash.. $progress%");

	tty = fbsplash_set_silent();
	fbsplashr_input_init();

	fbsplashr_tty_silent_init();
	fbsplashr_tty_silent_update();
	fbsplashr_render_screen(theme, true, false, FBSPL_EFF_NONE);

	for (i = 0; i < 65536; i += 64) {
		char a;
		fbsplashr_progress_set(theme, i);
		fbsplashr_render_screen(theme, false, false, FBSPL_EFF_NONE);
		a = fbsplashr_input_getkey(false);
		if (a == '\x1b')
			break;
	}

	fbsplashr_tty_silent_cleanup();
	fbsplashr_input_cleanup();
	fbsplash_set_verbose(tty);

	fbsplashr_theme_free(theme);
	fbsplashr_cleanup();
	fbsplash_lib_cleanup();

	return 0;
}

