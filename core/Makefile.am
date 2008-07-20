SUBDIRS = libs src docs misc scripts
ACLOCAL_AMFLAGS = -I m4

include common.mk

EXTRA_DIST = $(LIBFREETYPE2_SOURCE_INTERNAL) \
			 $(LIBJPEG_SOURCE_INTERNAL) \
			 $(LIBPNG_SOURCE_INTERNAL) \
			 $(LIBZ_SOURCE_INTERNAL) \
			 Doxyfile

doc:
	doxygen

