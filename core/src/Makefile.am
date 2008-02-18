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

include ../common.mk

################################################################################
# General
################################################################################

BUILT_SOURCES          =

noinst_HEADERS         =
nodist_include_HEADERS =

noinst_LTLIBRARIES     =
lib_LTLIBRARIES        =

noinst_PROGRAMS        =
eexecsbin_PROGRAMS     =
eexecbin_PROGRAMS      =
sbin_PROGRAMS          =
bin_PROGRAMS           =

EXTRA_DIST             =
MOSTLYCLEANFILES       =

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

BUILT_SOURCES   += fbsplash.h

dist-hook: 		dist-hook-header

################################################################################
# Header
################################################################################

nodist_include_HEADERS  += fbsplash.h

EXTRA_DIST              += fbsplash.h.in

MOSTLYCLEANFILES        += fbsplash.h

dist-hook-header:
	@# Removing this file from the distribution is easier than putting it
	@# in a nodist_*_SOURCES rather than the normal _SOURCES list for all
	@# the programs and libraries that depend on it.
	@rm -f $(top_distdir)/fbsplash.h

fbsplash.h: fbsplash.h.in $(top_builddir)/config.h
	@$(call infmsg,CREATE,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)rm -f $@
	$(Q)cat $< | $(SED) \
		-e 's|@bindir[@]|$(eexecbindir)|g' \
		-e 's|@sbindir[@]|$(eexecsbindir)|g' \
		-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
		-e 's|@themedir[@]|$(themedir)|g' \
		-e 's|@libdir[@]|$(eexeclibdir)|g' \
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
	../include/console_decor.h \
	../include/fbcondecor.h \
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

pkgconfigdir 		= $(libdir)/pkgconfig
pkgconfig_DATA  	= libfbsplash.pc libfbsplashrender.pc
EXTRA_DIST       	+= libfbsplash.pc.in libfbsplashrender.pc.in
MOSTLYCLEANFILES 	+= libfbsplash.pc libfbsplashrender.pc

%.pc: %.pc.in
	@$(call infmsg,CREATE,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)rm -f $@
	$(Q)cat $< | $(SED) \
		-e 's|@prefix[@]|$(prefix)|g' \
		-e 's|@exec_prefix[@]|$(exec_prefix)|g' \
		-e 's|@libdir[@]|$(libdir)|g' \
		-e 's|@includedir[@]|$(includedir)|g' \
		-e 's|@libs_private[@]|$(strip $($(basename $(@F))_la_LIBADD))|g' \
		> $@

################################################################################
# User
################################################################################

if CONFIG_FBCON_DECOR
sbin_PROGRAMS += fbcondecor_ctl
endif
sbin_PROGRAMS += fbsplashd
bin_PROGRAMS  += splash_util
eexecsbin_PROGRAMS += fbsplashctl

fbcondecor_ctl_SOURCES      = fbcon_decor_ctl.c fbcon_decor.h ../include/console_decor.h ../include/fbcondecor.h common.h fbsplash.h
fbcondecor_ctl_CPPFLAGS     = $(AM_CPPFLAGS) -DTARGET_UTIL $(libfbsplashrender_la_CFLAGS)
fbcondecor_ctl_LDFLAGS      = $(AM_LDFLAGS)
fbcondecor_ctl_LDADD        = libfbsplashrender.la libfbsplash.la

fbsplashd_SOURCES           = daemon_cmd.c daemon.c daemon.h common.h render.h fbsplash.h
fbsplashd_CPPFLAGS          = $(AM_CPPFLAGS) -DTARGET_UTIL
fbsplashd_CFLAGS            = $(AM_CFLAGS) $(PTHREAD_CFLAGS) $(RT_CFLAGS) $(libfbsplashrender_la_CFLAGS)
fbsplashd_LDFLAGS           = $(AM_LDFLAGS)
fbsplashd_LDADD             = libfbsplashrender.la libfbsplash.la $(PTHREAD_LIBS) $(RT_LIBS) $(GPM_LIBS)

splash_util_SOURCES         = util.c common.h fbsplash.h
splash_util_CPPFLAGS        = $(AM_CPPFLAGS) -DTARGET_UTIL $(libfbsplashrender_la_CFLAGS)
splash_util_LDFLAGS         = $(AM_LDFLAGS)
splash_util_LDADD           = libfbsplashrender.la libfbsplash.la

fbsplashctl_SOURCES         = $(fbsplashd_SOURCES) $(splash_util_SOURCES) fbsplashctl.c
fbsplashctl_CFLAGS          = $(AM_CFLAGS) $(PTHREAD_STATIC_CFLAGS) $(RT_CFLAGS) $(libfbsplashrender_la_CFLAGS) -DUNIFIED_BUILD
fbsplashctl_CPPFLAGS        = $(fbsplashd_CPPFLAGS) $(splash_util_CPPFLAGS)
fbsplashctl_LDFLAGS         = $(AM_LDFLAGS) -all-static
fbsplashctl_LDADD           = libfbsplashrender.la libfbsplash.la $(PTHREAD_STATIC_LIBS) $(RT_LIBS) $(GPM_LIBS)

if CONFIG_FBCON_DECOR
fbsplashctl_SOURCES += $(fbcondecor_ctl_SOURCES)
fbsplashctl_CPPFLAGS += $(fbcondecor_ctl_CPPFLAGS)
endif

install-exec-hook:
	cd $(DESTDIR)$(eexecsbindir) && \
		$(LN_S) fbsplashctl$(EXEEXT) fbsplashd.static$(EXEEXT) && \
		$(LN_S) fbsplashctl$(EXEEXT) splash_util.static$(EXEEXT)
if CONFIG_FBCON_DECOR
	cd $(DESTDIR)$(eexecsbindir) && $(LN_S) fbsplashctl$(EXEEXT) fbcondecor_ctl.static$(EXEEXT)
endif

################################################################################
# Kernel
################################################################################

if CONFIG_HELPER
eexecsbin_PROGRAMS  += fbcondecor_helper
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
	../include/console_decor.h \
	../include/fbcondecor.h \
	common.h \
	render.h \
	fbsplash.h
fbcondecor_helper_CPPFLAGS  = -DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER
fbcondecor_helper_CFLAGS    = -Os -w -ffunction-sections -fdata-sections -I.@am__isrc@
fbcondecor_helper_LDFLAGS   = 
fbcondecor_helper_LDADD     = 
fbcondecor_helper_CFLAGS   += -I$(abs_top_builddir)/libs/libjpeg -I$(LIBJPEG_SOURCE)
fbcondecor_helper_LDADD    += $(top_builddir)/libs/libjpeg.a
if CONFIG_PNG
fbcondecor_helper_CFLAGS   += -I$(LIBPNG_SOURCE)
fbcondecor_helper_LDADD    += $(top_builddir)/libs/libpng.a $(top_builddir)/libs/libz.a
endif
if CONFIG_TTF_KERNEL
fbcondecor_helper_SOURCES  += ttf.c ttf.h fbsplash.h
fbcondecor_helper_CFLAGS   += -I$(LIBFREETYPE2_SOURCE)/include -I$(LIBZ_SOURCE)
fbcondecor_helper_LDADD    += $(top_builddir)/libs/libfreetype.a
endif
if CONFIG_KLIBC_SHARED
fbcondecor_helper_LDFLAGS  += -shared
else
fbcondecor_helper_LDFLAGS  += -static
endif

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
