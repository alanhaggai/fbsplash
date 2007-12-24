if CONFIG_MISC
noinst_SCRIPTS  = avg.sh
noinst_PROGRAMS = benchmark blittest inputtest splashtest
endif

EXTRA_DIST 		= avg.sh

BUILT_SOURCES = $(top_builddir)/src/fbsplash.h

benchmark_SOURCES  = benchmark.c $(top_builddir)/src/fbsplash.h
benchmark_LDADD    = $(top_builddir)/src/libfbsplashrender.la $(top_builddir)/src/libfbsplash.la
blittest_SOURCES   = blittest.c
blittest_LDADD     =
inputtest_SOURCES  = inputtest.c $(top_builddir)/src/fbsplash.h
inputtest_LDADD    = $(top_builddir)/src/libfbsplashrender.la $(top_builddir)/src/libfbsplash.la
splashtest_SOURCES = splashtest.c $(top_builddir)/src/fbsplash.h
splashtest_LDADD   = $(top_builddir)/src/libfbsplashrender.la $(top_builddir)/src/libfbsplash.la

clean-local:
	@# For some reason automake is not removing this directory
	@# even though it created it by calling libtool for linking.
	@# Therefore, perform normal clean-libtool steps.
	@rm -rf .libs _libs

