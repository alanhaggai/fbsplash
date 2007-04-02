# Makefile for the splash baselayout plugin.
#

CFLAGS 	+= -fPIC -g -Wall -I../core
LDFLAGS += -L../core/objs

splash.so: splash.o
	$(CC) $(LDFLAGS) -fPIC -shared -Wl,-soname,$@ -o $@ -leinfo -lsplash $+

test: test.o
	$(CC) $+ -o $@ -leinfo -lrc -ldl

install:
	install -D splash.so $(DESTDIR)/lib/rcscripts/plugins/splash.so

clean:
	rm -rf *.o *.so

