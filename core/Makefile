# Makefile for splashutils
#
# Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
# 
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#

PKG_VERSION = 1.1.9.10
DEBUG 	 = false		# set to true to prevent stripping
K_SHARED = false		# set to true if you want to link to a shared klibc
QUIET    = true

CC	= gcc
KLCC	= klcc

JPEGSRC	?= libs/jpeg-6b
LPNGSRC ?= libs/libpng-1.2.8
ZLIBSRC ?= libs/zlib-1.2.3
FT2SRC  ?= libs/freetype-2.1.9

prefix     =
sbindir    = ${prefix}/sbin
usrbindir  = ${prefix}/usr/bin
mandir 	   = ${prefix}/usr/share/man

INSTALL 	= /usr/bin/install -c
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

ROOT	= $(shell pwd)

# flags for the kernel utils
K_CFLAGS = -w -ffunction-sections -fdata-sections $(MISCINCS) \
	   -I$(ROOT)/$(ZLIBSRC) -I$(ROOT)/$(FT2SRC)/include -I$(ROOT)/linux/include -I$(ROOT)/linux/include2 \
	   -DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER

PNGDEFS = -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt \
	  -DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO \
	  -DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED \
	  -DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS
	  
K_LDLIBS = $(JPEGSRC)/libjpeg.a 
K_OBJS 	 = $(KOUT)/kernel.o $(KOUT)/dev.o $(KOUT)/parse.o $(KOUT)/render.o $(KOUT)/image.o \
	   $(KOUT)/cmd.o $(KOUT)/common.o $(KOUT)/list.o $(KOUT)/effects.o 
K_DEPS   = 

# flags for the user utils
CFLAGS   = -Wall -g
LDLIBS   = -ljpeg -lm -lpthread
LDFLAGS  = 
INCLUDES = -I$(ROOT)/linux/include -I/usr/include/freetype2
OBJS     = splash.o parse.o render.o image.o cmd.o common.o daemon.o list.o effects.o

# checks whether an opton is set in config.h
config_opt	= $(shell if [[ -n "`egrep '^\#define[[:space:]]+$(1)([[:space:]]+|$$)' config.h`" ]]; then echo true ; fi)

ifeq ($(call config_opt,CONFIG_TTF),true)
	OBJS   += ttf.o
	LDLIBS += -lfreetype -lz
endif

ifeq ($(call config_opt,CONFIG_MNG),true)
	OBJS   += mng_render.o mng_callbacks.o
	LDLIBS += -lmng -llcms -ljpeg -lz
endif

ifeq ($(call config_opt,CONFIG_TTF_KERNEL),true)
	K_LDLIBS += $(FT2SRC)/objs/.libs/libfreetype.a 
	K_OBJS 	 += $(KOUT)/ttf.o
	K_DEPS	 += ft2
endif

ifeq ($(call config_opt,CONFIG_PNG),true)
	K_LDLIBS += $(LPNGSRC)/libpng.a $(ZLIBSRC)/libz.a 
	K_DEPS	 += libpng
	LDLIBS	 += -lpng -lz -lm
endif	

ifeq ($(strip $(DEBUG)),true)
	STRIP = true
else	
	STRIP = strip --strip-all -R .comment -R .note
endif

KOUT	= kernel
dotg    = \e[32;01m*\e[0m
info	= if [ "${QUIET}" == "true" ] ; then printf "  %-7s %s\n" $(1) $(2); fi
install_script = $(call info,INSTALL,$(1)) ; $(INSTALL_PROG) -D scripts/$(1) $(DESTDIR)$(usrbindir)/$(1) ; \
		 sed -e 's/%PKG_VERSION%/$(PKG_VERSION)/g' -i $(DESTDIR)$(usrbindir)/$(1)

SP_HELPER = splash_helper
SP_UTIL   = splash_util
SP_GIRAM  = splash_geninitramfs
SP_RESIZE = splash_resize
SP_CONV	  = bootsplash2fbsplash
SP_MNGR	  = splash_manager

#### make rules below #####

all:	splash_kern splash_user

splash_user: $(SP_UTIL)

$(SP_UTIL): $(OBJS)
	@$(call info,LD,$@)
	$(Q)$(CC) $+ $(LDLIBS) -o $@
	$(Q)$(CC) $+ $(LDLIBS) -static -o $@.static

linux:
	@if [ ! -e "$(ROOT)/linux" ]; then \
		ln -s /lib/modules/`uname -r`/source/ "$(ROOT)/linux"; \
	fi

kdir:
	@if [ ! -d "$(ROOT)/$(KOUT)" ]; then \
		mkdir "$(ROOT)/$(KOUT)" ; \
	fi

zlib:	config.h
	@cd $(ZLIBSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call info,CONF,zlib) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS)" \
		./configure > $(OUTPUT); \
		sed -i 's#^CFLAGS=\(.*\)#CFLAGS=\1 $(K_CFLAGS)#' Makefile ; \
	fi ; \
	if ! make -q CC=$(KLCC) libz.a; then $(call info,MAKE,zlib) ; fi ; \
	make CC=$(KLCC) libz.a > $(OUTPUT)

libpng:	zlib config.h
	@cd $(LPNGSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call info,CONF,libpng) ; \
		cp scripts/makefile.linux Makefile > $(OUTPUT); \
		sed -i -e '/^CFLAGS/ { N ; s#^CFLAGS=.*#CFLAGS=$(K_CFLAGS) $(PNGDEFS)# ; P ; D }' \
		       -e 's#^ZLIBINC=.*#ZLIBINC=$(ZLIBSRC)#' \
		       -e 's#^ZLIBLIB=.*#ZLIBLIB=$(ZLIBSRC)#' Makefile ; \
	fi ; \
	if ! make -q CC=$(KLCC) libpng.a; then $(call info,MAKE,libpng) ; fi ; \
	make CC=$(KLCC) libpng.a > $(OUTPUT) 

jpeglib:
	@cd $(JPEGSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call info,CONF,libjpeg) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS)" \
		./configure > $(OUTPUT) ; \
		echo "#define NO_GETENV 1" >> jconfig.h ; \
		sed -i "s#CFLAGS=.*#CFLAGS=$(K_CFLAGS)#" Makefile ; \
		sed -i "s:#define JMESSAGE.*::" jerror.c ; \
	fi ; \
	if ! make -q CC=$(KLCC) libjpeg.a; then $(call info,MAKE,libjpeg) ; fi ; \
	make CC=$(KLCC) libjpeg.a > $(OUTPUT)

ft2:	config.h
	@cd $(FT2SRC) ; \
	if [ ! -e ./config.mk ]; then \
		$(call info,CONF,freetype2) ; \
		CC=$(KLCC) CPPFLAGS="-I$(ROOT)/$(ZLIBSRC)" \
		CFLAGS="$(K_CFLAGS) -include $(ROOT)/klibc_compat.h" \
		./configure --disable-shared > $(OUTPUT) ; \
	fi ; \
	if ! make -q CFLAGS="$(K_CFLAGS)" library; then $(call info,MAKE,freetype2) ; fi ; \
	make CFLAGS="-c $(K_CFLAGS)" library > $(OUTPUT)
	
splash_kern: linux jpeglib kdir $(K_DEPS) $(SP_HELPER) config.h $(K_LDLIBS)

$(SP_HELPER): $(K_OBJS)
	@$(call info,LD,$@)
	$(Q)$(KLCC) $(K_LDFLAGS) -o $@ $+ $(K_LDLIBS)
	$(Q)$(STRIP) $@

$(KOUT)/%.o: %.c config.h splash.h
	@$(call info,CC,$@)
	$(Q)$(KLCC) $(K_CFLAGS) -I/usr/include -c -o $@ $<

%.o: %.c config.h splash.h
	@$(call info,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -DPKG_VERSION=\"$(PKG_VERSION)\" -c -o $@ $<

clean:	clean_jpg clean_png clean_zlib clean_ft2 clean_splash

clean_splash:
	@$(call info,CLEAN,splash)
	$(Q)rm -f *.o
	$(Q)rm -f kernel/*.o
	$(Q)rm -f splash_helper splash_util splash_util.static
	$(Q)rm -f linux

clean_jpg:
	@cd $(JPEGSRC) ; \
	if [ -e Makefile ]; then \
		$(call info,CLEAN,libjpeg) ; \
		make clean > $(OUTPUT) ; \
		rm -f Makefile ; \
	fi

clean_png:
	@cd $(LPNGSRC) ; \
	if [ -e Makefile ]; then \
		$(call info,CLEAN,libpng) ; \
		make clean > $(OUTPUT) ; \
		rm -f Makefile ; \
	fi

clean_zlib:
	@cd $(ZLIBSRC) ; \
	if [ -e Makefile ]; then \
		$(call info,CLEAN,zlib) ; \
		make clean > $(OUTPUT) ; \
		rm -f Makefile ; \
	fi

clean_ft2:
	@cd $(FT2SRC) ; \
	if [ -e ./config.mk ]; then \
		$(call info,CLEAN,freetype2) ; \
		make clean > $(OUTPUT) ; \
		rm -f config.mk ; \
	fi

install: all
	@$(call info,INSTALL,$(SP_HELPER))
	$(Q)$(INSTALL_PROG) -D $(SP_HELPER) $(DESTDIR)$(sbindir)/$(SP_HELPER)
	@$(call info,INSTALL,$(SP_UTIL))
	$(Q)$(INSTALL_PROG) -D $(SP_UTIL) $(DESTDIR)$(sbindir)/$(SP_UTIL)
	@$(call info,INSTALL,$(SP_UTIL).static)
	$(Q)$(INSTALL_PROG) -D $(SP_UTIL).static $(DESTDIR)$(sbindir)/$(SP_UTIL).static
	$(Q)$(call install_script,$(SP_GIRAM))
	$(Q)$(call install_script,$(SP_CONV))
	$(Q)$(call install_script,$(SP_RESIZE))
	$(Q)$(call install_script,$(SP_MNGR))

uninstall:
	@$(call info,UNINSTALL,$(SP_HELPER))
	$(Q)rm -f $(sbindir)/$(SP_HELPER)
	@$(call info,UNINSTALL,$(SP_UTIL))
	$(Q)rm -f $(sbindir)/$(SP_UTIL)
	@$(call info,UNINSTALL,$(SP_UTIL).static)
	$(Q)rm -f $(sbindir)/$(SP_UTIL).static
	@$(call info,UNINSTALL,$(SP_GIRAM))
	$(Q)rm -f $(usrbindir)/$(SP_GIRAM)
	@$(call info,UNINSTALL,$(SP_CONV))
	$(Q)rm -f $(usrbindir)/$(SP_CONV)
	@$(call info,UNINSTALL,$(SP_RESIZE))
	$(Q)rm -f $(usrbindir)/$(SP_RESIZE)
	@$(call info,UNINSTALL,$(SP_MNGR))
	$(Q)rm -f $(usrbindir)/$(SP_MNGR)
	
