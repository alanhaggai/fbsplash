# makefile for libpng.a and libpng12.so on Linux ELF with gcc
# Copyright (C) 1998, 1999, 2002 Greg Roelofs and Glenn Randers-Pehrson
# Copyright (C) 1996, 1997 Andreas Dilger
# For conditions of distribution and use, see copyright notice in png.h

LIBNAME = libpng12
PNGMAJ = 0
PNGMIN = 1.2.8
PNGVER = $(PNGMAJ).$(PNGMIN)

CC=gcc

# where "make install" puts libpng12.a, libpng12.so*,
# libpng12/png.h and libpng12/pngconf.h
# Prefix must be a full pathname.
prefix=/usr/local

# Where the zlib library and include files are located.
#ZLIBLIB=/usr/local/lib
#ZLIBINC=/usr/local/include
ZLIBLIB=libs/zlib-1.2.1
ZLIBINC=libs/zlib-1.2.1

ALIGN=
# for i386:
#ALIGN=-malign-loops=2 -malign-functions=2

WARNMORE=-Wwrite-strings -Wpointer-arith -Wshadow \
	-Wmissing-declarations -Wtraditional -Wcast-align \
	-Wstrict-prototypes -Wmissing-prototypes #-Wconversion

# for pgcc version 2.95.1, -O3 is buggy; don't use it.

CFLAGS=-w -mregparm=3 -DREGPARM=3 -march=i386 -Os -g -falign-functions=0 -falign-jumps=0 -falign-loops=0  -nostdinc -iwithprefix include -I/devel/fbdev/splashutils/utils/libs/klibc-0.179/klibc/../ -I/devel/fbdev/splashutils/utils/libs/klibc-0.179/klibc/../include/arch/i386 -I/devel/fbdev/splashutils/utils/libs/klibc-0.179/klibc/../include/bits32 -D__KLIBC__ -DBITSIZE=32 -I/devel/fbdev/splashutils/utils/libs/klibc-0.179/klibc/../include -I/devel/fbdev/splashutils/utils/linux/include  -DWITH_ERRLIST -I/devel/fbdev/splashutils/utils/libs/zlib-1.2.1 -I/usr/include -ffunction-sections -fdata-sections -DTARGET_KERNEL -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt -DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO -DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED -DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS

LDFLAGS=-L. -Wl,-rpath,. -L$(ZLIBLIB) -Wl,-rpath,$(ZLIBLIB) -lpng12 -lz -lm
LDFLAGS_A=-L$(ZLIBLIB) -Wl,-rpath,$(ZLIBLIB) libpng.a -lz -lm

RANLIB=ranlib
#RANLIB=echo

INCPATH=$(prefix)/include
LIBPATH=$(prefix)/lib
MANPATH=$(prefix)/man
BINPATH=$(prefix)/bin

# override DESTDIR= on the make install command line to easily support
# installing into a temporary location.  Example:
#
#    make install DESTDIR=/tmp/build/libpng
#
# If you're going to install into a temporary location
# via DESTDIR, $(DESTDIR)$(prefix) must already exist before
# you execute make install.
DESTDIR=

DB=$(DESTDIR)$(BINPATH)
DI=$(DESTDIR)$(INCPATH)
DL=$(DESTDIR)$(LIBPATH)
DM=$(DESTDIR)$(MANPATH)

OBJS = png.o pngset.o pngget.o pngrutil.o pngtrans.o pngwutil.o \
	pngread.o pngrio.o pngwio.o pngwrite.o pngrtran.o \
	pngwtran.o pngmem.o pngerror.o pngpread.o

OBJSDLL = $(OBJS:.o=.pic.o)

.SUFFIXES:      .c .o .pic.o

.c.pic.o:
	$(CC) -c $(CFLAGS) -fPIC -o $@ $*.c

all: libpng.a $(LIBNAME).so pngtest pngtest-static libpng.pc libpng-config

libpng.a: $(OBJS)
	ar rc $@ $(OBJS)
	$(RANLIB) $@

libpng.pc:
	cat scripts/libpng.pc.in | sed -e s\!@PREFIX@!$(prefix)! > libpng.pc

libpng-config:
	( cat scripts/libpng-config-head.in; \
	echo prefix=\"$(prefix)\"; \
	echo I_opts=\"-I$(INCPATH)/$(LIBNAME)\"; \
	echo L_opts=\"-L$(LIBPATH)\"; \
	echo R_opts=\"-Wl,-rpath,$(LIBPATH)\"; \
	echo libs=\"-lpng12 -lz -lm\"; \
	cat scripts/libpng-config-body.in ) > libpng-config
	chmod +x libpng-config

$(LIBNAME).so: $(LIBNAME).so.$(PNGMAJ)
	ln -sf $(LIBNAME).so.$(PNGMAJ) $(LIBNAME).so

$(LIBNAME).so.$(PNGMAJ): $(LIBNAME).so.$(PNGVER)
	ln -sf $(LIBNAME).so.$(PNGVER) $(LIBNAME).so.$(PNGMAJ)

$(LIBNAME).so.$(PNGVER): $(OBJSDLL)
	$(CC) -shared -Wl,-soname,$(LIBNAME).so.$(PNGMAJ) \
	-o $(LIBNAME).so.$(PNGVER) \
	$(OBJSDLL)

libpng.so.3.$(PNGMIN): $(OBJSDLL)
	$(CC) -shared -Wl,-soname,libpng.so.3 \
	-o libpng.so.3.$(PNGMIN) \
	$(OBJSDLL)

pngtest: pngtest.o $(LIBNAME).so
	$(CC) -o pngtest $(CFLAGS) pngtest.o $(LDFLAGS)

pngtest-static: pngtest.o libpng.a
	$(CC) -o pngtest-static $(CFLAGS) pngtest.o $(LDFLAGS_A)

test: pngtest pngtest-static
	@echo ""
	@echo "   Running pngtest dynamically linked with $(LIBNAME).so:"
	@echo ""
	./pngtest
	@echo ""
	@echo "   Running pngtest statically linked with libpng.a:"
	@echo ""
	./pngtest-static

install-headers: png.h pngconf.h
	-@if [ ! -d $(DI) ]; then mkdir $(DI); fi
	-@if [ ! -d $(DI)/$(LIBNAME) ]; then mkdir $(DI)/$(LIBNAME); fi
	cp png.h pngconf.h $(DI)/$(LIBNAME)
	chmod 644 $(DI)/$(LIBNAME)/png.h $(DI)/$(LIBNAME)/pngconf.h
	-@/bin/rm -f $(DI)/png.h $(DI)/pngconf.h
	-@/bin/rm -f $(DI)/libpng
	(cd $(DI); ln -sf $(LIBNAME) libpng; ln -sf $(LIBNAME)/* .)

install-static: install-headers libpng.a
	-@if [ ! -d $(DL) ]; then mkdir $(DL); fi
	cp libpng.a $(DL)/$(LIBNAME).a
	chmod 644 $(DL)/$(LIBNAME).a
	-@/bin/rm -f $(DL)/libpng.a
	(cd $(DL); ln -sf $(LIBNAME).a libpng.a)

install-shared: install-headers $(LIBNAME).so.$(PNGVER) libpng.pc \
	libpng.so.3.$(PNGMIN)
	-@if [ ! -d $(DL) ]; then mkdir $(DL); fi
	-@/bin/rm -f $(DL)/$(LIBNAME).so.$(PNGVER)* $(DL)/$(LIBNAME).so
	-@/bin/rm -f $(DL)/$(LIBNAME).so.$(PNGMAJ)
	-@/bin/rm -f $(DL)/libpng.so
	-@/bin/rm -f $(DL)/libpng.so.3
	-@/bin/rm -f $(DL)/libpng.so.3.$(PNGMIN)*
	cp $(LIBNAME).so.$(PNGVER) $(DL)
	cp libpng.so.3.$(PNGMIN) $(DL)
	chmod 755 $(DL)/$(LIBNAME).so.$(PNGVER)
	chmod 755 $(DL)/libpng.so.3.$(PNGMIN)
	(cd $(DL); \
	ln -sf libpng.so.3.$(PNGMIN) libpng.so.3; \
	ln -sf libpng.so.3 libpng.so; \
	ln -sf $(LIBNAME).so.$(PNGVER) $(LIBNAME).so.$(PNGMAJ); \
	ln -sf $(LIBNAME).so.$(PNGMAJ) $(LIBNAME).so)
	-@if [ ! -d $(DL)/pkgconfig ]; then mkdir $(DL)/pkgconfig; fi
	-@/bin/rm -f $(DL)/pkgconfig/$(LIBNAME).pc
	-@/bin/rm -f $(DL)/pkgconfig/libpng.pc
	cp libpng.pc $(DL)/pkgconfig/$(LIBNAME).pc
	chmod 644 $(DL)/pkgconfig/$(LIBNAME).pc
	(cd $(DL)/pkgconfig; ln -sf $(LIBNAME).pc libpng.pc)

install-man: libpng.3 libpngpf.3 png.5
	-@if [ ! -d $(DM) ]; then mkdir $(DM); fi
	-@if [ ! -d $(DM)/man3 ]; then mkdir $(DM)/man3; fi
	-@/bin/rm -f $(DM)/man3/libpng.3
	-@/bin/rm -f $(DM)/man3/libpngpf.3
	cp libpng.3 $(DM)/man3
	cp libpngpf.3 $(DM)/man3
	-@if [ ! -d $(DM)/man5 ]; then mkdir $(DM)/man5; fi
	-@/bin/rm -f $(DM)/man5/png.5
	cp png.5 $(DM)/man5

install-config: libpng-config
	-@if [ ! -d $(DB) ]; then mkdir $(DB); fi
	-@/bin/rm -f $(DB)/libpng-config
	-@/bin/rm -f $(DB)/$(LIBNAME)-config
	cp libpng-config $(DB)/$(LIBNAME)-config
	chmod 755 $(DB)/$(LIBNAME)-config
	(cd $(DB); ln -sf $(LIBNAME)-config libpng-config)

install: install-static install-shared install-man install-config

# If you installed in $(DESTDIR), test-installed won't work until you
# move the library to its final location.  Use test-dd to test it
# before then.

test-dd:
	echo
	echo Testing installed dynamic shared library in $(DL).
	$(CC) -I$(DI) -I$(ZLIBINC) \
	   `$(BINPATH)/libpng12-config --cflags` pngtest.c \
	   -L$(DL) -L$(ZLIBLIB) -W1, -rpath,$(DL) -Wl,-rpath,$(ZLIBLIB) \
	   -o pngtestd `$(BINPATH)/libpng12-config --ldflags`
	./pngtestd pngtest.png

test-installed:
	$(CC) -I$(ZLIBINC) \
	   `$(BINPATH)/libpng12-config --cflags` pngtest.c \
	   -L$(ZLIBLIB) -Wl,-rpath,$(ZLIBLIB) \
	   -o pngtesti `$(BINPATH)/libpng12-config --ldflags`
	./pngtesti pngtest.png

clean:
	/bin/rm -f *.o libpng.a pngtest pngout.png libpng-config \
	$(LIBNAME).so $(LIBNAME).so.$(PNGMAJ)* pngtest-static pngtesti \
	libpng.so.3.$(PNGMIN) \
	libpng.pc

DOCS = ANNOUNCE CHANGES INSTALL KNOWNBUG LICENSE README TODO Y2KINFO
writelock:
	chmod a-w *.[ch35] $(DOCS) scripts/*

# DO NOT DELETE THIS LINE -- make depend depends on it.

png.o png.pic.o: png.h pngconf.h
pngerror.o pngerror.pic.o: png.h pngconf.h
pngrio.o pngrio.pic.o: png.h pngconf.h
pngwio.o pngwio.pic.o: png.h pngconf.h
pngmem.o pngmem.pic.o: png.h pngconf.h
pngset.o pngset.pic.o: png.h pngconf.h
pngget.o pngget.pic.o: png.h pngconf.h
pngread.o pngread.pic.o: png.h pngconf.h
pngrtran.o pngrtran.pic.o: png.h pngconf.h
pngrutil.o pngrutil.pic.o: png.h pngconf.h
pngtrans.o pngtrans.pic.o: png.h pngconf.h
pngwrite.o pngwrite.pic.o: png.h pngconf.h
pngwtran.o pngwtran.pic.o: png.h pngconf.h
pngwutil.o pngwutil.pic.o: png.h pngconf.h
pngpread.o pngpread.pic.o: png.h pngconf.h

pngtest.o: png.h pngconf.h
