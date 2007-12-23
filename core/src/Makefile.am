################################################################################
# automake file for generating the user splashutils' Makefile file.
#
# Copyright (c) 2004-2007, Michal Januszewski <spock@gentoo.org>
# Copyright (c) 2007, Paul Bender <pebender@gmail.com>
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
################################################################################

SUBDIRS = . test

################################################################################
# General
################################################################################

BUILT_SOURCES          =

noinst_HEADERS         =
nodist_include_HEADERS =

noinst_LTLIBRARIES     =
elib_LTLIBRARIES       =
lib_LTLIBRARIES        =

noinst_PROGRAMS        =
esbin_PROGRAMS         =
ebin_PROGRAMS          =
sbin_PROGRAMS          =
bin_PROGRAMS           =

EXTRA_DIST             =
MOSTLYCLEANFILES       =

QUIET ?= true

if CONFIG_DEBUG
CFLAGS ?= -Wall -g
else
CFLAGS ?= -O2
endif

################################################################################
# All
################################################################################

INCLUDES = -I$(top_srcdir) -I$(top_builddir)

AM_MAKEFLAGS     =
AM_LIBTOOLFLAGS  =

# HACK: automake doesn't support ifeq, but will ignore it if it doesn't start
# at column 0
 ifeq ($(strip $(QUIET)),true)
	Q = @
	OUTPUT = > /dev/null
	infmsg = printf "  %-7s %s\n" $(1) $(2)
	AM_MAKEFLAGS    += --silent
	AM_LIBTOOLFLAGS += --silent
 else
	OUTPUT =
	Q =
	infmsg = :
 endif

BUILT_SOURCES   += fbsplash.h

mostlyclean-local: \
		mostlyclean-local-kernel

clean-local: \
		clean-local-test

dist-hook: \
		dist-hook-header

EXTRA_DIST    += \
	Doxyfile

doc:
	doxygen

################################################################################
# Header
################################################################################

nodist_include_HEADERS  += fbsplash.h

EXTRA_DIST              += fbsplash.h.in klibc_compat.h

MOSTLYCLEANFILES        += fbsplash.h

dist-hook-header:
	@# Removing this file from the distribution is easier than putting it
	@# in a nodist_*_SOURCES rather than the normal _SOURCES list for all
	@# the programs and libraries that depend on it.
	@rm -r $(top_distdir)/fbsplash.h

fbsplash.h: fbsplash.h.in $(top_builddir)/config.h
	@$(call infmsg,CREATE,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)rm -f $@
	$(Q)cat $< | $(SED) \
		-e 's|@bindir[@]|$(ebindir)|g' \
		-e 's|@sbindir[@]|$(esbindir)|g' \
		-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
		-e 's|@themedir[@]|$(themedir)|g' \
		-e 's|@libdir[@]|$(elibdir)|g' \
		> $@

$(top_builddir)/config.h:
	@echo "The file 'config.h' is missing. Please run 'configure'."
	@exit 1

################################################################################
# Library
################################################################################

lib_LTLIBRARIES += libfbsplash.la
lib_LTLIBRARIES += libfbsplashrender.la

libfbsplash_la_SOURCES = libfbsplash.c common.h fbsplash.h
libfbsplash_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(libfbsplash_version)
libfbsplash_la_LIBADD  =

libfbsplashrender_la_SOURCES  = \
	libfbsplashrender.c \
	fbcon_decor.c \
	common.c \
	parse.c \
	list.c \
	image.c \
	render.c \
	effects.c \
	fbcon_decor.h \
	include/console_decor.h \
	include/fbcondecor.h \
	common.h \
	render.h \
	fbsplash.h
libfbsplashrender_la_CFLAGS   = $(AM_CFLAGS)
libfbsplashrender_la_LDFLAGS  = $(AM_LDFLAGS) -version-info $(libfbsplashrender_version)
libfbsplashrender_la_LIBADD   = -lfbsplash

libfbsplashrender_la_CFLAGS  += $(JPEG_CFLAGS)
libfbsplashrender_la_LIBADD  += $(JPEG_LIBS)

if CONFIG_MNG
libfbsplashrender_la_SOURCES += mng_callbacks.c mng_render.c mng_splash.h common.h render.h fbsplash.h
libfbsplashrender_la_CFLAGS  += $(MNG_CFLAGS)
libfbsplashrender_la_LIBADD  += $(MNG_LIBS)
endif
if CONFIG_PNG
libfbsplashrender_la_CFLAGS  += $(PNG_CFLAGS)
libfbsplashrender_la_LIBADD  += $(PNG_LIBS)
endif
if CONFIG_TTF
libfbsplashrender_la_SOURCES += ttf.c ttf.h fbsplash.h
libfbsplashrender_la_CFLAGS  += $(FREETYPE2_CFLAGS) $(M_CFLAGS)
libfbsplashrender_la_LIBADD  += $(FREETYPE2_LIBS)   $(M_LIBS)
endif

AM_MAKEFLAGS += "libfbsplashrender_la_CFLAGS=$(libfbsplashrender_la_CFLAGS)"


################################################################################
# User
################################################################################

if CONFIG_FBCON_DECOR
esbin_PROGRAMS += fbcondecor_ctl
endif
esbin_PROGRAMS += fbsplashd
esbin_PROGRAMS += fbsplashd.static
bin_PROGRAMS   += splash_util
ebin_PROGRAMS  += splash_util.static

fbcondecor_ctl_SOURCES      = fbcon_decor_ctl.c fbcon_decor.h include/console_decor.h include/fbcondecor.h common.h fbsplash.h
fbcondecor_ctl_CPPFLAGS     = $(AM_CPPFLAGS) -DTARGET_UTIL $(libfbsplashrender_la_CFLAGS)
fbcondecor_ctl_LDFLAGS      = $(AM_LDFLAGS)
fbcondecor_ctl_LDADD        = libfbsplashrender.la libfbsplash.la

fbsplashd_SOURCES           = daemon_cmd.c daemon.c daemon.h common.h render.h fbsplash.h
fbsplashd_CPPFLAGS          = $(AM_CPPFLAGS) -DTARGET_UTIL
fbsplashd_CFLAGS            = $(AM_CFLAGS) $(PTHREAD_CFLAGS) $(RT_CFLAGS) $(libfbsplashrender_la_CFLAGS)
fbsplashd_LDFLAGS           = $(AM_LDFLAGS)
fbsplashd_LDADD             = libfbsplashrender.la libfbsplash.la $(PTHREAD_LIBS) $(RT_LIBS) $(GPM_LIBS)

fbsplashd_static_SOURCES    = daemon_cmd.c daemon.c daemon.h common.h render.h fbsplash.h
fbsplashd_static_CPPFLAGS   = $(AM_CPPFLAGS) -DTARGET_UTIL
fbsplashd_static_CFLAGS     = $(AM_CFLAGS) $(PTHREAD_STATIC_CFLAGS) $(RT_CFLAGS) $(libfbsplashrender_la_CFLAGS)
fbsplashd_static_LDFLAGS    = $(AM_LDFLAGS) -all-static
fbsplashd_static_LDADD      = libfbsplashrender.la libfbsplash.la $(PTHREAD_STATIC_LIBS) $(RT_LIBS) $(GPM_LIBS)

splash_util_SOURCES         = util.c common.h fbsplash.h
splash_util_CPPFLAGS        = $(AM_CPPFLAGS) -DTARGET_UTIL $(libfbsplashrender_la_CFLAGS)
splash_util_LDFLAGS         = $(AM_LDFLAGS)
splash_util_LDADD           = libfbsplashrender.la libfbsplash.la

splash_util_static_SOURCES  = util.c common.h fbsplash.h
splash_util_static_CPPFLAGS = $(AM_CPPFLAGS) -DTARGET_UTIL $(libfbsplashrender_la_CFLAGS)
splash_util_static_LDFLAGS  = $(AM_LDFLAGS) -all-static
splash_util_static_LDADD    = libfbsplashrender.la libfbsplash.la


################################################################################
# Kernel
################################################################################

if CONFIG_HELPER
esbin_PROGRAMS  += fbcondecor_helper
endif

fbcondecor_helper_SOURCES = \
	kernel.c \
	libfbsplash.c \
	libfbsplashrender.c \
	fbcon_decor.c \
	common.c \
	parse.c \
	list.c \
	render.c \
	image.c \
	effects.c \
	fbcon_decor.h \
	include/console_decor.h \
	include/fbcondecor.h \
	common.h \
	render.h \
	fbsplash.h
fbcondecor_helper_CPPFLAGS  = -DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER
fbcondecor_helper_CFLAGS    = -Os -w -ffunction-sections -fdata-sections -I.@am__isrc@
fbcondecor_helper_LDFLAGS   = 
fbcondecor_helper_LDADD     = 
fbcondecor_helper_CFLAGS   += -I$(abs_top_builddir)/libjpeg -I$(LIBJPEG_SOURCE)
fbcondecor_helper_LDADD    +=  libjpeg.a
if CONFIG_PNG
fbcondecor_helper_CFLAGS   += -I$(LIBPNG_SOURCE)
fbcondecor_helper_LDADD    += libpng.a libz.a
endif
if CONFIG_TTF_KERNEL
fbcondecor_helper_SOURCES  += ttf.c ttf.h fbsplash.h
fbcondecor_helper_CFLAGS   += -I$(LIBFREETYPE2_SOURCE)/include -I$(LIBZ_SOURCE)
fbcondecor_helper_LDADD    += libfreetype.a
endif
if CONFIG_KLIBC_SHARED
fbcondecor_helper_LDFLAGS  += -shared
else
fbcondecor_helper_LDFLAGS  += -static
endif

EXTRA_DIST += \
	$(LIBFREETYPE2_SOURCE_INTERNAL) \
	$(LIBJPEG_SOURCE_INTERNAL) \
	$(LIBPNG_SOURCE_INTERNAL) \
	$(LIBZ_SOURCE_INTERNAL)

mostlyclean-local-kernel: \
		mostlyclean-local-kernel-libfreetype \
		mostlyclean-local-kernel-libjpeg \
		mostlyclean-local-kernel-libpng \
		mostlyclean-local-kernel-libz

# Don't pass command line variables when calling make.
# This saves us trouble when building against klibc.
MAKEOVERRIDES=

LIBFREETYPE2_SOURCE_INTERNAL = libs/freetype-2.3.5
LIBJPEG_SOURCE_INTERNAL      = libs/jpeg-6b
LIBPNG_SOURCE_INTERNAL       = libs/libpng-1.2.18
LIBZ_SOURCE_INTERNAL         = libs/zlib-1.2.3

# Flags for the kernel utilities
K_CFLAGS = -Os -w -ffunction-sections -fdata-sections \
			-DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER

PNGDEFS = -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt \
		-DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO \
		-DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED \
		-DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS \
		-DPNG_NO_MMX_CODE

# Override automake's generation of target rules for building fbcondecor_helper.
fbcondecor_helper: $(fbcondecor_helper_LDADD) $(fbcondecor_helper_OBJECTS)
	@$(call infmsg,LD,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)$(KLCC) $(fbcondecor_helper_CFLAGS) $(fbcondecor_helper_LDFLAGS) $+ $(fbcondecor_helper_LDADD) -o $@
	$(Q)$(STRIP) $@

# Override automake's generation of target rules for building fbcondecor_helper object files.
fbcondecor_helper-kernel.o:
fbcondecor_helper-libfbsplash.o:
fbcondecor_helper-libfbsplashrender.o:
fbcondecor_helper-fbcon_decor.o:
fbcondecor_helper-common.o:
fbcondecor_helper-parse.o:
fbcondecor_helper-list.o:
fbcondecor_helper-render.o:
fbcondecor_helper-image.o:
fbcondecor_helper-effects.o:
fbcondecor_helper-ttf.o:
fbcondecor_helper-%.o: %.c
	@$(call infmsg,CC,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)$(KLCC) $(fbcondecor_helper_CPPFLAGS) $(fbcondecor_helper_CFLAGS) $(INCLUDES) -c $< -o $@

libfreetype.a: klibc_compat.h
	$(Q)$(MKDIR_P) $(abs_top_builddir)/libfreetype
	$(Q)cd $(abs_top_builddir)/libfreetype ; \
	if test ! -e ./config.mk ; then \
		$(call infmsg,CONF,libfreetype.a) ; \
		CFLAGS="$(K_CFLAGS) -include $(abs_top_srcdir)/src/klibc_compat.h" \
		CC="$(KLCC)" CPPFLAGS="-I$(LIBZ_SOURCE)" \
		$(LIBFREETYPE2_SOURCE)/configure --host=$(host) --build=$(build_triplet) --disable-shared $(OUTPUT) ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CFLAGS="$(K_CFLAGS)" library ; then \
		$(call infmsg,MAKE,libfreetype.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CFLAGS="-c $(K_CFLAGS)" CCexe="$(BUILD_CC)" library $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f $(abs_top_builddir)/libfreetype/.libs/libfreetype.a $@

libjpeg.a:
	$(Q)$(MKDIR_P) $(abs_top_builddir)/libjpeg
	$(Q)cd $(abs_top_builddir)/libjpeg ; \
	if test ! -e ./Makefile ; then \
		$(call infmsg,CONF,libjpeg.a) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS) -I$(abs_top_builddir)/libjpeg" $(LIBJPEG_SOURCE)/configure \
		--host=$(host) --build=$(build_triplet) $(OUTPUT) ; \
		echo "#define NO_GETENV 1" >> jconfig.h ; \
		$(SED) -i "s#CFLAGS=.*#CFLAGS=$(K_CFLAGS) -I$(abs_top_builddir)/libjpeg#" Makefile ; \
		$(SED) -i "s:#define JMESSAGE.*::" $(LIBJPEG_SOURCE)/jerror.c ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CC="$(KLCC)" libjpeg.a ; then  \
		$(call infmsg,MAKE,libjpeg.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CC="$(KLCC)" libjpeg.a $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f $(abs_top_builddir)/libjpeg/libjpeg.a $@

libpng.a: libz.a
	$(Q)cd $(LIBPNG_SOURCE) ; \
	if test ! -e ./Makefile ; then \
		$(call infmsg,CONF,libpng.a) ; \
		cp scripts/makefile.linux Makefile $(OUTPUT); \
		$(SED) -i -e '/^CFLAGS/ { N ; s#^CFLAGS=.*#CFLAGS=$(K_CFLAGS) $(PNGDEFS)# ; P ; D }' \
		       -e 's#^ZLIBINC=.*#ZLIBINC=$(LIBZ_SOURCE)#' \
		       -e 's#^ZLIBLIB=.*#ZLIBLIB=$(LIBZ_SOURCE)#' Makefile ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CC="$(KLCC)" AR_RC="$(AR) rc" RANLIB="$(RANLIB)" libpng.a ; then \
		$(call infmsg,MAKE,libpng.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CC="$(KLCC)" AR_RC="$(AR) rc" RANLIB="$(RANLIB)" libpng.a $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f $(LIBPNG_SOURCE)/libpng.a $@

libz.a:
	$(Q)cd $(LIBZ_SOURCE) ; \
	if test ! -e ./Makefile ; then \
		$(call infmsg,CONF,libz.a) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS)" \
		./configure $(OUTPUT); \
		$(SED) -i 's#^CFLAGS=\(.*\)#CFLAGS=\1 $(K_CFLAGS)#' Makefile ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CC="$(KLCC)" AR="$(AR) rc" RANLIB="$(RANLIB)" libz.a ; then \
		$(call infmsg,MAKE,zlib.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CC="$(KLCC)" AR="$(AR) rc" RANLIB="$(RANLIB)" libz.a $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f  $(LIBZ_SOURCE)/libz.a $@

mostlyclean-local-kernel-libfreetype:
	$(Q)$(MKDIR_P) libfreetype
	$(Q)cd libfreetype ; \
	if test -e ./config.mk ; then \
		$(call infmsg,CLEAN,libfreetype.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f config.mk ; \
	fi
	$(Q)rm -fr libfreetype
	$(Q)rm -f  libfreetype.a

mostlyclean-local-kernel-libjpeg:
	$(Q)$(MKDIR_P) libjpeg
	$(Q)cd libjpeg ; \
	if test -e Makefile ; then \
		$(call infmsg,CLEAN,libjpeg.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f Makefile ; \
	fi
	$(Q)rm -fr libjpeg
	$(Q)rm -f  libjpeg.a

mostlyclean-local-kernel-libpng:
	$(Q)cd $(LIBPNG_SOURCE) ; \
	if test -e Makefile ; then \
		$(call infmsg,CLEAN,libpng.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f Makefile ; \
	fi
	$(Q)rm -f  libpng.a

mostlyclean-local-kernel-libz:
	$(Q)cd $(LIBZ_SOURCE) ; \
	if test -e Makefile ; then \
		$(call infmsg,CLEAN,libz.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f Makefile ; \
	fi
	$(Q)rm -f  libz.a

