/*
 * splashtest.c
 *
 * A simple program to demonstrate the use of the fbsplashrender library.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fbsplash.h>

int main(int argc, char **argv)
{
	struct fbspl_theme *theme;
	int tty = 0;

	fbsplash_lib_init(fbspl_bootup);
	fbsplash_acc_theme_set("test");

	fbsplashr_init(false);
	theme = fbsplashr_theme_load();
	if (!theme) {
		fprintf(stderr, "Failed to load theme.\n");
		return 1;
	}

	tty = fbsplash_set_silent();
	fbsplashr_tty_silent_init(true);
	fbsplashr_tty_silent_update();
	fbsplashr_render_screen(theme, true, false, FBSPL_EFF_FADEIN);
	sleep(2);
	fbsplashr_render_screen(theme, true, false, FBSPL_EFF_FADEOUT);
	fbsplashr_tty_silent_cleanup();
	fbsplash_set_verbose(tty);

	fbsplashr_theme_free(theme);
	fbsplashr_cleanup();
	fbsplash_lib_cleanup();

	return 0;
}

