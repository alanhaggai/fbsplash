all: fgconsole

fgconsole: fgconsole.o getfd.o
	$(CC) $(CFLAGS) $(LDFLAGS) $+ -o $@

install:
	install -D fgconsole $(DESTDIR)/lib/splash/bin/fgconsole

