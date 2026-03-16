#
# Makefile vbisam/bin
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

ISAM_LIB = -lvisam

AM_LDFLAGS = -L$(top_builddir)/libvisam $(ISAM_LIB)
AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/libvisam

# note: adjusted with internal program_transform_name for cisam-compat
bin_PROGRAMS = vcheck vrecover
