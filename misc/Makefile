SUBDIRS = fbtruetype kbd

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
	rm -f *.o
	rm -f fbres
	rm -r usleep

install:
	install -D fbres $(DESTDIR)/lib/splash/bin/fbres
	install -D usleep $(DESTDIR)/lib/splash/bin/usleep
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

