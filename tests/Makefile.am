#
# Makefile vbisam/tests
#
# Copyright (C) 2007 Roger While
# Copyright (C) 2020 Simon Sobisch
#
# This file is part of VBISAM.
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

noinst_PROGRAMS = mvtest detest00 detest01 jvntest mvtest2

# Use serial test runner (avoids dependency on build_aux/test-driver)
AUTOMAKE_OPTIONS = serial-tests

# P1 regression test — runs under 'make check', TAP output, exit 0 = pass
check_PROGRAMS = vbtest_p1
TESTS          = vbtest_p1

CLEANFILES = *.idx *.dat RECOVER* vbtp1_rw.idx vbtp1_rw.dat \
             vbtp1_del.idx vbtp1_del.dat \
             vbtp1_p4.idx vbtp1_p4.dat vbtp1_p4.log \
             vbtp1_aud.idx vbtp1_aud.dat vbtp1_aud.audit
DISTCLEANFILES = atconfig
EXTRA_DIST = TESTDATA

ISAM_LIB = -lvisam

AM_CFLAGS  = -I$(top_srcdir) -I$(top_srcdir)/libvisam
AM_LDFLAGS = -L$(top_builddir)/libvisam/.libs $(ISAM_LIB)
