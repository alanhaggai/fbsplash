#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "../common.h"
#include "../render.h"
#include "parse.h"

#undef iprint
#define iprint(lev, args...) ;

char *icons_ok[] = {
	"icon dummy.png 0 0",
	"icon dummy.png 100 23 svc_start dummysvc",
	"icon dummy.png 340 40 crop <0,0,0,0> <30,30,10,10>",
	"icon dummy.png 45 65 crop <1,1,1,1> <23,4,5,6> svc_stop dummysvc ",
};

char *icons_err[] = {
	"icondummy.png 0 0",
	"icon dummy.png",
	"icon",
	"icon dummy.png svc_start dummysvc",
	"icon dummy.png 0 svc_start dummysvc",
	"icon dummy.png 10 100 crop <0,0,0,0>",
	"icon dummy.png 34 34 svc_stop",
	"icon 3 5",
};

char *anims_ok[] = {
	"anim once dummy.mng 0 0",
	"anim loop dummy.mng 30 40 svc_started dummysvc",
	"anim proportional dummy.mng 30 40 svc_start_failed dummysvc",
};

char *anims_err[] = {
	"anim dummy.png 0 0",
	"anim once dummy.png svc_start dummysvc",
	"anim loop 0 0",
	"anim loop dummy.png 0",
	"anim loop dummy.png 0 3 svc_start   ",
};

char *text_ok[] = {
	"text dummy.ttf 12 0 0 #000000 \"foobar\"",
	"text silent verbose dummy.ttf 12 0 0 #fe09ae \"dummy\"",
	"text silent dummy.ttf biu 12 5 left 5 top #deadbeef exec eval \"$progress test\"",
	"text verbose dummy.ttf ubi 3 100 middle 347 middle #ffffff exec \"dummy\"",
	"text verbose dummy.ttf b 1 100 right 100 bottom #123456 eval \"just a test\"",
};

char *text_err[] = {
	"text 12 0 0",
	"text dummy.ttf biu 3 4",
	"text dummy.ttf 3 top 4 middle",
	"text dummy.ttf 4 5 \"test\"",
	"text dummy.ttf 4 4 5 \"test\"",
	"text dummy.ttf 12 0 0 #fff00 \"test\"",
	"text dummy.ttf 12 0 0 #ff00ff00f \"test\"",
	"text dummy.ttf 12 0 0 #ffffff test",
};

char *box_ok[] = {
	"box silent 0 0 1 1 #001122",
	"box 0 0 0 0 #12345678",
	"box silent 0 1 2 3 #000000 #111111 #222222 #333333",
};

char *box_err[] = {
	"box silent 1 1 2 2",
	"box 1 2 3 4 #0012gf",
	"box 1 3 #00f0f0",
	"box 1 2 3 #f0f0f0",
	"box 5 5 1 1 #f0f0f0",
	"box 1 1 4 4 #f0f0f0 #f0f0f0",
	"box 2 5 6 7 #f0f0f0 #f0f0f0 #deadbeef",
	"box 7 7 8 8 #708090 #909090 #deadbeef #beefdeade",
	"box silent       0    349  1279 450  #22222c #22222c #383849 #040454498",
};

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))

int tests_failed = 0;
int tests_run = 0;

bool parse_box_wrapper(char *t)
{
	void *p = parse_box(t);
	return (p == NULL) ? false : true;
}

void test_parse(bool (*parsef)(char*), bool expect, char *stuff, int offset)
{
	char *p = strdup(stuff);
	bool out = parsef(p + offset);

	tests_run++;

	if (out != expect) {
		printf("* Failed: %s\n", stuff);
		tests_failed++;
	} else {
		printf("* OK: %s\n", stuff);
	}

	free(p);
	return;
}

int main(int argc, char **argv)
{
	int i;
	char *p;
	fbspl_cfg_t *config;

	config = fbsplash_lib_init(fbspl_bootup);
	fbsplash_acc_theme_set("test");
	fbsplashr_init(false);

	config->verbosity = FBSPL_VERB_QUIET;

	tmptheme.xres = 1000;
	tmptheme.yres = 1000;

	for (i = 0; i < ARRAY_SIZE(icons_ok); i++) {
		test_parse(parse_icon, true, icons_ok[i], 4);
	}

	for (i = 0; i < ARRAY_SIZE(icons_err); i++) {
		test_parse(parse_icon, false, icons_err[i], 4);
	}

#if WANT_MNG
	for (i = 0; i < ARRAY_SIZE(anims_ok); i++) {
		test_parse(parse_anim, true, anims_ok[i], 4);
	}

	for (i = 0; i < ARRAY_SIZE(anims_err); i++) {
		test_parse(parse_anim, false, anims_err[i], 4);
	}
#endif

#if WANT_TTF
	for (i = 0; i < ARRAY_SIZE(text_ok); i++) {
		test_parse(parse_text, true, text_ok[i], 4);
	}

	for (i = 0; i < ARRAY_SIZE(text_err); i++) {
		test_parse(parse_text, false, text_err[i], 4);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(box_ok); i++) {
		test_parse(parse_box_wrapper, true, box_ok[i], 3);
	}

	for (i = 0; i < ARRAY_SIZE(box_err); i++) {
		test_parse(parse_box_wrapper, false, box_err[i], 3);
	}

	printf("Ran %d tests, %d failed.\n", tests_run, tests_failed);

	fbsplashr_cleanup();
	fbsplash_lib_cleanup();

	return tests_failed;
}
