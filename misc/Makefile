SUBDIRS = fbtruetype kbd

all:	fbres

fbres:	
	$(CC) -O2 $@.c -o $@
	strip $@

%:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

clean:
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f *.o
	rm -f fbres

install:
	install -D fbres $(DESTDIR)/lib/splash/bin/fbres
	@set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done

