################################################################################
# autoconf file for generating the splashutils' configure file.
#
# Copyright (c) 2006-2007, Michal Januszewski <spock@gentoo.org>
# Copyright (c) 2007, Paul Bender <pebender@gmail.com>
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
################################################################################

AC_INIT([splashutils], [1.5.4])

AC_SUBST([libfbsplash_version], [1:0:0])
AC_SUBST([libfbsplashrender_version], [1:0:0])

AM_INIT_AUTOMAKE([1.10 -Wno-portability])

AC_PREREQ(2.59)
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_SED

AS_IF(
  [test "x${BUILD_CC}" = "x" && test "x${build_alias}" != "x"],
  [AC_CHECK_PROG([BUILD_CC], [${build_alias}-gcc], [${build_alias}-gcc])]
)
AS_IF(
  [test "x${BUILD_CC}" = "x"],
  [AC_CHECK_PROG([BUILD_CC], [gcc], [gcc])]
)
AS_IF(
  [test "x${BUILD_CC}" = "x"],
  [AC_CHECK_PROG([BUILD_CC], [cc], [cc])]
)
AS_IF(
  [test "x${BUILD_CC}" = "x"],
  [BUILD_CC="\$(CC)"]
)

AC_PREFIX_DEFAULT([/usr])

AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--enable-debug], [do not strip files]),
  [
    AS_CASE(["${enableval}"],
      [yes], [config_debug="yes"],
      [no],  [config_debug="no"],
             [AC_MSG_ERROR([bad value '${enableval}' for --enable-debug])]
    )
  ],
  [config_debug="no"]
)
AM_CONDITIONAL([CONFIG_DEBUG], [test "x${config_debug}" = "xyes"])

AC_ARG_ENABLE([klibc-shared],
  AS_HELP_STRING([--enable-klibc-shared], [link to shared klibc]),
  [
    AS_CASE(["${enableval}"],
        [yes], [config_klibc_shared="yes"],
        [no],  [config_klibc_shared="no"],
               [AC_MSG_ERROR([bad value '${enableval}' for --enable-klibc-shared])]
    )
  ],
  [config_klibc_shared="no"]
)
AM_CONDITIONAL([CONFIG_KLIBC_SHARED], [test "x${config_klibc_shared}" = "xyes"])

AC_ARG_ENABLE([deprecated],
  AC_HELP_STRING([--enable-deprecated], [include support for deprecated features]),
  [
    AS_CASE(["${enableval}"],
      [yes], [config_deprecated="yes"],
      [no],  [config_deprecated="no"],
             [AC_MSG_ERROR([bad value '${enableval}' for --enable-deprecated])]
    )
  ],
  [config_deprecated="no"]
)
AM_CONDITIONAL([CONFIG_DEPRECATED], [test "x${config_deprecated}" = "xyes"])
AS_IF(
  [test "x${config_deprecated}" = "xyes"],
  [AC_DEFINE([CONFIG_DEPRECATED], [1], [Define to 1 to include support for deprecated features.])]
)

AC_ARG_ENABLE([fbcondecor],
  AC_HELP_STRING([--disable-fbcondecor], [exclude support for fbcondecor]),
  [
    AS_CASE(["${enableval}"],
       [yes], [config_fbcondecor="yes"],
       [no],  [config_fbcondecor="no"],
              [AC_MSG_ERROR([bad value '${enableval}' for --enable-fbcondecor])]
    )
  ],
  [config_fbcondecor="yes"]
)
AM_CONDITIONAL([CONFIG_FBCON_DECOR], [test "x${config_fbcondecor}" = "xyes"])
AS_IF(
  [test "x${config_fbcondecor}" = "xyes"],
  [AC_DEFINE([CONFIG_FBCON_DECOR], [1], [Define to 1 to include support for fbcondecor (previously called fbsplash).])]
)

AC_ARG_ENABLE([helper],
  AC_HELP_STRING([--disable-helper], [do not build kernel helper]),
  [
    AS_CASE(["${enableval}"],
      [yes], [config_helper="yes"],
      [no],  [config_helper="no"],
             [AC_MSG_ERROR([bad value '${enableval}' for --enable-helper])]
    )
  ],
  [config_helper="yes"]
)
AM_CONDITIONAL([CONFIG_HELPER], [test "x${config_helper}" = "xyes"])
AS_IF(
  [test "x${config_helper}" = "xyes"],
  [AC_DEFINE([CONFIG_HELPER], [1], [Define to 1 to build kernel helper.])]
)

AC_ARG_ENABLE([misc],
  AC_HELP_STRING([--enable-misc], [build misc programs]),
  [
    AS_CASE(["${enableval}"],
      [yes], [config_misc="yes"],
      [no],  [config_misc="no"],
             [AC_MSG_ERROR([bad value '${enableval}' for --enable-misc])]
    )
  ],
  [config_misc="no"]
)
AM_CONDITIONAL([CONFIG_MISC], [test "x${config_misc}" = "xyes"])
AS_IF(
  [test "x${config_misc}" = "xyes"],
  [AC_DEFINE([CONFIG_MISC], [1], [Define to 1 to build misc programs.])]
)

AC_ARG_WITH([gpm],
  AC_HELP_STRING([--without-gpm], [exclude support for GPM]),
  [
    AS_CASE(["${withval}"],
      [yes], [config_gpm="yes"],
      [no],  [config_gpm="no"],
             [AC_MSG_ERROR([bad value '${withval}' for --with-gpm])]
    )
  ],
  [config_gpm="yes"]
)
AM_CONDITIONAL([CONFIG_GPM], [test "x${config_gpm}" = "xyes"])
AS_IF(
  [test "x${config_gpm}" = "xyes"],
  [AC_DEFINE([CONFIG_GPM], [1], [Define to 1 to include support for GPM.])]
)

AC_ARG_WITH([mng],
  AC_HELP_STRING([--with-mng], [include  support for MNG animations]),
  [
    AS_CASE(["${withval}"],
      [yes], [config_mng="yes"],
      [no],  [config_mng="no"],
             [AC_MSG_ERROR([bad value '${withval}' for --with-mng])]
    )
  ],
  [config_mng="no"]
)
AM_CONDITIONAL([CONFIG_MNG], [test "x${config_mng}" = "xyes"])
AS_IF(
  [test "x${config_mng}" = "xyes"],
  [AC_DEFINE([CONFIG_MNG], [1], [Define to 1 to include support for MNG animations.])]
)

AC_ARG_WITH([png],
  AC_HELP_STRING([--without-png], [exclude support for PNG images]),
  [
    AS_CASE(["${withval}"],
      [yes], [config_png="yes"],
      [no],  [config_png="no"],
             [AC_MSG_ERROR([bad value '${withval}' for --with-png])]
    )
  ],
  [config_png="yes"]
)
AM_CONDITIONAL([CONFIG_PNG], [test "x${config_png}" = "xyes"])
AS_IF(
  [test "x${config_png}" = "xyes"],
  [AC_DEFINE([CONFIG_PNG], [1], [Define to 1 to include support for PNG images.])]
)

AC_ARG_WITH([ttf],
  AC_HELP_STRING([--without-ttf], [exclude support for truetype fonts]),
  [
    AS_CASE(["${withval}"],
      [yes], [config_ttf="yes"],
      [no],  [config_ttf="no"],
             [AC_MSG_ERROR([bad value ${withval} for --with-ttf])]
    )
  ],
  [config_ttf="yes"]
)
AM_CONDITIONAL([CONFIG_TTF], [test "x${config_ttf}" = "xyes"])
AS_IF(
  [test "x${config_ttf}" = "xyes"],
  [AC_DEFINE([CONFIG_TTF], [1], [Define to 1 to include support for truetype fonts.])]
)

AC_ARG_WITH([ttf-kernel],
  AC_HELP_STRING([--without-ttf-kernel], [exclude support for truetype fonts in kernel helper]),
  [
    AS_CASE(["${withval}"],
      [yes], [config_ttf_kernel="yes"],
      [no],  [config_ttf_kernel="no"],
             [AC_MSG_ERROR([bad value ${withval} for --with-ttf-kernel])]
    )
  ],
  [config_ttf_kernel="yes"]
)
AM_CONDITIONAL([CONFIG_TTF_KERNEL], [test "x${config_ttf_kernel}" = "xyes"])
AS_IF(
  [test "x${config_ttf_kernel}" = "xyes"],
  [AC_DEFINE([CONFIG_TTF_KERNEL], [1], [Define to 1 to include support for truetype fonts in kernel helper.])]
)

AC_ARG_WITH([klibc-compiler],
  AS_HELP_STRING([--with-klibc-compiler], [compiler to use when building against klibc @<:@auto@:>@]),
  [KLCC="${withval}"],
  [
    AS_IF(
      [test "x${host_alias}" != "x${build_alias}"],
      [AC_CHECK_PROG([KLCC], [${host_alias}-klcc], [${host_alias}-klcc])],
      [AC_CHECK_PROG([KLCC], [klcc], [klcc])]
    )
    AS_IF(
      [test "x${KLCC}" = "x"],
      [AC_MSG_WARN([could not determine compiler to use when building against klibc.])]
    )
  ]
)
AC_SUBST([KLCC])

AC_ARG_WITH([freetype2-src],
  AS_HELP_STRING([--with-freetype2-src=DIR], [freetype2 source to use when linking with klibc]),
  [
	AS_IF(
	  [test "`echo ${withval} | sed -e 's,^/.*,,'`" = "${withval}"],
	  [LIBFREETYPE2_SOURCE="\$(abs_top_srcdir)/${withval}"],
	  [LIBFREETYPE2_SOURCE="${withval}"]
	)
  ],
  [LIBFREETYPE2_SOURCE="\$(abs_top_srcdir)/\$(LIBFREETYPE2_SOURCE_INTERNAL)"]
)
AC_SUBST([LIBFREETYPE2_SOURCE])

AC_ARG_WITH([jpeg-src],
  AS_HELP_STRING([--with-jpeg-src=DIR], [jpeglib source to use when linking with klibc]),
  [
	AS_IF(
	  [test "`echo ${withval} | sed -e 's,^/.*,,'`" = "${withval}"],
	  [LIBJPEG_SOURCE="\$(abs_top_srcdir)/${withval}"],
	  [LIBJPEG_SOURCE="${withval}"]
	)
  ],
  [LIBJPEG_SOURCE="\$(abs_top_srcdir)/\$(LIBJPEG_SOURCE_INTERNAL)"]
)
AC_SUBST([LIBJPEG_SOURCE])

AC_ARG_WITH([lpng-src],
  AS_HELP_STRING([--with-lpng-src=DIR], [libpng source to use when linking with klibc]),
  [
	AS_IF(
	  [test "`echo ${withval} | sed -e 's,^/.*,,'`" = "${withval}"],
	  [LIBPNG_SOURCE="\$(abs_top_srcdir)/${withval}"],
	  [LIBPNG_SOURCE="${withval}"]
	)
  ],
  [LIBPNG_SOURCE="\$(abs_top_srcdir)/\$(LIBPNG_SOURCE_INTERNAL)"]
)
AC_SUBST([LIBPNG_SOURCE])

AC_ARG_WITH([zlib-src],
  AS_HELP_STRING([--with-zlib-src=DIR], [zlib source to use when linking with klibc]),
  [
	AS_IF(
	  [test "`echo ${withval} | sed -e 's,^/.*,,'`" = "${withval}"],
	  [LIBZ_SOURCE="\$(abs_top_srcdir)/${withval}"],
	  [LIBZ_SOURCE="${withval}"]
	)
  ],
  [LIBZ_SOURCE="\$(abs_top_srcdir)/\$(LIBZ_SOURCE_INTERNAL)"]
)
AC_SUBST([LIBZ_SOURCE])

AC_ARG_WITH([essential-prefix],
  AS_HELP_STRING([--with-essential-prefix=ESPREFIX], [install essential files in ESPREFIX @<:@/@:>@]),
  [essential_prefix="$(echo ${withval} | sed -e 's#/$##')"],
  [essential_prefix="/"]
)
AC_SUBST([essential_prefix])

AC_ARG_WITH([essential-bindir],
  AS_HELP_STRING([--with-essential-bindir=DIR], [essential user apps directory @<:@ESPREFIX/bin@:>@]),
  [eexecbindir="${withval}"],
  [eexecbindir="\$(essential_prefix)/bin"]
)
AC_SUBST([eexecbindir])

AC_ARG_WITH([essential-sbindir],
  AS_HELP_STRING([--with-eseential-sbindir=DIR], [essential admin apps directory @<:@ESPREFIX/sbin@:>@]),
  [eexecsbindir="${withval}"],
  [eexecsbindir="\$(essential_prefix)/sbin"]
)
AC_SUBST([eexecsbindir])

AC_ARG_WITH([essential-libdir],
  AS_HELP_STRING([--with-essential-libdir=DIR], [essential libraries directory @<:@ESPREFIX/lib@:>@]),
  [eexeclibdir="${withval}"],
  [eexeclibdir="\$(essential_prefix)/lib"]
)
AC_SUBST([eexeclibdir])

AC_ARG_WITH([themedir],
  AS_HELP_STRING([--with-themedir=DIR], [splash theme directory @<:@ESPREFIX/etc/splash@:>@]),
  [themedir="${withval}"],
  [themedir="\$(essential_prefix)/etc/splash"]
)
AC_SUBST([themedir])

M_CFLFAGS=
M_LIBS=
AC_CHECK_HEADER(
  [math.h],
  [M_CFLAGS="${M_CFLAGS}"],
  [AC_MSG_ERROR(['math.h' header file not found.])]
)
AC_CHECK_LIB(
  [m], [ceilf],
  [M_LIBS="${M_LIBS} -lm"],
  [AC_MSG_ERROR(['libm' library not found.])]
)
AC_SUBST([M_CFLAGS])
AC_SUBST([M_LIBS])

RT_CFLFAGS=
RT_LIBS=
AC_CHECK_HEADER(
  [time.h],
  [RT_CFLAGS="${RT_CFLAGS}"],
  [AC_MSG_ERROR(['time.h' header file was not found.])]
)
AC_CHECK_LIB([rt], [clock_gettime],
  [RT_LIBS="${RT_LIBS} -lrt"],
  [AC_MSG_ERROR(['librt' library was not found.])]
)
AC_SUBST([RT_CFLAGS])
AC_SUBST([RT_LIBS])

PTHREAD_CFLFAGS=
PTHREAD_LIBS=
AC_CHECK_HEADER([pthread.h],
  [PTHREAD_CFLAGS="${PTHREAD_CFLAGS}"],
  [AC_MSG_ERROR(['pthread.h' header file was not found.])]
)
AC_CHECK_LIB([pthread], [pthread_create],
  [PTHREAD_LIBS="${PTHREAD_LIBS} -lpthread"],
  [AC_MSG_ERROR(['libpthread' library file was not found.])]
)
AC_SUBST([PTHREAD_CFLAGS])
AC_SUBST([PTHREAD_LIBS])

PTHREAD_STATIC_CFLFAGS=
PTHREAD_STATIC_LIBS=
AC_CHECK_HEADER(
  [pthread.h],
  [PTHREAD_STATIC_CFLFAGS="${PTHREAD_STATIC_CFLFAGS}"],
  [AC_MSG_ERROR(['pthread.h' header file was not found.])]
)
AC_CHECK_LIB([pthread], [xxpthread_create],
  [PTHREAD_STATIC_LIBS="${PTHREAD_STATIC_LIBS} -lpthread"],
  [
    AC_CHECK_LIB([pthread], [pthread_create],
      [PTHREAD_STATIC_LIBS="${PTHREAD_STATIC_LIBS} -L\$(DESTDIR)\$(libdir)/nptl -lpthread"],
      [
        AC_CHECK_LIB([pthread], [pthread_create],
          [PTHREAD_STATIC_LIBS="${PTHREAD_STATIC_LIBS} -L\$(DESTDIR)\$(elibdir)/nptl -lpthread"],
          [AC_MSG_ERROR([the required 'libpthread' static library file not found.])],
          [-static -L${DESTDIR}${elibdir}/nptl]
        )
      ],
      [-static -L${DESTDIR}${libdir}/nptl]
    )
  ],
  [-static]
)
AC_SUBST([PTHREAD_STATIC_CFLAGS])
AC_SUBST([PTHREAD_STATIC_LIBS])

AC_ARG_VAR([JPEG_CFLAGS], [C compiler flags for JPEG library, overriding autodetection])
AC_ARG_VAR([JPEG_LIBS], [linker flags for JPEG library, overriding autodetection])
AS_IF(
  [test "x${JPEG_CFLAGS}" = "x"],
  [
    AC_CHECK_HEADER([jpeglib.h],
      [JPEG_CFLAGS="${JPEG_CFLAGS}"],
      [AC_MSG_ERROR(['jpeglib.h' header file not found.])]
    )
  ]
)
AS_IF(
  [test "x${JPEG_CFLAGS}" = "no"],
  [JPEG_CFLAGS=""]
)
AS_IF(
  [test "x${JPEG_LIBS}" = "x"],
  [
    AC_CHECK_LIB([jpeg], [jpeg_set_defaults],
      [JPEG_LIBS="${JPEG_LIBS} -ljpeg"],
      [AC_MSG_ERROR([the required 'libjpeg' library file was not found.])]
    )
  ]
)
AS_IF(
  [test "x${JPEG_LIBS}" = "no"],
  [JPEG_LIBS=""]
)
AC_SUBST([JPEG_CFLAGS])
AC_SUBST([JPEG_LIBS])

# As needed, check GPM files and set GPM compiler flags.
AC_ARG_VAR([GPM_CFLAGS], [C compiler flags for GPM library, overriding autodetection])
AC_ARG_VAR([GPM_LIBS],   [linker flags for GPM library, overriding autodetection])
AS_IF(
  [test "x${config_gpm}" = "xyes"],
  [
    AS_IF(
      [test "x${GPM_CFLAGS}" = "x"],
      [
        AC_CHECK_HEADER(
          [gpm.h],
          [GPM_CFLAGS="${GPM_CFLAGS}"],
          [AC_MSG_ERROR(['gpm.h' header file was not found.])]
        )
      ]
    )
    AS_IF(
      [test "x${GPM_CFLAGS}" = "no"],
      [GPM_CFLAGS=""]
    )
    AS_IF(
      [test "x${GPM_LIBS}" = "x"],
      [
        AC_CHECK_LIB(
          [gpm], [Gpm_Open],
          [GPM_LIBS="${GPM_LIBS} -lgpm"],
          [AC_MSG_ERROR(['libgpm' library file was not found.])]
        )
      ]
    )
    AS_IF(
      [test "x${GPM_LIBS}" = "no"],
      [GPM_LIBS=""]
    )
  ]
)
AC_SUBST([GPM_CFLAGS])
AC_SUBST([GPM_LIBS])

AC_ARG_VAR([MNG_CFLAGS], [C compiler flags for MNG library, overriding autodetection])
AC_ARG_VAR([MNG_LIBS],   [linker flags for MNG library, overriding autodetection])
AS_IF(
  [test "x${config_mng}" = "xyes"],
  [
    AS_IF(
      [test "x${MNG_CFLAGS}" = "x"],
      [
        AC_CHECK_HEADER(
          [libmng.h],
          [
            MNG_CFLAGS="${MNG_CFLAGS}"
            MNG_CFLAGS="${MNG_CFLAGS} ${JPEG_CFLAGS}"
            MNG_CFLAGS="${MNG_CFLAGS} ${RT_CFLAGS}"
          ],
          [AC_MSG_ERROR(['libmng.h' header file was not found.])]
        )
      ]
    )
    AS_IF(
      [test "x${MNG_CFLAGS}" = "xno"],
      [MNG_CFLAGS=""]
    )
    AS_IF(
      [test "x${MNG_LIBS}" = "x"],
      [
        AC_CHECK_LIB(
          [mng], [mng_initialize],
          [
            MNG_LIBS="${MNG_LIBS} -lmng"
            MNG_LIBS="${MNG_LIBS} ${JPEG_LIBS}"
            MNG_LIBS="${MNG_LIBS} ${LT_LIBS}"
            AC_CHECK_LIB([z],    [zlibVersion],    [MNG_LIBS="${MNG_LIBS} -lz"   ])
            AC_CHECK_LIB([lcms], [cmsCloseProfile],[MNG_LIBS="${MNG_LIBS} -llcms"])
          ],
          [AC_MSG_ERROR(['libmng' library file was not found.])]
        )
      ]
    )
    AS_IF(
      [test "x${MNG_LIBS}" = "xno"],
      [MNG_LIBS=""]
    )
  ]
)
AC_SUBST([MNG_CFLAGS])
AC_SUBST([MNG_LIBS])

AC_ARG_VAR([PNG_CFLAGS], [C compiler flags for PNG library, overriding autodetection])
AC_ARG_VAR([PNG_LIBS],   [linker flags for PNG library, overriding autodetection])

AS_IF(
  [test "x${config_png}" = "xyes"],
  [
    AS_IF(
      [test "x${LIBPNG_CONFIG}" = "x"],
      [AC_PATH_PROG(LIBPNG_CONFIG, libpng-config)]
    )
    AS_IF(
      [test "x${LIBPNG_CONFIG}" = "xno"],
      [LIBPNG_CONFIG=""]
    )
    AS_IF(
      [test "x${PNG_CFLAGS}" = "x"],
      [
        AS_IF(
          [test "x$LIBPNG_CONFIG" != "x"],
          [PNG_CFLAGS=$(${LIBPNG_CONFIG} --static --cflags)],
          [
            AC_CHECK_HEADER([png.h],
              [PNG_CFLAGS="${PNG_CFLAGS}"],
              [AC_MSG_ERROR(['png.h' header file not found.])]
            )
          ]
        )
      ]
    )
    AS_IF(
      [test "x${PNG_CFLAGS}" = "xno"],
      [PNG_CFLAGS=""]
    )
    AS_IF(
      [test "x${PNG_LIBS}" = "x"],
      [
        AS_IF(
          [test "x$LIBPNG_CONFIG" != "x"],
          [PNG_LIBS=$(${LIBPNG_CONFIG} --static --libs)],
          [
            AC_CHECK_LIB([png], [png_access_version_number],
              [
                PNG_LIBS="${PNG_LIBS} -lpng"
                AC_CHECK_LIB([z], [zlibVersion],
                  [PNG_LIBS="${PNG_LIBS} -lz"]
                )
                PNG_LIBS="${PNG_LIBS} ${M_LIBS}"
              ],
              [AC_MSG_ERROR(['libpng' library file not found.])]
            )
          ]
        )
      ]
    )
    AS_IF(
      [test "x${PNG_LIBS}" = "xno"],
      [PNG_LIBS=""]
    )
  ]
)
AC_SUBST([PNG_CFLAGS])
AC_SUBST([PNG_LIBS])

AS_IF(
  [test "x${config_ttf}" = "xyes"],
  [
    PKG_CHECK_MODULES(
      [FREETYPE2],
      [freetype2],
      ,
      [AC_MSG_ERROR([freetype was not found.])]
    )
  ]
)

AH_TOP([#ifndef __SPLASH_CONFIG_H
#define __SPLASH_CONFIG_H])
AH_BOTTOM([#endif /* SPLASH_CONFIG_H */])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile libs/Makefile misc/Makefile src/test/Makefile docs/Makefile scripts/Makefile])

AC_OUTPUT
