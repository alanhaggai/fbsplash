LIB ?= lib

all: fgconsole

fgconsole: fgconsole.o getfd.o
	$(CC) $(CFLAGS) $(LDFLAGS) $+ -o $@

clean:
	rm -f *.o fgconsole

install:
	install -D fgconsole $(DESTDIR)/$(LIB)/splash/bin/fgconsole

