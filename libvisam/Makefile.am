#
# Makefile visam/libvisam
#
# Copyright (C) 2007 Roger While
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

COMMON_SOURCES = isdecimal.c ishelper.c isaudit.c isbuild.c isdelete.c \
	isopen.c isread.c isrecover.c isrewrite.c istrans.c iswrite.c ischeck.c \
	vbdataio.c vbindexio.c vbkeysio.c vblocking.c \
	vblowlevel.c vbmemio.c vbnodememio.c isinternal.h byteswap.h

AM_CFLAGS = -I$(top_srcdir)
AM_LDFLAGS = -no-undefined

lib_LTLIBRARIES = libvisam.la
libvisam_la_SOURCES = $(COMMON_SOURCES)
libvisam_la_LDFLAGS = -version-info 4:0:0

