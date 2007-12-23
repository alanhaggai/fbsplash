if CONFIG_TEST
noinst_PROGRAMS 	= test_parser
endif

test_parser_SOURCES  = test_parser.c
test_parser_CPPFLAGS = $(AM_CPPFLAGS) $(libfbsplashrender_la_CFLAGS) -DTARGET_UTIL
test_parser_LDFLAGS  = $(AM_LDFLAGS) ../libfbsplashrender.la ../libfbsplash.la

clean-local:
	@# For some reason automake is not removing this directory
	@# even though it created it by calling libtool for linking.
	@# Therefore, perform normal clean-libtool steps.
	@rm -fr .libs _libs

