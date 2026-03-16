/*
 * Copyright (C) 2007 Roger While
 * Copyright (C) 2021 Ron Norman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1,
 * or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING.LIB.  If
 * not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"visam.h"

#ifdef _WIN32
#include	<ctype.h>
#include	<stdlib.h>
#endif

static void
usage (char *bin)
{
	printf ("Usage:\n\t%s [-r] [FILENAME...]\n", bin);
	printf ("      \t-r   if you want the file rebuilt as needed\n");
	printf ("      \t-C   To rebuild the file in C-ISAM format\n");
	printf ("      \t-V   To rebuild the file in VB-ISAM format\n");
	printf ("      \t-d n Set the duplicates field to 'n' bytes (2 or 4)\n");
	printf ("      \t-x n Set the index block size to 'n' bytes\n");
}

int
main (int iargc, char **ppcargv)
{
	int         iloop;
	int         idxsz = 0;
	int         duplen = 0;
	int         rebuild = 0;
	int         isformat = 0;

	if (iargc == 1) {
		usage (ppcargv[0]);
		return 1;
	}

	for (iloop = 1; iloop < iargc; iloop++) {
		if (memcmp (ppcargv[iloop], "-r", 2) == 0) {
			rebuild = 1;
			continue;
		}
		if (memcmp (ppcargv[iloop], "-h", 2) == 0) {
			usage (ppcargv[0]);
			exit (0);
		}
		if (memcmp (ppcargv[iloop], "-d", 2) == 0
		 || memcmp (ppcargv[iloop], "-D", 2) == 0) {
			if (isdigit (ppcargv[iloop][2]))
				duplen = atoi (&ppcargv[iloop][2]);
			else
				duplen = atoi (&ppcargv[++iloop][0]);
			continue;
		}
		if (memcmp (ppcargv[iloop], "-x", 2) == 0
		 || memcmp (ppcargv[iloop], "-X", 2) == 0) {
			if (isdigit (ppcargv[iloop][2]))
				idxsz = atoi (&ppcargv[iloop][2]);
			else
				idxsz = atoi (&ppcargv[++iloop][0]);
			continue;
		}
		if (memcmp (ppcargv[iloop], "-v", 2) == 0
		 || memcmp (ppcargv[iloop], "-V", 2) == 0) {
			isformat = ISMVBISAM;
			rebuild = 1;
			continue;
		}
		if (memcmp (ppcargv[iloop], "-c", 2) == 0
		 || memcmp (ppcargv[iloop], "-C", 2) == 0) {
			isformat = ISMCISAM;
			rebuild = 1;
			continue;
		}
		if (duplen != 2 && duplen != 4)
			duplen = 0;
		idxsz = (idxsz + 511) / 512;
		isformat |= (duplen << ISMDUPSHIFT) | (idxsz << ISMIDXSHIFT);
		ischeck ((VB_CHAR *) ppcargv[iloop], (const int) rebuild, (const int) isformat);
	}
	return 0;
}
