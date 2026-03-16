#                                                       -*- Autoconf -*-
# Configure template for VISAM
# Process this file with autoconf to produce a configure script.

# Copyright (C) 2005-2007 Roger While
# Copyright (C) 2008 Cobol-IT
# Copyright (C) 2020 Simon Sobisch
# Copyright (C) 2021 Ron Norman 
#
# This file is part of VISAM.
#
# VISAM is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 2.1 of the License,
# or (at your option) any later version.
#
# VISAM is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with VISAM.  If not, see <https://www.gnu.org/licenses/>.

AC_PREREQ(2.61)

AC_INIT([VISAM],
	[2.2],
	[visam-list@lists.sourceforge.net],
	[visam])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build_aux])

AC_CONFIG_FILES([Makefile libvisam/Makefile bin/Makefile
	tests/atlocal tests/Makefile])
AC_CONFIG_FILES([pre-inst-env:build_aux/pre-inst-env.in],
  [chmod +x pre-inst-env])

# Set CFLAGS/CXXFLAGS.  We MUST set the flags before we call autoconf
# compiler configuration macros, because if we don't, they set CFLAGS
# to no optimization and -g, which isn't what we want.
CFLAGS=${CFLAGS-""}
CXXFLAGS=${CXXFLAGS-"$CFLAGS"}
LDFLAGS=${LDFLAGS-""}

# Note for SUN Solaris (gcc)
# export/setenv CFLAGS "-m64 -mptr64"
# export/setenv LDFLAGS "-m64 -mptr64 -L/usr/local/lib/sparcv9"
# ./configure --libdir=/usr/local/lib/sparcv9
#

# Hack for AIX 64 bit
AIX64=0
# Hack for AIX 64 bit (gcc)
# Required -
# export/setenv CFLAGS=-maix64
# export/setenv LDFLAGS=-maix64
if echo $CFLAGS | grep 'aix64' 1>/dev/null 2>&1
then
	AIX64=1
fi
# Hack for AIX 64 bit (xlc)
# Required -
# export/setenv CFLAGS=-q64
# export/setenv LDFLAGS=-q64
if echo $CFLAGS | grep 'q64' 1>/dev/null 2>&1
then
	AIX64=1
fi
if test "$AIX64" = "1" 
then
	if test -f /usr/ccs/bin/ar
	then
		AR="/usr/ccs/bin/ar -X64"
	else
		AR="ar -X64"
	fi
	NM="/usr/ccs/bin/nm -X64 -B"
fi

AM_INIT_AUTOMAKE([gnu no-texinfo.tex])

# Autoheader templates
AH_TEMPLATE([ISAMMODE],   [ISAM mode 0=C-ISAM compatible, 1=Extended])
AH_TEMPLATE([WITH_LFS64], [Compile with large file system I/O])
AH_TEMPLATE([VBDEBUG],   [Turn on debugging mode])
AH_TEMPLATE([__USE_STRING_INLINES], [Compiler optimization])
AH_TEMPLATE([HAS_INLINE], [Can use inline keyword])
AH_TEMPLATE([HAVE__THREAD_ATTR], [Can use __THREAD keyword])
AH_TEMPLATE([HAVE_PTHREAD_H], [Have usable libpthread])
AH_TEMPLATE([WITH_LOGGING],   [Enable logging/transaction mode])

# preparation for cross-compilation
AC_ARG_PROGRAM

AC_DEFINE(__USE_STRING_INLINES)

AC_ARG_ENABLE([vbisamdefault],
  [AS_HELP_STRING([--enable-vbisamdefault],
    [(VISAM) VB-ISAM default mode])],
  [CISAM_MODE=no
   DEFAULT_MODE=VB-ISAM],
  [CISAM_MODE=yes
   DEFAULT_MODE=C-ISAM])

AC_ARG_WITH(lfs64,
  [  --with-lfs64            (VISAM) use large file system for file I/O],
  [ AC_DEFINE(WITH_LFS64, 1)],
  [ ])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
    [(VISAM) Enable -g C compiler debug option])],
  [enable_debug=yes],
  [enable_debug=no])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--enable-hardening, --disable-hardening],
    [Enable GNU C "hardening" options: define _FORTIFY_SOURCE and use -fstack-protector.
	If disabled, these are explicit removed from CFLAGS for building VISAM.
	Defaults to "defined by CFLAGS".])],,
  [enable_hardening="unset"])

AC_ARG_ENABLE([logging],
  [AS_HELP_STRING([--enable-logging],
    [(VISAM) enable logging/transaction feature])],
  [ AC_DEFINE(WITH_LOGGING, 1)
    with_logging=yes ],
  [ woth_logging=no ])

AS_IF([test "$enable_debug" = "yes"], [
   AC_DEFINE(VBDEBUG, 1)
])


if test "${CISAM_MODE}" = "yes"; then
	AC_DEFINE(ISAMMODE, 0)
else
	AC_DEFINE(ISAMMODE, 1)
	AC_DEFINE(WITH_LFS64, 1)
fi

AC_MSG_CHECKING(--enable-thread-safe)
AC_ARG_ENABLE(thread-safe,
	[AC_HELP_STRING([--enable-thread-safe],
			[Build Thread safe code. No longer supported])],
	[CIT_THREAD_SAFE="yes"
	 CPPFLAGS="$CPPFLAGS -D_REENTRANT"],
	[CIT_THREAD_SAFE="no"])
AC_MSG_RESULT($CIT_THREAD_SAFE)

CIT_PTHREAD="no"
AC_MSG_CHECKING(--with-pthread)
AC_ARG_WITH(pthread,
	[AC_HELP_STRING([--with-pthread],
			[Use pthread instead of __thread])],
	[CIT_PTHREAD="yes"], [CIT_PTHREAD="no"])
AC_MSG_RESULT($CIT_PTHREAD)

# Checks for programs.
AC_PROG_CC(gcc xlc cc)
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROVIDE([AC_PROG_F77])
AC_PROVIDE([AC_PROG_CXX])
with_tags=
AC_PROG_LN_S
AC_PROG_INSTALL
# AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_CHECK_TOOLS(AR, ar, :)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h malloc.h locale.h stddef.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_C_INLINE
if test "$ac_cv_c_inline" != no ; then
	AC_DEFINE(HAS_INLINE,1)
fi

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([memmove memset regcomp pow setlocale fcntl strerror \
		strcasecmp strchr strrchr strdup strstr strtol])

AC_CACHE_CHECK([for long long], ac_cv_type_long_long,
	[AC_TRY_LINK([long long ll = 1LL; int i = 63;],
			[long long llmax = (long long) -1;
				return ll << i | ll >> i | llmax / ll | llmax % ll;],
			ac_cv_type_long_long=yes,
			ac_cv_type_long_long=no)])
if test $ac_cv_type_long_long = yes; then
	AC_DEFINE(HAVE_LONG_LONG, 1,
	[Define if you have the 'long long' type.])
fi

if test "$enable_debug" = "no"
then
	CFLAGS=`echo "$CFLAGS " | sed -e 's/-g //'`
	if test "`basename $CC`" = "gcc"
	then
		vb_temp_flags="`${CC} ${srcdir}/cpucheck.c -o cpucheck 1>/dev/null 2>&1 && ./cpucheck 2>/dev/null`"
		rm -f cpucheck${EXEEXT}
		if test "x$vb_temp_flags" != "x"
		then
			CFLAGS="$CFLAGS $vb_temp_flags -finline-functions -fomit-frame-pointer"
		else
			CFLAGS="$CFLAGS -finline-functions -fomit-frame-pointer"
		fi
	fi
fi

if test "$enable_hardening" = "yes"; then
	AC_MSG_NOTICE([Compile with hardening options])
	# needs optimization
	if test "$enable_debug" = "yes"; then
	   CFLAGS=" -O $CFLAGS"
	fi
	CPPFLAGS="$CPPFLAGS -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"

	curr_cflags="$CFLAGS"
	temp_flags="-fstack-protector-strong"
	CFLAGS="$curr_cflags $temp_flags"
	AC_MSG_CHECKING([for $temp_flags option])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[int testint;]])],
		[AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])
		temp_flags="-fstack-protector"
		CFLAGS="$curr_cflags $temp_flags"
		AC_MSG_CHECKING([for $temp_flags option])
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[int testint;]])],
			[AC_MSG_RESULT([yes])],
			[CFLAGS="$curr_cflags"; temp_flags=""; AC_MSG_RESULT([no])])
		 ])
	if test "x$temp_flags" != "x"; then
	   if test "x$LDFLAGS" != "x"; then
	      LDFLAGS="$LDFLAGS $temp_flags"
	   else
	      LDFLAGS="$temp_flags"
	   fi
	fi
	unset temp_flags
fi

AH_VERBATIM([_XOPEN_SOURCE_EXTENDED],
[/* Define to 1 if on HPUX.  */
#ifndef _XOPEN_SOURCE_EXTENDED
# undef _XOPEN_SOURCE_EXTENDED
#endif])dnl

COB_OPTIMIZE_FLAG=${OPTIMIZE_FLAG-"-O"}
case $host_os in
  hpux*)
	if test "`basename $CC`" != "gcc"
	then
		COB_OPTIMIZE_FLAG=${OPTIMIZE_FLAG-"+O2"}
		COB_SHARED_OPT="-b"
		AC_DEFINE([_XOPEN_SOURCE_EXTENDED],1)
		rm -f hptest*
		echo 'int main() { return 0; }' > hptest.c
		${CC} ${CFLAGS} +Olit=all -o hptest hptest.c > hptest.out 2>&1
		if test $? -ne 0 -o -s hptest.out
		then
			CFLAGS="$CFLAGS +ESlit"
		else
			CFLAGS="$CFLAGS +Olit=all"
		fi
		rm -f hptest*
	fi
	;;
  solaris*)
	if test "`basename $CC`" != "gcc"
	then
		COB_OPTIMIZE_FLAG=${OPTIMIZE_FLAG-"-xO2"}		
		COB_CFLAGS="$COB_CFLAGS -xstrconst -w"
		COB_SHARED_OPT="-G"
	fi
	;;
esac

if test "`basename $CC`" = "gcc"
then
	CFLAGS="$CFLAGS -fsigned-char -Wall -Wwrite-strings -Wmissing-prototypes -Wno-format-y2k"
fi

# Checks __thread 
if test "$CIT_THREAD_SAFE" = "yes"; then
 if test "$CIT_PTHREAD" = "no"; then
  AC_MSG_CHECKING(support __thread)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[__thread int mythreadvar;]], [[
 	mythreadvar = 0;
 	return mythreadvar;
 	]])],
 	[AC_DEFINE(HAVE__THREAD_ATTR)],
 	[CIT_THREAD_SAFE="no"
 	 CIT_PTHREAD = "yes"]
 	)

  AC_MSG_RESULT(${CIT_THREAD_SAFE})
 fi

 if test "$CIT_PTHREAD" = "yes"; then
  AC_MSG_CHECKING(support libpthread)
  AC_CHECK_HEADERS([pthread.h], [AC_CHECK_LIB([pthread], [pthread_setspecific], 
				                         [AC_DEFINE(HAVE_PTHREAD_H)
                                          LDFLAGS="$LDFLAGS -lpthread"], 
	                                     [CIT_THREAD_SAFE="no"]) ],
			       [CIT_THREAD_SAFE="no"])
  AC_MSG_RESULT(${CIT_THREAD_SAFE})
 fi
fi


AM_CONDITIONAL([CISAM_MODE], [test "${CISAM_MODE}" = "yes"])

AC_OUTPUT

echo ""
echo "VISAM Configuration:"
echo ""
echo "  Default file creation mode     ${DEFAULT_MODE}"
echo "  Debugging mode                 ${enable_debug}"
echo "  Thread safe                    ${CIT_THREAD_SAFE}"
echo "  Large file system              ${with_lfs64}"
echo "  Logging                        ${with_logging}"
echo ""
echo "  CC                             ${CC}"
echo "  CFLAGS                         ${CFLAGS}"
echo "  CPPFLAGS                       ${CPPFLAGS}"
echo ""
