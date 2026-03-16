#
# Makefile visam/tests
#
# Copyright (C) 2007 Roger While
# Copyright (C) 2020 Simon Sobisch
# Copyright (C) 2021 Ron Norman 
#
# This file was part of VBISAM.
#
# VBISAM is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 2.1 of the License,
# or (at your option) any later version.
#
# VBISAM is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with VBISAM.  If not, see <https://www.gnu.org/licenses/>.
#
# The package has now been renamed to V-ISAM

SUBDIRS = libvisam bin tests
ACLOCAL_AMFLAGS = -I m4

pkgincludedir = $(includedir)
pkginclude_HEADERS = vdecimal.h visam.h visam_rename.h

EXTRA_DIST= cpucheck.c libvisam/iscommon.c 

dist_noinst_SCRIPTS = autogen.sh build_aux/bootstrap

# files shipped with the package that should be 755'ed:
FILES_TO_BE_EXECUTABLE = $(dist_noinst_SCRIPTS) \
	build_aux/config.guess build_aux/config.sub \
	build_aux/depcomp build_aux/install-sh build_aux/ltmain.sh build_aux/mdate-sh \
	build_aux/missing build_aux/mkinstalldirs

dist-hook:
	find $(top_distdir) -type d -print | xargs chmod 755	# otherwise directories have 777
	find $(top_distdir) -type f -print | xargs chmod 644	# otherwise files are unchanged but with u+r
# because of the global file change, adjust here again
	cd $(top_distdir) && chmod 755 $(FILES_TO_BE_EXECUTABLE)
