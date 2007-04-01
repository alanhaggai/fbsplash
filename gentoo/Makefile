# Makefile for the splash baselayout plugin.
#

CFLAGS += -fPIC -g -Wall

splash.so: splash.o ../core/libsplash.o
	$(CC) -fPIC -shared -Wl,-soname,$@ -o $@ -leinfo $+

test: test.o
	$(CC) $+ -o $@ -leinfo -lrc -ldl

install:
	install -D splash.so $(DESTDIR)/lib/rcscripts/plugins/splash.so

clean:
	rm -rf *.o *.so

