include ../common.mk

# Don't pass command line variables when calling make.
# This saves us trouble when building against klibc.
MAKEOVERRIDES=

EXTRA_DIST = klibc_compat.h

PNGDEFS = -DPNG_NO_WRITE_TIME -DPNG_NO_FLOATING_POINT_SUPPORTED -DPNG_NO_WRITE_SUPPORTED -DPNG_NO_READ_iTXt \
		-DPNG_LEGACY_SUPPORTED -DPNG_NO_PROGRESSIVE_READ -DPNG_NO_MNG_FEATURES -DPNG_NO_CONSOLE_IO \
		-DPNG_NO_ERROR_NUMBERS -DPNG_READ_GRAY_TO_RGB_SUPPORTED -DPNG_READ_16_TO_8_SUPPORTED \
		-DPNG_READ_STRIP_ALPHA_SUPPORTED -DPNG_NO_INFO_IMAGE -DPNG_NO_READ_TRANSFORMS -DPNG_NO_READ_ANCILLARY_CHUNKS \
		-DPNG_NO_MMX_CODE

# Flags for the kernel utilities
K_CFLAGS = -Os -w -ffunction-sections -fdata-sections \
			-DWITH_ERRLIST -DTARGET_KERNEL -DTT_CONFIG_OPTION_BYTECODE_INTERPRETER

mostlyclean-local: \
		mostlyclean-local-libfreetype \
		mostlyclean-local-libjpeg \
		mostlyclean-local-libpng \
		mostlyclean-local-libz

noinst_LIBRARIES =

if CONFIG_HELPER
noinst_LIBRARIES += libjpeg.a
if CONFIG_PNG
noinst_LIBRARIES += libpng.a libz.a
endif
if CONFIG_TTF_KERNEL
noinst_LIBRARIES += libfreetype.a
endif
endif

libjpeg_a_SOURCES =
libpng_a_SOURCES =
libz_a_SOURCES =
libfreetype_a_SOURCES =

libfreetype.a: klibc_compat.h
	$(Q)$(MKDIR_P) libfreetype
	$(Q)cd libfreetype ; \
	if test ! -e ./config.mk ; then \
		$(call infmsg,CONF,libfreetype.a) ; \
		CFLAGS="$(K_CFLAGS) -include $(abs_top_srcdir)/libs/klibc_compat.h" \
		CC="$(KLCC)" CPPFLAGS="-I$(LIBZ_SOURCE)" \
		$(LIBFREETYPE2_SOURCE)/configure --host=$(host) --build=$(build_triplet) --disable-shared $(OUTPUT) ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CFLAGS="$(K_CFLAGS)" library ; then \
		$(call infmsg,MAKE,libfreetype.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CFLAGS="-c $(K_CFLAGS)" CCexe="$(BUILD_CC)" library $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f libfreetype/.libs/libfreetype.a $@

libjpeg.a:
	$(Q)$(MKDIR_P) libjpeg
	$(Q)cd libjpeg ; \
	if test ! -e ./Makefile ; then \
		$(call infmsg,CONF,libjpeg.a) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS) -I$(abs_top_builddir)/libs/libjpeg" $(LIBJPEG_SOURCE)/configure \
		--host=$(host) --build=$(build_triplet) $(OUTPUT) ; \
		echo "#define NO_GETENV 1" >> jconfig.h ; \
		$(SED) -i "s#CFLAGS=.*#CFLAGS=$(K_CFLAGS) -I$(abs_top_builddir)/libs/libjpeg#" Makefile ; \
		$(SED) -i "s:#define JMESSAGE.*::" $(LIBJPEG_SOURCE)/jerror.c ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CC="$(KLCC)" libjpeg.a ; then  \
		$(call infmsg,MAKE,libjpeg.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CC="$(KLCC)" libjpeg.a $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f libjpeg/libjpeg.a $@

libpng.a: libz.a
	$(Q)cd $(LIBPNG_SOURCE) ; \
	if test ! -e ./Makefile ; then \
		$(call infmsg,CONF,libpng.a) ; \
		cp scripts/makefile.linux Makefile $(OUTPUT); \
		$(SED) -i -e '/^CFLAGS/ { N ; s#^CFLAGS=.*#CFLAGS=$(K_CFLAGS) $(PNGDEFS)# ; P ; D }' \
		       -e 's#^ZLIBINC=.*#ZLIBINC=$(LIBZ_SOURCE)#' \
		       -e 's#^ZLIBLIB=.*#ZLIBLIB=$(LIBZ_SOURCE)#' Makefile ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CC="$(KLCC)" AR_RC="$(AR) rc" RANLIB="$(RANLIB)" libpng.a ; then \
		$(call infmsg,MAKE,libpng.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CC="$(KLCC)" AR_RC="$(AR) rc" RANLIB="$(RANLIB)" libpng.a $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f $(LIBPNG_SOURCE)/libpng.a $@

libz.a:
	$(Q)cd $(LIBZ_SOURCE) ; \
	if test ! -e ./Makefile ; then \
		$(call infmsg,CONF,libz.a) ; \
		CC=$(KLCC) CFLAGS="$(K_CFLAGS)" \
		./configure $(OUTPUT); \
		$(SED) -i 's#^CFLAGS=\(.*\)#CFLAGS=\1 $(K_CFLAGS)#' Makefile ; \
	fi ; \
	if ! $(MAKE) --question $(AM_MAKEFLAGS) CC="$(KLCC)" AR="$(AR) rc" RANLIB="$(RANLIB)" libz.a ; then \
		$(call infmsg,MAKE,zlib.a) ; \
	fi ; \
	$(MAKE) $(AM_MAKEFLAGS) CC="$(KLCC)" AR="$(AR) rc" RANLIB="$(RANLIB)" libz.a $(OUTPUT)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)cp -f  $(LIBZ_SOURCE)/libz.a $@

mostlyclean-local-libfreetype:
	$(Q)$(MKDIR_P) libfreetype
	$(Q)cd libfreetype ; \
	if test -e ./config.mk ; then \
		$(call infmsg,CLEAN,libfreetype.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f config.mk ; \
	fi
	$(Q)rm -fr libfreetype
	$(Q)rm -f  libfreetype.a

mostlyclean-local-libjpeg:
	$(Q)$(MKDIR_P) libjpeg
	$(Q)cd libjpeg ; \
	if test -e Makefile ; then \
		$(call infmsg,CLEAN,libjpeg.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f Makefile ; \
	fi
	$(Q)rm -fr libjpeg
	$(Q)rm -f  libjpeg.a

mostlyclean-local-libpng:
	$(Q)cd $(LIBPNG_SOURCE) ; \
	if test -e Makefile ; then \
		$(call infmsg,CLEAN,libpng.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f Makefile ; \
	fi
	$(Q)rm -f  libpng.a

mostlyclean-local-libz:
	$(Q)cd $(LIBZ_SOURCE) ; \
	if test -e Makefile ; then \
		$(call infmsg,CLEAN,libz.a) ; \
		$(MAKE) $(AM_MAKEFLAGS) clean $(OUTPUT) ; \
		rm -f Makefile ; \
	fi
	$(Q)rm -f  libz.a

