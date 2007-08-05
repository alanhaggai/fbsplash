# Makefile for the splash baselayout plugin.
#

LIB		?= lib
CFLAGS 	+= -g -Wall
LDFLAGS += -L../core/objs

INCLUDES = -I../core

splash.so: splash.o
	$(CC) $(LDFLAGS) -fPIC -shared -Wl,-soname,$@ $+ -o $@ -leinfo -lsplash

test: test.o
	$(CC) $+ -o $@ -leinfo -lrc -ldl

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c -o $@ $<

install:
	install -D splash.so $(DESTDIR)/$(LIB)/rcscripts/plugins/splash.so

clean:
	rm -rf *.o *.so

