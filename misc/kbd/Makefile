all: fgconsole

fgconsole: fgconsole.o getfd.o
	$(CC) $(CFLAGS) $(LDFLAGS) $+ -o $@

install:
	install -D splash_fgcon $(DESTDIR)/lib/splash/bin/fgconsole

