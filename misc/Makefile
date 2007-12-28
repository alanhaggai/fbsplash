SUBDIRS = fbtruetype kbd
LIB		?= lib
STRIP	= strip

all:	fbres

fbres:
	$(CC) $(CFLAGS) $(LDFLAGS) $@.c -o $@
	$(STRIP) $@

%:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

clean:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	@rm -f *.o fbres

install:
	install -D fbres $(DESTDIR)/$(LIB)/splash/bin/fbres
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

