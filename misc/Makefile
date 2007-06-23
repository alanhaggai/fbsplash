SUBDIRS = fbtruetype kbd
LIB		?= lib
STRIP	= strip

all:	fbres usleep

fbres:
	$(CC) $(CFLAGS) $(LDFLAGS) $@.c -o $@
	$(STRIP) $@

usleep:
	$(CC) $(CFLAGS) $(LDFLAGS) $@.c -o $@
	$(STRIP) $@

%:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

clean:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	@rm -f *.o fbres usleep

install:
	install -D fbres $(DESTDIR)/$(LIB)/splash/bin/fbres
	install -D usleep $(DESTDIR)/$(LIB)/splash/bin/usleep
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

