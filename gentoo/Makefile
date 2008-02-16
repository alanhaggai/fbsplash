# Makefile for the splash baselayout plugin.
#

LIB		?= lib
CFLAGS 	+= -g -Wall
LDFLAGS += -L../core/objs/.libs -L../core/.libs

INCLUDES = -I../core/objs -I../core -I../core/src -I../core/objs/src

splash.so: splash.o
	$(CC) $(LDFLAGS) -fPIC -shared -Wl,-soname,$@ $+ -o $@ -leinfo -lrc -lfbsplash

test: test.o
	$(CC) $+ -o $@ -leinfo -lrc -ldl

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c -o $@ $<

install:
	install -D splash.so $(DESTDIR)/$(LIB)/rc/plugins/splash.so

clean:
	rm -rf *.o *.so

