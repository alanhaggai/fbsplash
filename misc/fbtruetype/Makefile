# Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

ARCH     = $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/)
SHELL    = /bin/sh
CC       = gcc
STRIP	 = strip

# Triggering lib64 path from spec file.
PREFIX   ?= /usr
LIB      ?= lib

LIBDIR   = $(PREFIX)/$(LIB)

PROGRAMS = fbtruetype fbtruetype.static
OBJECTS  = fbtruetype.o messages.o console.o ttf.o luxisri.o

LDFLAGS  =  -L$(LIBDIR)
LIBSS    = $(LIBDIR)/libfreetype.a $(LIBDIR)/libz.a -lm
LIBSD    = -lfreetype -lm

CFLAGS   = -O2 -Wall -I/usr/include/freetype2
COMPILE  = $(CC) $(CFLAGS)
LINKS    = $(CC) $(CFLAGS) $(LDFLAGS) -static -o $@
LINKD    = $(CC) $(CFLAGS) $(LDFLAGS) -o $@

INSTALL_PROG = install

all: $(PROGRAMS) strip

.SUFFIXES:
.SUFFIXES: .S .c .o .s

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean:
	-rm -f *.o core
clean:	mostlyclean
	-rm -f fbtruetype fbtruetype.static

fbtruetype: $(OBJECTS) 
	@rm -f fbtruetype
	$(LINKD) $(LDFLAGS) $(OBJECTS) $(LIBSD)
fbtruetype.static: $(OBJECTS)
	@rm -f fbtruetype.static
	$(LINKS) $(LDFLAGS) $(OBJECTS) $(LIBSS)

install:
	$(INSTALL_PROG) -D fbtruetype $(DESTDIR)/sbin/fbtruetype
	$(INSTALL_PROG) -D fbtruetype.static $(DESTDIR)/sbin/fbtruetype.static

strip: $(PROGRAMS)
	$(STRIP) -s $(PROGRAMS)

.PHONY: mostlyclean clean fbtruetype fbtruetype.static all strip
