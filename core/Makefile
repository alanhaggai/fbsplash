# Makefile for splashutils
#
# Copyright (C) 2004-2007, Michal Januszewski <spock@gentoo.org>
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#

PKG_VERSION	= 1.5.2
DEBUG		= false		# set to true to prevent stripping
K_SHARED	= false		# set to true if you want to link to a shared klibc
QUIET		= true

CC			= gcc
KLCC		= klcc

# These variables can be defined on the command-line when the user
# builds splashutils against his own versions of the libraries.

JPEGSRC	?= libs/jpeg-6b
LPNGSRC ?= libs/libpng-1.2.18
ZLIBSRC ?= libs/zlib-1.2.3
FT2SRC  ?= libs/freetype-2.3.5

fbsplash_ver = 1:0:0
fbsplashr_ver = 1:0:0

LIB = $(shell grep 'define CONFIG_LIBDIR' config.h | sed -e 's/.*"\([^"]*\)".*/\1/g' -e 's!^/!!')

prefix 	  ?= /usr
prefix2	  ?= /
bindir     = ${prefix2}/bin
sbindir    = ${prefix2}/sbin
libdir     = ${prefix2}/${LIB}
usrbindir  = ${prefix}/bin
usrlibdir  = ${prefix}/${LIB}
mandir 	   = ${prefix}/share/man
incdir     = ${prefix}/include

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
	LQ = --quiet
	OUTPUT = /dev/null
else
	Q =
	LQ = 
	OUTPUT = /dev/stdout
endif

ROOT = $(shell pwd)

# Flags for the kernel utilities
K_CFLAGS = -Os -w -ffunction-sections -fdata-sections $(MISCINCS) \
		-I$(ROOT)/$(ZLIBSRC) -I$(ROOT)/$(FT2SRC)/include -I$(ROOT)/$(OBJDIR)/jpeglib \
		-DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER \
		-I$(ROOT)/$(LPNGSRC) -I$(ROOT)/$(JPEGSRC)

PNGDEFS = -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt \
		-DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO \
		-DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED \
		-DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS \
		-DPNG_NO_MMX_CODE

K_LDLIBS = $(OBJDIR)/jpeglib/libjpeg.a
K_OBJS 	 = $(KOUT)/kernel.o $(KOUT)/parse.o $(KOUT)/render.o $(KOUT)/image.o \
		   $(KOUT)/fbcon_decor.o $(KOUT)/common.o $(KOUT)/list.o $(KOUT)/effects.o $(KOUT)/libfbsplash.o \
		   $(KOUT)/libfbsplashrender.o
K_DEPS   =

# Flags for the user utilities
LDLLIBS  = -ljpeg -lm
LDLIBS   = -lm -pthread -lrt
LDFLAGS  ?=

ifeq ($(strip $(DEBUG)),true)
	STRIP = true
	CFLAGS ?= -Wall -g
else
	STRIP = strip --strip-all -R .comment -R .note
	CFLAGS ?= -O2
endif

# Include the klibc header files so that we can compile against fb.h
# from a kernel thee patched with fbcondecor.
INCLUDES = -I/usr/include/freetype2 -idirafter /usr/$(LIB)/klibc/include/linux
LIBOBJS  = $(OBJLDIR)/parse.lo $(OBJLDIR)/image.lo \
		   $(OBJLDIR)/render.lo $(OBJLDIR)/common.lo $(OBJLDIR)/list.lo \
		   $(OBJLDIR)/fbcon_decor.lo $(OBJLDIR)/effects.lo

# Checks whether an option is set in config.h
config_opt = $(shell if [ -e config.h -a -n "`egrep '^\#define[[:space:]]+$(1)([[:space:]]+|$$)' config.h 2>/dev/null`" ]; then echo true ; fi)

ifeq ($(call config_opt,CONFIG_TTF),true)
	LIBOBJS += $(OBJLDIR)/ttf.lo
	LDLLIBS += -lfreetype -lz
endif

ifeq ($(call config_opt,CONFIG_MNG),true)
	LIBOBJS += $(OBJLDIR)/mng_render.lo $(OBJLDIR)/mng_callbacks.lo
	LDLLIBS += -lmng -llcms -ljpeg -lz -lrt
endif

ifeq ($(call config_opt,CONFIG_TTF_KERNEL),true)
	K_LDLIBS += $(OBJDIR)/freetype2/.libs/libfreetype.a
	K_OBJS 	 += $(KOUT)/ttf.o
	K_DEPS	 += ft2
endif

ifeq ($(call config_opt,CONFIG_PNG),true)
	K_LDLIBS += $(LPNGSRC)/libpng.a $(ZLIBSRC)/libz.a
	K_DEPS	 += libpng
	LDLLIBS	 += -lpng -lz -lm
endif

ifeq ($(call config_opt,CONFIG_GPM),true)
	LDLIBS	 += -lgpm
endif

OBJDIR	= objs
OBJLDIR = $(OBJDIR)/lib
OBJTDIR = $(OBJDIR)/test
KOUT	= $(OBJDIR)/kernel
dotg    = \e[32;01m*\e[0m
infmsg	= if [ "${QUIET}" = "true" ] ; then printf "  %-7s %s\n" $(1) $(2) ; fi
install_script = $(call infmsg,INSTALL,$(1)) ; $(INSTALL_PROG) -D scripts/$(1) $(DESTDIR)$(usrbindir)/$(1) ; \
		 sed -e 's/%PKG_VERSION%/$(PKG_VERSION)/g' -i $(DESTDIR)$(usrbindir)/$(1)

SP_HELPER = fbcondecor_helper
SP_UTIL   = splash_util
SP_GIRAM  = splash_geninitramfs
SP_RESIZE = splash_resize
SP_CONV	  = bootsplash2fbsplash
SP_MNGR	  = splash_manager

# Don't pass command line variables when calling make. This saves
# us trouble when building against klibc.
MAKEOVERRIDES=

TARGETS   = objdir $(OBJDIR)/libfbsplash.la $(OBJDIR)/libfbsplashrender.la splash_user
ifeq ($(call config_opt,CONFIG_HELPER),true)
	TARGETS += splash_kern
endif
ifeq ($(call config_opt,CONFIG_FBCON_DECOR),true)
	TARGETS += $(OBJDIR)/fbcondecor_ctl
endif

#### make rules below #####

all:	$(TARGETS)

$(OBJDIR)/libfbsplash.la: $(OBJLDIR)/libfbsplash.lo
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) -version-info $(fbsplash_ver) $(LDFLAGS) -o $(OBJDIR)/libfbsplash.la $+ -rpath $(usrlibdir)

$(OBJDIR)/libfbsplashrender.la: $(OBJLDIR)/libfbsplashrender.lo $(LIBOBJS)
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) -version-info $(fbsplashr_ver) $(LDFLAGS) -o $(OBJDIR)/libfbsplashrender.la $+ $(LDLLIBS) $(OBJDIR)/libfbsplash.la -rpath $(usrlibdir)

splash_user: $(OBJDIR)/$(SP_UTIL) $(OBJDIR)/$(SP_UTIL).static $(OBJDIR)/fbsplashd $(OBJDIR)/fbsplashd.static

$(OBJDIR)/fbcondecor_ctl: $(OBJDIR)/libfbsplashrender.la $(OBJDIR)/fbcon_decor_ctl.o
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) $(LDFLAGS) $+ -o $(OBJDIR)/libfbsplashrender.la $(OBJDIR)/libfbsplash.la -o $@

config.h:
	@echo "Using default configuration."
	@if [ ! -e config.h ]; then \
		./configure --default ; \
		$(warning "Restart make") \
		exit 1 ; \
	fi

$(OBJDIR)/fbsplash.h: fbsplash.h config.h
	$(Q)cat fbsplash.h | sed -e "s!#define LIBDIR.*!#define LIBDIR \"/$(LIB)\"!" > objs/fbsplash.h

$(OBJDIR)/$(SP_UTIL): $(OBJDIR)/util.o $(OBJDIR)/libfbsplash.la $(OBJDIR)/libfbsplashrender.la
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) $(LDFLAGS) $+ $(OBJDIR)/libfbsplashrender.la $(OBJDIR)/libfbsplash.la -o $@

$(OBJDIR)/$(SP_UTIL).static: $(OBJDIR)/util.o $(OBJDIR)/libfbsplash.la $(OBJDIR)/libfbsplashrender.la
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) -all-static $+ $(OBJDIR)/libfbsplashrender.la $(OBJDIR)/libfbsplash.la $(LDLIBS) -L/usr/lib/nptl -o $@

$(OBJDIR)/fbsplashd: $(OBJDIR)/daemon.o $(OBJDIR)/daemon_cmd.o $(OBJDIR)/libfbsplash.la $(OBJDIR)/libfbsplashrender.la
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) $(LDFLAGS) $(LDLIBS) $+ $(OBJDIR)/libfbsplashrender.la $(OBJDIR)/libfbsplash.la -o $@

$(OBJDIR)/fbsplashd.static: $(OBJDIR)/daemon.o $(OBJDIR)/daemon_cmd.o $(OBJDIR)/libfbsplash.la $(OBJDIR)/libfbsplashrender.la
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) -all-static $(LDFLAGS) $(LDLIBS) $+ $(OBJDIR)/libfbsplashrender.la $(OBJDIR)/libfbsplash.la -o $@

objdir:
	@if [ ! -d "$(OBJDIR)" ]; then \
		mkdir "$(OBJDIR)" ; \
	fi
	@if [ ! -d "$(KOUT)" ]; then \
		mkdir "$(KOUT)" ; \
	fi
	@if [ ! -d "$(OBJLDIR)" ]; then \
		mkdir "${OBJLDIR}" ; \
	fi
	@if [ ! -d "$(OBJTDIR)" ]; then \
		mkdir "$(OBJTDIR)" ; \
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
		CFLAGS="$(K_CFLAGS) -include $(ROOT)/klibc_compat.h" \
		CC=$(KLCC) CPPFLAGS="-I$(ROOT)/$(ZLIBSRC)" \
		$(ROOT)/$(FT2SRC)/configure --disable-shared > $(OUTPUT) ; \
	fi ; \
	if ! make -q CFLAGS="$(K_CFLAGS)" library; then $(call infmsg,MAKE,freetype2) ; fi ; \
	make CFLAGS="-c $(K_CFLAGS)" CCexe="$(CC)" library > $(OUTPUT)

splash_kern: jpeglib $(K_DEPS) $(OBJDIR)/$(SP_HELPER) config.h $(K_LDLIBS)

$(OBJDIR)/$(SP_HELPER): $(K_OBJS)
	@$(call infmsg,LD,$@)
	$(Q)$(KLCC) $(K_LDFLAGS) -o $@ $+ $(K_LDLIBS)
	$(Q)$(STRIP) $@

$(KOUT)/%.o: %.c config.h $(OBJDIR)/fbsplash.h
	@$(call infmsg,CC,$@)
	$(Q)$(KLCC) $(K_CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c config.h $(OBJDIR)/fbsplash.h daemon.h util.h
	@$(call infmsg,CC,$@)
	$(Q)$(CC) $(CFLAGS) -DTARGET_UTIL $(INCLUDES) -DPKG_VERSION=\"$(PKG_VERSION)\" -c -o $@ $<

$(OBJLDIR)/%.lo: %.c config.h $(OBJDIR)/fbsplash.h
	@$(call infmsg,CC,$@)
	$(Q)libtool $(LQ) --mode=compile $(CC) $(CFLAGS) $(INCLUDES) -DPKG_VERSION=\"$(PKG_VERSION)\" -c -o $@ $<

doc:
	doxygen

tests:	objdir $(OBJDIR)/test_parser

$(OBJDIR)/test_parser: $(OBJDIR)/test/test_parser.o $(OBJDIR)/libfbsplash.la $(OBJDIR)/libfbsplashrender.la
	@$(call infmsg,LD,$@)
	$(Q)libtool $(LQ) --tag=CC --mode=link $(CC) $(LDFLAGS) $(LDLIBS) $+ -o $@

clean:	clean_all

clean_all:	clean_zlib clean_png clean_test clean_splash
	@$(call infmsg,CLEAN,all)
	$(Q)rm -rf $(OBJDIR)/*

clean_test:
	@$(call infmsg,CLEAN,test)
	$(Q)rm -rf $(OBJDIR)/test

clean_splash:
	@$(call infmsg,CLEAN,splash)
	$(Q)rm -f $(OBJDIR)/{*.o,splash_util,*.so,*.lo,splash_util.static,fbcondecor_helper}
	$(Q)rm -rf $(OBJDIR)/lib
	$(Q)rm -rf $(OBJDIR)/kernel
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
ifeq ($(call config_opt,CONFIG_HELPER),true)
	@$(call infmsg,INSTALL,$(SP_HELPER))
	$(Q)$(INSTALL_PROG) -D $(OBJDIR)/$(SP_HELPER) $(DESTDIR)$(sbindir)/$(SP_HELPER)
	$(Q)ln -sf $(SP_HELPER) $(DESTDIR)$(sbindir)/splash_helper
	$(Q)$(call install_script,$(SP_GIRAM))
endif
	@$(call infmsg,INSTALL,libfbsplash.so)
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/libfbsplash.la $(DESTDIR)$(usrlibdir)/libfbsplash.la
	@$(call infmsg,INSTALL,libfbsplashrender.so)
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/libfbsplashrender.la $(DESTDIR)$(usrlibdir)/libfbsplashrender.la
	@$(call infmsg,INSTALL,fbsplashd)
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/fbsplashd $(DESTDIR)$(sbindir)/fbsplashd
	@$(call infmsg,INSTALL,fbsplashd.static)
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/fbsplashd.static $(DESTDIR)$(sbindir)/fbsplashd.static
	@$(call infmsg,INSTALL,$(SP_UTIL))
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/$(SP_UTIL) $(DESTDIR)$(usrbindir)/$(SP_UTIL)
	@$(call infmsg,INSTALL,$(SP_UTIL).static)
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/$(SP_UTIL).static $(DESTDIR)$(bindir)/$(SP_UTIL).static
ifeq ($(call config_opt,CONFIG_FBCON_DECOR),true)
	@$(call infmsg,INSTALL,fbcondecor_ctl)
	$(Q)libtool $(LQ) --mode=install $(INSTALL_PROG) -D $(OBJDIR)/fbcondecor_ctl $(DESTDIR)$(sbindir)/fbcondecor_ctl
endif
	@$(call infmsg,INSTALL,fbsplash.h)
	$(Q)$(INSTALL_PROG) -D objs/fbsplash.h $(DESTDIR)$(incdir)/fbsplash.h
	@$(call infmsg,INSTALL,splash-functions.sh)
	$(Q)$(INSTALL_PROG) -D scripts/splash-functions.sh $(DESTDIR)$(sbindir)/splash-functions.sh
	$(Q)$(call install_script,$(SP_CONV))
	$(Q)$(call install_script,$(SP_RESIZE))
	$(Q)$(call install_script,$(SP_MNGR))
	$(Q)cp -f libfbsplash.pc libfbsplashrender.pc $(OBJDIR)/
	$(Q)sed -i -e 's#prefix=.*#prefix=$(prefix)#' $(OBJDIR)/libfbsplash.pc
	$(Q)sed -i -e 's#prefix=.*#prefix=$(prefix)#' $(OBJDIR)/libfbsplashrender.pc
	$(Q)sed -i -e 's#libdir=.*#libdir=$(usrlibdir)#' $(OBJDIR)/libfbsplash.pc
	$(Q)sed -i -e 's#libdir=.*#libdir=$(usrlibdir)#' $(OBJDIR)/libfbsplashrender.pc
	$(Q)sed -i -e 's#Libs.private: .*#Libs.private: -lfbsplash $(LDLLIBS)#' $(OBJDIR)/libfbsplashrender.pc
	@$(call infmsg,INSTALL,libfbsplash.pc)
	$(Q)$(INSTALL_PROG) -D -m 0664 $(OBJDIR)/libfbsplash.pc $(DESTDIR)$(usrlibdir)/pkgconfig/libfbsplash.pc
	@$(call infmsg,INSTALL,libfbsplashrender.pc)
	$(Q)$(INSTALL_PROG) -D -m 0664 $(OBJDIR)/libfbsplashrender.pc $(DESTDIR)$(usrlibdir)/pkgconfig/libfbsplashrender.pc

uninstall:
ifeq ($(call config_opt,CONFIG_HELPER),true)
	@$(call infmsg,UNINSTALL,$(SP_HELPER))
	$(Q)rm -f $(sbindir)/$(SP_HELPER)
	$(Q)rm -f $(sbindir)/splash_helper
	@$(call infmsg,UNINSTALL,$(SP_GIRAM))
	$(Q)rm -f $(usrbindir)/$(SP_GIRAM)
endif
ifeq ($(call config_opt,CONFIG_FBCON_DECOR),true)
	@$(call infmsg,UNINSTALL,fbcondecor_ctl)
	$(Q)rm -f $(sbindir)/fbcondecor_ctl
endif
	@$(call infmsg,UNINSTALL,fbsplashd)
	$(Q)rm -f $(sbindir)/fbsplashd
	@$(call infmsg,UNINSTALL,fbsplashd.static)
	$(Q)rm -f $(sbindir)/fbsplashd.static
	@$(call infmsg,UNINSTALL,$(SP_UTIL))
	$(Q)rm -f $(usrbindir)/$(SP_UTIL)
	@$(call infmsg,UNINSTALL,$(SP_UTIL).static)
	$(Q)rm -f $(bindir)/$(SP_UTIL).static
	@$(call infmsg,UNINSTALL,libfbsplash.so)
	$(Q)libtool $(LQ) --mode=uninstall rm -f $(usrlibdir)/libfbsplash.so
	@$(call infmsg,UNINSTALL,libfbsplashrender.so)
	$(Q)libtool $(LQ) --mode=uninstall rm -f $(usrlibdir)/libfbsplashrender.so
	@$(call insmsg,UNINSTALL,fbsplash.h)
	$(Q)rm -f $(incdir)/fbsplash.h
	@$(call infmsg,UNINSTALL,splash-functions.sh)
	$(Q)rm -f $(sbindir)/splash-functions.sh
	@$(call infmsg,UNINSTALL,$(SP_CONV))
	$(Q)rm -f $(usrbindir)/$(SP_CONV)
	@$(call infmsg,UNINSTALL,$(SP_RESIZE))
	$(Q)rm -f $(usrbindir)/$(SP_RESIZE)
	@$(call infmsg,UNINSTALL,$(SP_MNGR))
	$(Q)rm -f $(usrbindir)/$(SP_MNGR)
	@$(call infmsg,UNINSTALL,libfbsplash.pc)
	$(Q)rm -f $(usrlibdir)/pkgconfig/libfbsplash.pc
	@$(call infmsg,UNINSTALL,libfbsplashrender.pc)
	$(Q)rm -f $(usrlibdir)/pkgconfig/libfbsplashrender.pc

