SUBDIRS = libvbisam bin tests
ACLOCAL_AMFLAGS = -I m4

# GCC 13+ compatibility: VBISAM is C89-era code. GCC 13 made several
# C89 implicit behaviors into hard errors. These flags restore the
# pre-GCC-13 warning-only behavior without changing any source semantics.
AM_CFLAGS = -Wno-implicit-int \
            -Wno-implicit-function-declaration \
            -Wno-incompatible-pointer-types

pkgincludedir = $(includedir)
pkginclude_HEADERS = vbdecimal.h vbisam.h

EXTRA_DIST= cpucheck.c libvbisam/iscommon.c \
	docs/cisamdiff.txt  docs/vbisamdoc.txt docs/DOCO  docs/TODO

