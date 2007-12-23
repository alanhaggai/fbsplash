QUIET ?= true

LIBFREETYPE2_SOURCE_INTERNAL = libs/freetype-2.3.5
LIBJPEG_SOURCE_INTERNAL      = libs/jpeg-6b
LIBPNG_SOURCE_INTERNAL       = libs/libpng-1.2.18
LIBZ_SOURCE_INTERNAL         = libs/zlib-1.2.3

# HACK: automake doesn't support ifeq, but will ignore it if it doesn't start
# at column 0
 ifeq ($(strip $(QUIET)),true)
	Q = @
	OUTPUT = > /dev/null
	infmsg = printf "  %-7s %s\n" $(1) $(2)
	AM_MAKEFLAGS    += --silent
	AM_LIBTOOLFLAGS += --silent
 else
	OUTPUT =
	Q =
	infmsg = :
 endif


