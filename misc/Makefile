SUBDIRS = fbtruetype kbd
LIB		?= lib

all:	fbres usleep

fbres:	
	$(CC) $(CFLAGS) $@.c -o $@
	strip $@

usleep:	
	$(CC) $(CFLAGS) $@.c -o $@
	strip $@

%:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

clean:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	@rm -f *.o fbres usleep

install:
	install -D fbres $(DESTDIR)/$(LIB)/splash/bin/fbres
	install -D usleep $(DESTDIR)/$(LIB)/splash/bin/usleep
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

