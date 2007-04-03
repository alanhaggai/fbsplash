# Makefile for splashutils
#
# Copyright (C) 2004-2007, Michal Januszewski <spock@gentoo.org>
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#

PKG_VERSION	= 1.4
DEBUG		= false		# set to true to prevent stripping
K_SHARED	= false		# set to true if you want to link to a shared klibc
QUIET		= true

CC			= gcc
KLCC		= klcc

# These variables can be defined on the command-line when the user
# builds splashutils against his own versions of the libraries.

JPEGSRC	?= libs/jpeg-6b
LPNGSRC ?= libs/libpng-1.2.8
ZLIBSRC ?= libs/zlib-1.2.3
FT2SRC  ?= libs/freetype-2.1.9

LIB		?= lib

prefix     =
sbindir    = ${prefix}/sbin
libdir     = ${prefix}/lib
usrbindir  = ${prefix}/usr/bin
mandir 	   = ${prefix}/usr/share/man

INSTALL 		= /usr/bin/install -c
INSTALL_PROG 	= ${INSTALL}
INSTALL_DATA 	= ${INSTALL} -m 644
INSTALL_SCRIPT 	= ${INSTALL_PROG}

ifeq ($(strip $(K_SHARED)),true)
	K_LDFLAGS = -shared
else
	K_LDFLAGS = -static
endif

ifeq ($(strip $(QUIET)),true)
	Q = @
	OUTPUT = /dev/null
else
	Q =
	OUTPUT = /dev/stdout
endif

ROOT = $(shell pwd)

# Flags for the kernel utilities
K_CFLAGS = -w -ffunction-sections -fdata-sections $(MISCINCS) \
		-I$(ROOT)/$(ZLIBSRC) -I$(ROOT)/$(FT2SRC)/include -I$(ROOT)/$(OBJDIR)/jpeglib \
		-DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER

PNGDEFS = -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt \
		-DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO \
		-DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED \
		-DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS

K_LDLIBS = $(OBJDIR)/jpeglib/libjpeg.a
K_OBJS 	 = $(KOUT)/kernel.o $(KOUT)/parse.o $(KOUT)/render.o $(KOUT)/image.o \
		   $(KOUT)/cmd.o $(KOUT)/common.o $(KOUT)/list.o $(KOUT)/effects.o $(KOUT)/libsplash.o
K_DEPS   =

# Flags for the user utilities
CFLAGS   = -Wall -g
LDLIBS   = -ljpeg -lm -pthread
LDFLAGS  =

# Include the klibc header files so that we can compile against fb.h
# from a kernel thee patched with fbsplash.
INCLUDES = -I/usr/include/freetype2 -idirafter /usr/$(LIB)/klibc/include/linux
OBJS     = $(OBJDIR)/splash.o $(OBJDIR)/parse.o $(OBJDIR)/render.o $(OBJDIR)/image.o \
		   $(OBJDIR)/cmd.o $(OBJDIR)/common.o $(OBJDIR)/daemon.o $(OBJDIR)/daemon_cmd.o \
	       $(OBJDIR)/list.o $(OBJDIR)/effects.o $(OBJDIR)/libsplash.o

# Checks whether an option is set in config.h
config_opt = $(shell if [ -e config.h -a -n "`egrep '^\#define[[:space:]]+$(1)([[:space:]]+|$$)' config.h`" ]; then echo true ; fi)

ifeq ($(call config_opt,CONFIG_TTF),true)
	OBJS   += $(OBJDIR)/ttf.o
	LDLIBS += -lfreetype -lz
endif

ifeq ($(call config_opt,CONFIG_MNG),true)
	OBJS   += $(OBJDIR)/mng_render.o $(OBJDIR)/mng_callbacks.o
	LDLIBS += -lmng -llcms -ljpeg -lz
endif

ifeq ($(call config_opt,CONFIG_TTF_KERNEL),true)
	K_LDLIBS += $(OBJDIR)/freetype2/.libs/libfreetype.a
	K_OBJS 	 += $(KOUT)/ttf.o
	K_DEPS	 += ft2
endif

ifeq ($(call config_opt,CONFIG_PNG),true)
	K_LDLIBS += $(LPNGSRC)/libpng.a $(ZLIBSRC)/libz.a
	K_DEPS	 += libpng
	LDLIBS	 += -lpng -lz -lm
endif

ifeq ($(call config_opt,CONFIG_GPM),true)
	LDLIBS	 += -lgpm
endif

ifeq ($(strip $(DEBUG)),true)
	STRIP = true
else
	STRIP = strip --strip-all -R .comment -R .note
endif

OBJDIR	= objs
KOUT	= $(OBJDIR)/kernel
dotg    = \e[32;01m*\e[0m
infmsg	= if [ "${QUIET}" = "true" ] ; then printf "  %-7s %s\n" $(1) $(2) ; fi
install_script = $(call infmsg,INSTALL,$(1)) ; $(INSTALL_PROG) -D scripts/$(1) $(DESTDIR)$(usrbindir)/$(1) ; \
		 sed -e 's/%PKG_VERSION%/$(PKG_VERSION)/g' -i $(DESTDIR)$(usrbindir)/$(1)

SP_HELPER = splash_helper
SP_UTIL   = splash_util
SP_GIRAM  = splash_geninitramfs
SP_RESIZE = splash_resize
SP_CONV	  = bootsplash2fbsplash
SP_MNGR	  = splash_manager

#### make rules below #####

all:	objdir $(OBJDIR)/libsplash.so splash_kern splash_user

$(OBJDIR)/libsplash.so: $(OBJDIR)/libsplash.o
	@$(call infmsg,LD,$@)
	$(Q)$(CC) $(LDFLAGS) -fPIC -shared -Wl,-soname,libsplash.so -o $@ -leinfo $+

splash_user: $(OBJDIR)/$(SP_UTIL)

config.h:
	@echo "Using default configuration."
	@if [ ! -e config.h ]; then \
		./configure --default ; \
		$(warning "Restart make") \
		exit 1 ; \
	fi

$(OBJDIR)/$(SP_UTIL): $(OBJS)
	@$(call infmsg,LD,$@)
	$(Q)$(CC) $+ $(LDLIBS) -o $@
	$(Q)$(CC) $+ $(LDLIBS) -L/usr/lib/nptl -static -o $@.static

objdir:
	@if [ ! -d "$(OBJDIR)" ]; then \
		mkdir "$(OBJDIR)" ; \
	fi
	@if [ ! -d "$(KOUT)" ]; then \
		mkdir "$(KOUT)" ; \
	fi

zlib:	config.h
	@cd $(ZLIBSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call infmsg,CONF,zlib) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS)" \
		./configure > $(OUTPUT); \
		sed -i 's#^CFLAGS=\(.*\)#CFLAGS=\1 $(K_CFLAGS)#' Makefile ; \
	fi ; \
	if ! make -q CC=$(KLCC) libz.a; then $(call infmsg,MAKE,zlib) ; fi ; \
	make CC=$(KLCC) libz.a > $(OUTPUT)

libpng:	zlib config.h
	@cd $(LPNGSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call infmsg,CONF,libpng) ; \
		cp scripts/makefile.linux Makefile > $(OUTPUT); \
		sed -i -e '/^CFLAGS/ { N ; s#^CFLAGS=.*#CFLAGS=$(K_CFLAGS) $(PNGDEFS)# ; P ; D }' \
		       -e 's#^ZLIBINC=.*#ZLIBINC=$(ZLIBSRC)#' \
		       -e 's#^ZLIBLIB=.*#ZLIBLIB=$(ZLIBSRC)#' Makefile ; \
	fi ; \
	if ! make -q CC=$(KLCC) libpng.a; then $(call infmsg,MAKE,libpng) ; fi ; \
	make CC=$(KLCC) libpng.a > $(OUTPUT)

jpeglib:
	@cd $(OBJDIR) ; \
	if [ ! -d jpeglib ] ; then \
		mkdir jpeglib ; \
	fi ; \
	cd jpeglib ; \
	if [ ! -e ./Makefile ]; then \
		$(call infmsg,CONF,libjpeg) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS) -I$(ROOT)/$(OBJDIR)/jpeglib" \
		$(ROOT)/$(JPEGSRC)/configure > $(OUTPUT) ; \
		echo "#define NO_GETENV 1" >> jconfig.h ; \
		sed -i "s#CFLAGS=.*#CFLAGS=$(K_CFLAGS) -I$(ROOT)/$(OBJDIR)/jpeglib#" Makefile ; \
		sed -i "s:#define JMESSAGE.*::" $(ROOT)/$(JPEGSRC)/jerror.c ; \
	fi ; \
	if ! make -q CC=$(KLCC) libjpeg.a; then $(call infmsg,MAKE,libjpeg) ; fi ; \
	make CC=$(KLCC) libjpeg.a > $(OUTPUT)

ft2:	config.h
	@cd $(OBJDIR) ; \
	if [ ! -d freetype2 ] ; then \
		mkdir freetype2 ; \
	fi ; \
	cd freetype2 ; \
	if [ ! -e ./config.mk ]; then \
		$(call infmsg,CONF,freetype2) ; \
		sh -c 'export CFLAGS="$(K_CFLAGS) -include $(ROOT)/klibc_compat.h" \
		CC=$(KLCC) CPPFLAGS="-I$(ROOT)/$(ZLIBSRC)" ; \
		$(ROOT)/$(FT2SRC)/configure --disable-shared > $(OUTPUT)' ; \
	fi ; \
	if ! make -q CFLAGS="$(K_CFLAGS)" library; then $(call infmsg,MAKE,freetype2) ; fi ; \
	make CFLAGS="-c $(K_CFLAGS)" library > $(OUTPUT)

splash_kern: jpeglib $(K_DEPS) $(OBJDIR)/$(SP_HELPER) config.h $(K_LDLIBS)

$(OBJDIR)/$(SP_HELPER): $(K_OBJS)
	@$(call infmsg,LD,$@)
	$(Q)$(KLCC) $(K_LDFLAGS) -o $@ $+ $(K_LDLIBS)
	$(Q)$(STRIP) $@

$(KOUT)/%.o: %.c config.h splash.h
	@$(call infmsg,CC,$@)
	$(Q)$(KLCC) $(K_CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c config.h splash.h daemon.h
	@$(call infmsg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -DPKG_VERSION=\"$(PKG_VERSION)\" -DLIBDIR=\"$(LIB)\" -c -o $@ $<

$(OBJDIR)/libsplash.o: libsplash.c splash.h config.h
	@$(call infmsg,CC,$@)
	$(Q)$(CC) $(CFLAGS) -fPIC $(INCLUDES) -DPKG_VERSION=\"$(PKG_VERSION)\" -DLIBDIR=\"$(LIB)\" -c -o $@ $<

clean:	clean_all

clean_all:	clean_zlib clean_png
	@$(call infmsg,CLEAN,all)
	$(Q)rm -rf $(OBJDIR)/*

clean_splash:
	@$(call infmsg,CLEAN,splash)
	$(Q)rm -f $(OBJDIR)/*.o
	$(Q)rm -f $(OBJDIR)/kernel/*.o
	$(Q)rm -f $(OBJDIR)/splash_helper splash_util splash_util.static
	$(Q)rm -r config.h

clean_jpg:
	$(Q)cd $(OBJDIR) ; \
	if [ -e jpeglib ]; then \
		cd jpeglib ; \
	fi ; \
	if [ -e Makefile ]; then \
		$(call infmsg,CLEAN,libjpeg) ; \
		make clean > $(OUTPUT) ; \
		rm -f Makefile ; \
	fi

clean_png:
	$(Q)cd $(LPNGSRC) ; \
	if [ -e Makefile ]; then \
		$(call infmsg,CLEAN,libpng) ; \
		make clean > $(OUTPUT) ; \
		rm -f Makefile ; \
	fi

clean_zlib:
	$(Q)cd $(ZLIBSRC) ; \
	if [ -e Makefile ]; then \
		$(call infmsg,CLEAN,zlib) ; \
		make clean > $(OUTPUT) ; \
		rm -f Makefile ; \
	fi

clean_ft2:
	$(Q)cd $(OBJDIR) ; \
	if [ -e freetype2 ]; then \
		cd freetype2 ; \
	fi ; \
	if [ -e ./config.mk ]; then \
		$(call infmsg,CLEAN,freetype2) ; \
		make clean > $(OUTPUT) ; \
		rm -f config.mk ; \
	fi

install: all
	@$(call infmsg,INSTALL,$(SP_HELPER))
	$(Q)$(INSTALL_PROG) -D $(OBJDIR)/$(SP_HELPER) $(DESTDIR)$(sbindir)/$(SP_HELPER)
	@$(call infmsg,INSTALL,$(SP_UTIL))
	$(Q)$(INSTALL_PROG) -D $(OBJDIR)/$(SP_UTIL) $(DESTDIR)$(sbindir)/$(SP_UTIL)
	@$(call infmsg,INSTALL,$(SP_UTIL).static)
	$(Q)$(INSTALL_PROG) -D $(OBJDIR)/$(SP_UTIL).static $(DESTDIR)$(sbindir)/$(SP_UTIL).static
	@$(call infmsg,INSTALL,libsplash.so)
	$(Q)$(INSTALL_PROG) -D $(OBJDIR)/libsplash.so $(DESTDIR)$(libdir)/libsplash.so
	@$(call infmsg,INSTALL,splash-functions.sh)
	$(Q)$(INSTALL_PROG) -D scripts/splash-functions.sh $(DESTDIR)$(sbindir)/splash-functions.sh
	$(Q)$(call install_script,$(SP_GIRAM))
	$(Q)$(call install_script,$(SP_CONV))
	$(Q)$(call install_script,$(SP_RESIZE))
	$(Q)$(call install_script,$(SP_MNGR))

uninstall:
	@$(call infmsg,UNINSTALL,$(SP_HELPER))
	$(Q)rm -f $(sbindir)/$(SP_HELPER)
	@$(call infmsg,UNINSTALL,$(SP_UTIL))
	$(Q)rm -f $(sbindir)/$(SP_UTIL)
	@$(call infmsg,UNINSTALL,$(SP_UTIL).static)
	$(Q)rm -f $(sbindir)/$(SP_UTIL).static
	@$(call infmsg,UNINSTALL,libsplash.so)
	$(Q)rm -f $(libdir)/libsplash.so
	@$(call infmsg,UNINSTALL,splash-functions.sh)
	$(Q)rm -f $(sbindir)/splash-functions.sh
	@$(call infmsg,UNINSTALL,$(SP_GIRAM))
	$(Q)rm -f $(usrbindir)/$(SP_GIRAM)
	@$(call infmsg,UNINSTALL,$(SP_CONV))
	$(Q)rm -f $(usrbindir)/$(SP_CONV)
	@$(call infmsg,UNINSTALL,$(SP_RESIZE))
	$(Q)rm -f $(usrbindir)/$(SP_RESIZE)
	@$(call infmsg,UNINSTALL,$(SP_MNGR))
	$(Q)rm -f $(usrbindir)/$(SP_MNGR)

