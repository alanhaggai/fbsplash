check_PROGRAMS = test_parser

TESTS = test_parser

test_parser_SOURCES  = test_parser.c ../parse.c
test_parser_CPPFLAGS = $(AM_CPPFLAGS) $(libfbsplashrender_la_CFLAGS) -DTARGET_UTIL -I..
test_parser_LDFLAGS  = $(AM_LDFLAGS) ../libfbsplashrender.la ../libfbsplash.la

