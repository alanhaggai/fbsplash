# Makefile for splashutils
#
# Copyright (C) 2004=2005, Michael Januszewski <spock@gentoo.org>
# 
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#

PKG_VERSION = 0.9.2
DEBUG 	 = false		# set to true to prevent stripping
K_SHARED = false		# set to true if you want to link to a shared klibc
QUIET    = true

JPEGSRC	= libs/jpeg-6b
LPNGSRC = libs/libpng-1.2.8
ZLIBSRC = libs/zlib-1.2.1
LIBCSRC	= libs/klibc-0.199/klibc

prefix     =
sbindir    = ${prefix}/sbin
usrbindir  = ${prefix}/usr/bin
mandir 	   = ${prefix}/usr/share/man

INSTALL 	= /usr/bin/install -c
INSTALL_PROG 	= ${INSTALL}
INSTALL_DATA 	= ${INSTALL} -m 644
INSTALL_SCRIPT 	= ${INSTALL_PROG}

ifeq ($(strip $(K_SHARED)),true)
	LIBC = $(LIBCSRC)/libc.so
	CRT0 = $(LIBCSRC)/interp.o 
	K_LDFLAGS =
else 
	LIBC = $(LIBCSRC)/libc.a
	CRT0 = $(LIBCSRC)/crt0.o
	K_LDFLAGS = --gc-sections
endif

ifeq ($(strip $(QUIET)),true)
	OUTPUT = /dev/null
else
	OUTPUT = /dev/stdout
endif

LIBGCC	= $(shell $(CC) --print-libgcc)
ROOT	= $(shell pwd)
ARCH    = $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/)

include $(LIBCSRC)/arch/$(ARCH)/MCONFIG

# flags for the kernel utils
K_CFLAGS = -w $(OPTFLAGS) $(ARCHREQFLAGS) -nostdinc -iwithprefix include -I${ROOT}/$(LIBCSRC)/../ \
	   -I${ROOT}/$(LIBCSRC)/../include/arch/$(ARCH) \
	   -I${ROOT}/$(LIBCSRC)/../include/bits$(BITSIZE) \
	   -D__KLIBC__ -DBITSIZE=$(BITSIZE) -I${ROOT}/$(LIBCSRC)/../include -I${ROOT}/linux/include $(MISCINCS) \
	   -DWITH_ERRLIST -I${ROOT}/$(ZLIBSRC) -I/usr/include -ffunction-sections -fdata-sections -DTARGET_KERNEL

PNGDEFS = -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt \
	  -DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO \
	  -DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED \
	  -DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS
	  
K_LDLIBS = $(JPEGSRC)/libjpeg.a $(LPNGSRC)/libpng.a $(ZLIBSRC)/libz.a

# flags for the user utills
CFLAGS = -Wall -g -I${ROOT}/linux/include -I/usr/include/freetype2
LDLIBS = -ljpeg -lpng -lz -lm -lfreetype
LDFLAGS = 

ifeq ($(strip $(DEBUG)),true)
	STRIP = true
else	
	STRIP = strip --strip-all -R .comment -R .note
endif

KOUT	= kernel
dotg    = \e[32;01m*\e[0m
info	= printf "  %-7s %s\n" $(1) $(2)
install_script = $(call info,INSTALL,$(1)) ; $(INSTALL_PROG) -D scripts/$(1) $(DESTDIR)$(usrbindir)/$(1) ; \
		 sed -e 's/%PKG_VERSION%/$(PKG_VERSION)/g' -i $(DESTDIR)$(usrbindir)/$(1)

SP_HELPER = splash_helper
SP_UTIL   = splash_util
SP_GIRAM  = splash_geninitramfs
SP_RESIZE = splash_resize
SP_CONV	  = bootsplash2fbsplash

#### make rules below #####

all:	splash_kern splash_user

splash_user: $(SP_UTIL)

$(SP_UTIL): splash.o parse.o render.o image.o cmd.o common.o daemon.o list.o ttf.o effects.o
	@$(call info,LD,$@)
	@$(CC) $+ $(LDLIBS) -o $@
	@$(CC) $+ $(LDLIBS) -static -o $@.static

linux:
	@if [ ! -e linux ]; then \
		ln -s /lib/modules/`uname -r`/source/ linux; \
	fi
	@if [ ! -d $(KOUT) ]; then \
		mkdir $(KOUT) ; \
	fi

klibc: linux
	@if [ ! -e linux ]; then \
		echo "The 'linux' symlink does not exist!" ; \
		exit 1 ; \
	fi
	@cd $(LIBCSRC) ; \
	if ! make -q; then $(call info,MAKE,klibc) ; fi ; \
	make > $(OUTPUT)

zlib:	
#	@$(call info,BUILD,zlib)
	@cd $(ZLIBSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call info,CONF,zlib) ; \
		./configure > /dev/null; \
		sed -i 's#^CFLAGS=\(.*\)#CFLAGS=\1 $(K_CFLAGS)#' Makefile ; \
	fi ; \
	if ! make -q libz.a; then $(call info,MAKE,zlib) ; fi ; \
	make libz.a > $(OUTPUT)

libpng:	zlib
#	@$(call info,BUILD,libpng)
	@cd $(LPNGSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call info,CONF,libpng) ; \
		cp scripts/makefile.linux Makefile > /dev/null; \
		sed -i -e '/^CFLAGS/ { N ; s#^CFLAGS=.*#CFLAGS=$(K_CFLAGS) $(PNGDEFS)# ; P ; D }' \
		       -e 's#^ZLIBINC=.*#ZLIBINC=$(ZLIBSRC)#' \
		       -e 's#^ZLIBLIB=.*#ZLIBLIB=$(ZLIBSRC)#' Makefile ; \
	fi ; \
	if ! make -q libpng.a; then $(call info,MAKE,libpng) ; fi ; \
	make libpng.a > $(OUTPUT) 

jpeglib:
#	@$(call info,BUILD,libjpeg)
	@cd $(JPEGSRC) ; \
	if [ ! -e ./Makefile ]; then \
		$(call info,CONF,libjpeg) ; \
		./configure > /dev/null ; \
		echo "#define NO_GETENV 1" >> jconfig.h ; \
		sed -i "s#CFLAGS=.*#CFLAGS=$(K_CFLAGS)#" Makefile ; \
		sed -i "s:#define JMESSAGE.*::" jerror.c ; \
	fi ; \
	if ! make -q libjpeg.a; then $(call info,MAKE,libjpeg) ; fi ; \
	make libjpeg.a > $(OUTPUT)

splash_kern: klibc jpeglib libpng $(SP_HELPER) $(LIBC) config.h $(K_LDLIBS)

$(SP_HELPER): $(KOUT)/kernel.o $(KOUT)/dev.o $(KOUT)/parse.o $(KOUT)/render.o $(KOUT)/image.o \
	      $(KOUT)/cmd.o $(KOUT)/common.o $(KOUT)/list.o $(KOUT)/effects.o
	@$(call info,LD,$@)
ifeq ($(strip $(K_SHARED)),true)
	@ld $(K_LDFLAGS) -o $@ -e main $(CRT0) $+ $(K_LDLIBS) -R $(LIBC) $(LIBGCC)
else
	@ld $(K_LDFLAGS) -o $@ $(CRT0) $+ $(K_LDLIBS) $(LIBC) $(LIBGCC) 
endif
	@$(STRIP) $@

$(KOUT)/%.o: %.c config.h
	@$(call info,CC,$@)
	@$(CC) $(K_CFLAGS) -c -o $@ $<

%.o: %.c config.h
	@$(call info,CC,$@)
	@$(CC) $(CFLAGS) -DPKG_VERSION=\"$(PKG_VERSION)\" -c -o $@ $<

clean:	clean_klibc clean_jpg clean_png clean_zlib clean_splash

clean_splash:
	@$(call info,CLEAN,splash)
	@rm -f *.o
	@rm -f kernel/*.o
	@rm -f splash_helper splash_util splash_util.static
	@rm -f linux

clean_jpg:
	@cd $(JPEGSRC) ; \
	if [ -e Makefile ]; then \
		$(call info,CLEAN,libjpeg) ; \
		make clean > /dev/null ; \
		rm Makefile ; \
	fi

clean_png:
	@cd $(LPNGSRC) ; \
	if [ -e Makefile ]; then \
		$(call info,CLEAN,libpng) ; \
		make clean > /dev/null ; \
		rm Makefile ; \
	fi

clean_zlib:
	@cd $(ZLIBSRC) ; \
	if [ -e Makefile ]; then \
		$(call info,CLEAN,zlib) ; \
		make clean > /dev/null ; \
		rm Makefile ; \
	fi

clean_klibc:
	@cd $(LIBCSRC) ; \
	$(call info,CLEAN,klibc) ; \
	make clean > /dev/null 2> /dev/null

install: all
	@$(call info,INSTALL,$(SP_HELPER)) ; $(INSTALL_PROG) -D $(SP_HELPER) $(DESTDIR)$(sbindir)/$(SP_HELPER)
	@$(call info,INSTALL,$(SP_UTIL)) ; $(INSTALL_PROG) -D $(SP_UTIL) $(DESTDIR)$(sbindir)/$(SP_UTIL)
	@$(call info,INSTALL,$(SP_UTIL).static) ; $(INSTALL_PROG) -D $(SP_UTIL).static $(DESTDIR)$(sbindir)/$(SP_UTIL).static
	@$(call install_script,$(SP_GIRAM))
	@$(call install_script,$(SP_CONV))
	@$(call install_script,$(SP_RESIZE))

uninstall:
	@$(call info,UNINSTALL,$(SP_HELPER)) ; rm $(sbindir)/$(SP_HELPER)
	@$(call info,UNINSTALL,$(SP_UTIL)) ; rm $(sbindir)/$(SP_UTIL)
	@$(call info,UNINSTALL,$(SP_UTIL).static) ; rm $(sbindir)/$(SP_UTIL).static
	@$(call info,UNINSTALL,$(SP_GIRAM)) ; rm $(usrbindir)/$(SP_GIRAM)
	@$(call info,UNINSTALL,$(SP_CONV)) ; rm $(usrbindir)/$(SP_CONV)
	@$(call info,UNINSTALL,$(SP_RESIZE)) ; rm $(usrbindir)/$(SP_RESIZE)
