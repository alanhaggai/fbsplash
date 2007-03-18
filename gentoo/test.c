#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <rc.h>

int (*spf)(rc_hook_t hook, const char *name) = NULL;

int main(int argc, char **argv)
{
	void *h = dlopen("./splash.so", RTLD_LAZY);
	void *f = dlsym(h, "_splash_hook");

	spf = f;
	spf(rc_hook_runlevel_start_out, "sysinit");
/*	spf(rc_hook_runlevel_start_in, "boot");
	spf(rc_hook_service_start_in, "keymaps");
	spf(rc_hook_service_start_out, "keymaps");
*/
	return 0;
}
