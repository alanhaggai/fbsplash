#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <rc.h>
#include <errno.h>
#include <sys/mount.h>

#define SPLASH_CACHEDIR		"/lib/splash/cache"
#define SPLASH_TMPDIR		"/lib/splash/tmp"


int (*spf)(rc_hook_t hook, const char *name) = NULL;

int main(int argc, char **argv)
{
	void *h = dlopen("./splash.so", RTLD_LAZY);
	void *f = dlsym(h, "_splash_hook");

	rename(SPLASH_TMPDIR"/profile", SPLASH_CACHEDIR"/profile");

/*f (mount("cachedir", SPLASH_CACHEDIR, "tmpfs", MS_MGC_VAL, "mode=0644,size=4096k")) {
		printf("failed: %s\n", strerror(errno));
	}
*/
	return 0;

	spf = f;
/*	spf(rc_hook_runlevel_stop_in, "reboot");
	spf(rc_hook_service_stop_in, "keymaps");
*/
	spf(rc_hook_runlevel_start_in, "boot");
	spf(rc_hook_service_start_in, "keymaps");
	spf(rc_hook_service_start_out, "keymaps");

	return 0;
}
