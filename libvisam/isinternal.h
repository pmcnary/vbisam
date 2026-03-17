/*
 * Copyright (C) 2003 Trevor van Bremen
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

#ifndef	VB_LIBVBISAM_H
#define	VB_LIBVBISAM_H

#include	"config.h"


/* Note : following are pulled in from config.h : */
/* ISAMMODE		- ISAM Mode */
/* HAVE_LFS64		- 64 bit file I/O */
/* VBDEBUG		- Debugging mode */

#ifdef      _MSC_VER
#ifdef VBISAM_BUILD_DLL
#ifdef VBISAM_LIB
#define VBISAM_DLL_EXPIMP __declspec(dllexport)
#else
#define VBISAM_DLL_EXPIMP __declspec(dllimport)
#endif
#else
#define VBISAM_DLL_EXPIMP
#endif
#else
#define VBISAM_DLL_EXPIMP
#endif

#if defined(__LP64__) || defined(__ppc64__) || defined(__ia64) || defined(__x86_64__)
#define VISAM_64BIT 1
#endif

#ifdef WITH_LFS64
#define _LFS64_LARGEFILE	1
#define _LFS64_STDIO	1
#define _FILE_OFFSET_BITS	64
#define _LARGEFILE64_SOURCE	1
#define __USE_LARGEFILE64	1
#define __USE_FILE_OFFSET64	1
#define _LARGE_FILES	1

#if (defined(__hpux__) || defined(__hpux)) && !defined(__LP64__)
#define _APP32_64BIT_OFF_T	1
#endif
#if (defined(__hpux__) || defined(__hpux))
#define VB_SHORT_LOCK_RANGE     1
#endif

#define	VB_MAX_OFF_T		(off_t)9223372036854775807LL
#elif defined(VISAM_64BIT)
#define	VB_MAX_OFF_T		(off_t)9223372036854775807LL
#else
#define	VB_MAX_OFF_T		(off_t)2147483647
#endif

#define MAX_BUFFER_LENGTH   65536

#if defined(ibm) || defined (__ppc64__) || defined(aix)
#if defined(_XOPEN_SOURCE_EXTENDED)
#undef _XOPEN_SOURCE_EXTENDED
#endif
/* This was causing compile errors in /usr/include/unistd.h on AIX */
#if defined(_LARGE_FILES)
#undef _LARGE_FILES
#endif
#if defined(_LARGE_FILE_API)
#undef _LARGE_FILE_API
#endif
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef  HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<ctype.h>
#ifdef	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<time.h>
#include	<limits.h>
#include	<float.h>

#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#include	<windows.h>
#include	<io.h>
#include	<sys/locking.h>
#define	open(x, y, z)	_open(x, y, z)
#define	read(x, y, z)	_read(x, y, z)
#define	write(x, y, z)	_write(x, y, z)
#ifndef _UNISTD_H
#ifdef WITH_LFS64
#define	lseek(x, y, z)	_lseeki64(x, y, z)
#else
#define	lseek(x, y, z)	_lseek(x, y, z)
#endif
#endif
#define	close(x)	_close(x)
#define	unlink(x)	_unlink(x)
#define fsync		_commit
typedef int uid_t;
#ifndef	_PID_T_
typedef int pid_t;
#endif
#ifndef	_MODE_T_
typedef unsigned short mode_t;
#endif
#if (!defined(_SSIZE_T_DEFINED) && !defined(_SIZE_T_))
typedef int ssize_t;
#endif
#endif

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE 1
#define VB_INLINE _inline
#include	<malloc.h>
#pragma warning(disable: 4996)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define __attribute__(x)
#define __i386__
	/*#define VB_THREAD __declspec( thread ) */

#elif  __370__
#define VB_INLINE __inline
#elif defined(HAS_INLINE)
#define VB_INLINE inline
#else
#define VB_INLINE
#endif


/*#define	VBISAM_LIB only define for a dll*/
#include	"visam.h"

/* Implementation limits */
#define	MAX_NODE_LENGTH		8192	   /* Maximum block size for index file */
#define	V_DFLT_NODE_LENGTH	4096	   /* VB-ISAM default for index block size */
#define	C_DFLT_NODE_LENGTH	1024	   /* C-ISAM default for index block size */
#if (ISAMMODE == 1)
#define ISMMODE ISMVBISAM
#else
#define ISMMODE ISMCISAM
#endif

/*
 * VBLOCKING & VBC7LOCKING
 * should likely become 'configure' settings
 */
#ifndef VBLOCKING
#define VBLOCKING	2				   /* 0:off 1:old 2:std 3:lck */
#endif
#ifndef VBC7LOCKING
#define VBC7LOCKING	1				   /* 1:CISAM 5 -> 7.1; 2:CISAM 7.2 */
#endif

/* --enable-logging causes WITH_LOGGING to be set on */
#ifdef WITH_LOGGING
#ifndef VBLOGGING
#define VBLOGGING   1
#endif
#else
#ifndef VBLOGGING
#define VBLOGGING   0
#endif
#endif

#if VBLOCKING == 4					   /* Old VBISAM locking options */
#if (VB_SHORT_LOCK_RANGE == 1)
#define VBLKBUSYLEN    0x3FFFFFFFL
#define VBLKBUSYOFF    0x00000000L
#define VBLKDATALEN    0x3FFFFFFFL
#define VBLKDATAOFF    0x40000000L
#define VBLKFILEOFF    0x7FFFFFFFL
#else
#define VBLKBUSYLEN    0x3FFFFFFFFFFFFFFFLL
#define VBLKBUSYOFF    0x0000000000000000LL
#define VBLKDATALEN    0x3FFFFFFFFFFFFFFFLL
#define VBLKDATAOFF    0x4000000000000000LL
#define VBLKFILEOFF    0x7FFFFFFFFFFFFFFFLL
#endif
#define V_LKBUSYLEN    VBLKBUSYLEN
#define V_LKBUSYOFF    VBLKBUSYOFF
#define V_LKDATALEN    VBLKDATALEN
#define V_LKDATAOFF    VBLKDATAOFF
#define V_LKFILEOFF    VBLKFILEOFF

#elif ( VBLOCKING == 2 )
#define VBLKBUSYOFF     0x00000000L	   /* offset of I/O lock table */
#define VBLKBUSYLEN     0x3FFFFFFFL	   /* length of the above */
#define VBLKDATAOFF     0x40000000L	   /* offset of data lock table */
#define VBLKDATALEN     0x3FFFFFFFL	   /* length of the above */
#if ( VBC7LOCKING == 1 || VBC7LOCKING == 2 )
#define VBLKFILEOFF    0x7FFFFFFFL	   /* offset of file claim table */
#else
#define VBLKFILEOFF    0x40000000L	   /* offset of file claim table */
#endif

#elif ( VBLOCKING == 3 )
#define VBLKBUSYOFF     0x20000000L	   /* offset of I/O lock table */
#define VBLKBUSYLEN     0x20000000L	   /* length of the above */
#define VBLKDATAOFF     0x40000000L	   /* offset of data lock table */
#define VBLKDATALEN     0x40000000L	   /* length of the the above */
#define VBLKFILEOFF     0x00000000L	   /* offset of file claim table */

#else /* Default matches DISAM ISLOCKING == 1 */
#define VBLKBUSYOFF     0x3FF00000L	   /* offset of I/O lock table */
#define VBLKBUSYLEN     0x00100000L	   /* length of the above */
#define VBLKDATAOFF     0x40000000L	   /* offset of data lock table */
#define VBLKDATALEN     0x0FFFFFFFL	   /* length of the above */
#define VBLKFILEOFF     0x40000000L	   /* offset of file claim table */
#endif

#ifdef _MSC_VER
#define COB_THREAD __declspec( thread )
#define HAVE__THEAD_ATTR 1
extern COB_THREAD vb_rtd_t vb_rtd_data;
#else

#ifdef HAVE__PTHREAD_ATTR
extern __pthread vb_rtd_t vb_rtd_data;
#undef VB_GET_RTD
#define VB_GET_RTD &vb_rtd_data

#elif defined (HAVE_PTHREAD_H)
#else
extern vb_rtd_t vb_rtd_data;
#undef VB_GET_RTD
#define VB_GET_RTD &vb_rtd_data

#endif
#endif


#define VB_HIDDEN

#ifndef	O_BINARY
#define	O_BINARY	0
#endif

#ifdef	WITH_LFS64
#ifdef	_WIN32
#define off_t __int64
#endif
#endif

typedef VB_UCHAR *ucharptr;

/* Inline versions of load/store routines */
static VB_INLINE int
inl_ldint (void *pclocation)
{
	union {
		unsigned short v;
		VB_UCHAR    c[2];
	} val;
#ifndef	WORDS_BIGENDIAN
	val.c[1] = ((VB_UCHAR *) pclocation)[0];
	val.c[0] = ((VB_UCHAR *) pclocation)[1];
#else
	memcpy ((void *) (&val.c[0]), pclocation, 2);
#endif
	return (int) val.v;
}

static VB_INLINE void
inl_stint (int ivalue, void *pclocation)
{
	union {
		unsigned short v;
		VB_UCHAR    c[2];
	} val;
	val.v = (unsigned short) ivalue;
#ifndef	WORDS_BIGENDIAN
	((VB_UCHAR *) pclocation)[0] = val.c[1];
	((VB_UCHAR *) pclocation)[1] = val.c[0];
#else
	memcpy (pclocation, (void *) (&val.c[0]), 2);
#endif
	return;
}

static VB_INLINE int
inl_ldlong (void *pclocation)
{
	union {
		int         v;
		VB_UCHAR    c[4];
	} val;
#ifndef	WORDS_BIGENDIAN
	val.c[0] = ((VB_UCHAR *) pclocation)[3];
	val.c[1] = ((VB_UCHAR *) pclocation)[2];
	val.c[2] = ((VB_UCHAR *) pclocation)[1];
	val.c[3] = ((VB_UCHAR *) pclocation)[0];
#else
	memcpy ((void *) (&val.c[0]), pclocation, 4);
#endif
	return val.v;
}

static VB_INLINE void
inl_stlong (int lvalue, void *pclocation)
{
	union {
		int         v;
		VB_UCHAR    c[4];
	} val;
	val.v = lvalue;
#ifndef	WORDS_BIGENDIAN
	((VB_UCHAR *) pclocation)[3] = val.c[0];
	((VB_UCHAR *) pclocation)[2] = val.c[1];
	((VB_UCHAR *) pclocation)[1] = val.c[2];
	((VB_UCHAR *) pclocation)[0] = val.c[3];
#else
	memcpy (pclocation, (void *) (&val.c[0]), 4);
#endif
	return;
}

/* Load value from pclocation and return it
 * 'len' may be 2 or 4 or 8
 * sizeof(off_t) could be 4 or 8
 */
static VB_INLINE off_t
inl_ldcompx (void *pclocation, const int len)
{
	union {
		off_t       v;
		VB_UCHAR    c[sizeof (off_t)];
	}
	val;

#ifndef	WORDS_BIGENDIAN
	if (len == 8) {
		val.c[0] = ((VB_UCHAR *) pclocation)[7];
		val.c[1] = ((VB_UCHAR *) pclocation)[6];
		val.c[2] = ((VB_UCHAR *) pclocation)[5];
		val.c[3] = ((VB_UCHAR *) pclocation)[4];
		if (sizeof (off_t) == 8) {
			val.c[4] = ((VB_UCHAR *) pclocation)[3];
			val.c[5] = ((VB_UCHAR *) pclocation)[2];
			val.c[6] = ((VB_UCHAR *) pclocation)[1];
			val.c[7] = ((VB_UCHAR *) pclocation)[0];
		}
	} else if (len == 2) {
		val.v = 0;
		val.c[0] = ((VB_UCHAR *) pclocation)[1];
		val.c[1] = ((VB_UCHAR *) pclocation)[0];
	} else {
		val.v = 0;
		val.c[0] = ((VB_UCHAR *) pclocation)[3];
		val.c[1] = ((VB_UCHAR *) pclocation)[2];
		val.c[2] = ((VB_UCHAR *) pclocation)[1];
		val.c[3] = ((VB_UCHAR *) pclocation)[0];
	}
#else
	if (len == 8) {
		if (sizeof (off_t) == 8) {
			memcpy ((void *) &val.c[0], pclocation, 8);
		} else {
			memcpy ((void *) &val.c[0], (void *) ((VB_UCHAR *) pclocation)[4], 4);
		}
	} else if (len == 2) {
		val.v = 0;
		if (sizeof (off_t) == 4) {
			memcpy ((void *) &val.c[2], pclocation, 2);
		} else {
			memcpy ((void *) &val.c[6], pclocation, 2);
		}
	} else {
		if (sizeof (off_t) == 4) {
			memcpy ((void *) &val.c[0], pclocation, 4);
		} else {
			val.v = 0;
			memcpy ((void *) &val.c[4], pclocation, 4);
		}
	}
#endif
	return val.v;
}

/* Store tvalue into pclocation
 * 'len' may be 2 or 4 or 8
 * sizeof(off_t) could be 4 or 8
 */
static VB_INLINE void
inl_stcompx (off_t tvalue, void *pclocation, const int len)
{
	union {
		off_t       v;
		VB_UCHAR    c[sizeof (off_t)];
	}
	val;
	val.v = tvalue;
#ifndef	WORDS_BIGENDIAN
	if (len == 8) {
		((VB_UCHAR *) pclocation)[7] = val.c[0];
		((VB_UCHAR *) pclocation)[6] = val.c[1];
		((VB_UCHAR *) pclocation)[5] = val.c[2];
		((VB_UCHAR *) pclocation)[4] = val.c[3];
		if (sizeof (off_t) == 8) {
			((VB_UCHAR *) pclocation)[3] = val.c[4];
			((VB_UCHAR *) pclocation)[2] = val.c[5];
			((VB_UCHAR *) pclocation)[1] = val.c[6];
			((VB_UCHAR *) pclocation)[0] = val.c[7];
		} else {
			memset (pclocation, 0, 4);
		}
	} else if (len == 2) {
		((VB_UCHAR *) pclocation)[1] = val.c[0];
		((VB_UCHAR *) pclocation)[0] = val.c[1];
	} else {
		((VB_UCHAR *) pclocation)[3] = val.c[0];
		((VB_UCHAR *) pclocation)[2] = val.c[1];
		((VB_UCHAR *) pclocation)[1] = val.c[2];
		((VB_UCHAR *) pclocation)[0] = val.c[3];
	}
#else
	if (len == 8) {
		if (sizeof (off_t) == 8) {
			memcpy (pclocation, &val.c[0], 8);
		} else {
			memset (pclocation, 0, 4);
			memcpy ((void *) ((VB_UCHAR *) pclocation)[4], (void *) &val.c[0], 4);
		}
	} else if (len == 2) {
		if (sizeof (off_t) == 4) {
			memcpy (pclocation, (void *) &val.c[2], 2);
		} else {
			memcpy (pclocation, (void *) &val.c[6], 2);
		}
	} else {
		if (sizeof (off_t) == 4) {
			memcpy (pclocation, (void *) &val.c[0], 4);
		} else {
			memcpy (pclocation, (void *) &val.c[4], 4);
		}
	}
#endif
	return;
}

/* Short hand functions */
#define ld_compx(cx) inl_ldcompx(cx, (const int)sizeof(cx))
#define st_compx(v,cx) inl_stcompx((off_t)v, cx, (const int)sizeof(cx))
#define ld_compx8(cx) inl_ldcompx(cx, (const int)8)
#define st_compx8(v,cx) inl_stcompx((off_t)v, cx, (const int)8)
#define ld_compx4(cx) inl_ldcompx(cx, (const int)4)
#define st_compx4(v,cx) inl_stcompx((off_t)v, cx, (const int)4)

#define	SLOTS_PER_NODE	((MAX_NODE_LENGTH >> 2) - 1)	/* Used in vbvarlenio.c */

#define	MAX_PATH_LENGTH		512
#define	MAX_OPEN_TRANS		1024	   /* Currently, only used in isrecover () */
#define MAX_ISREC_LENGTH	32511
#define MAX_RESERVED_LENGTH	32768	   /* Greater then MAX_ISREC_LENGTH */

/* Arguments to ivblock */
#define	VBUNLOCK	0				   /* Unlock */
#define	VBRDLOCK	1				   /* A simple read lock, non-blocking */
#define	VBRDLCKW	2				   /* A simple read lock, blocking */
#define	VBWRLOCK	3				   /* An exclusive write lock, non-blocking */
#define	VBWRLCKW	4				   /* An exclusive write lock, blocking */

/* Values for ivbrcvmode (used to control how isrecover works) */
#define	RECOV_C		0x00			   /* Boring old buggy C-ISAM mode */
#define	RECOV_VB	0x01			   /* New, improved error detection */
#define	RECOV_LK	0x02			   /* Use locks in isrecover (See documentation) */

/* Values for vb_rtd->ivbintrans */
#define	VBNOTRANS	0				   /* NOT in a transaction at all */
#define	VBBEGIN		1				   /* An isbegin has been issued but no more */
#define	VBNEEDFLUSH	2				   /* Something BEYOND just isbegin has been issued */
#define	VBCOMMIT	3				   /* We're in 'iscommit' mode */
#define	VBROLLBACK	4				   /* We're in 'isrollback' mode */
#define	VBRECOVER	5				   /* We're in 'isrecover' mode */

/*
VB_HIDDEN extern	VB_CHAR	vb_rtd->cvbtransbuffer[MAX_BUFFER_LENGTH];
*/

struct VBLOCK {
	struct VBLOCK *psnext;
	int         ihandle;		/* The handle that 'applied' this lock */
	off_t       trownumber;
};

struct VBKEY {
	struct VBKEY *psnext;		/* Next key in this node */
	struct VBKEY *psprev;		/* Previous key in this node */
	struct VBTREE *psparent;	/* Pointer towards ROOT */
	struct VBTREE *pschild;		/* Pointer towards LEAF */
	off_t       trownode;		/* The row / node number */
	off_t       tdupnumber;		/* The duplicate number (1st = 0) */
	VB_UCHAR    iisnew;			/* If this is a new entry (split use) */
	VB_UCHAR    iishigh;		/* Is this a GREATER THAN key? */
	VB_UCHAR    iisdummy;		/* A simple end of node marker */
	VB_UCHAR    pspare[5];		/* Spares */
	VB_UCHAR    ckey[1];		/* Placeholder for the key itself */
};

struct VBTREE {
	struct VBTREE *psnext;		/* Used for the free list only! */
	struct VBTREE *psparent;	/* The next level up from this node */
	struct VBKEY *pskeyfirst;	/* Pointer to the FIRST key in this node */
	struct VBKEY *pskeylast;	/* Pointer to the LAST key in this node */
	struct VBKEY *pskeycurr;	/* Pointer to the CURRENT key */
	off_t       tnodenumber;	/* The actual node */
	off_t       ttransnumber;	/* Transaction number stamp */
	unsigned int ilevel;		/* The level number (0 = LEAF) */
	unsigned int ikeysinnode;	/* # keys in pskeylist */
	VB_UCHAR    iisroot;		/* 1 = This is the ROOT node */
	VB_UCHAR    iistof;			/* 1 = First entry in index */
	VB_UCHAR    iiseof;			/* 1 = Last entry in index */
	VB_UCHAR    pspare[5];		/* Spare */
	struct VBKEY *pskeylist[512];
};

#define QUADSIZ4 4
#define QUADSIZ8 8

/**************************************/
/* Old style VB-ISAM dictionary block */
/* This is the format of the control block on disk */
/**************************************/
struct VBDICTNODE {				/* Offset VB-ISAM */
	VB_UCHAR    cvalidation[2];	/* 0x00   0x5642  ("VB") */
	VB_CHAR     cheaderrsvd;	/* 0x02   0x02 */
	VB_CHAR     cfooterrsvd;	/* 0x03   0x02 */
	VB_CHAR     crsvdperkey;	/* 0x04   0x08 */
	VB_CHAR     crfu1;			/* 0x05   0x04 */
	VB_CHAR     cnodesize[INTSIZE];	/* 0x06   Index block size minus 1 */
	VB_CHAR     cindexcount[INTSIZE];	/* 0x08   Number of indexes */
	VB_CHAR     crfu2[2];		/* 0x0a   0x0704 DUPLEN is 4 */
	VB_CHAR     cfileversion;	/* 0x0c   Same */
	VB_CHAR     cminrowlength[INTSIZE];	/* 0x0d   Same */
	VB_CHAR     cnodekeydesc[QUADSIZ8];	/* 0x0f   Normaly 2 */
	VB_CHAR     clocalindex;	/* 0x17   Same */
	VB_CHAR     crfu3[5];		/* 0x18   Same */
	VB_CHAR     cdatafree[QUADSIZ8];	/* 0x1d   Same */
	VB_CHAR     cnodefree[QUADSIZ8];	/* 0x25   Same */
	VB_CHAR     cdatacount[QUADSIZ8];	/* 0x2d   Same */
	VB_CHAR     cnodecount[QUADSIZ8];	/* 0x35   Same */
	VB_CHAR     ctransnumber[QUADSIZ8];	/* 0x3d   Same */
	VB_CHAR     cuniqueid[QUADSIZ8];	/* 0x45   Same */
	VB_CHAR     cnodeaudit[QUADSIZ8];	/* 0x4d   Same */
	VB_CHAR     clockmethod[INTSIZE];	/* 0x55   0x0008 */
	VB_CHAR     crfu4[QUADSIZ8];	/* 0x57   Same */
	VB_CHAR     cmaxrowlength[INTSIZE];	/* 0x5f   Same */
	VB_CHAR     cvarleng[9][QUADSIZ8];	/* 0x61   Same */
	VB_CHAR     crfulocalindex[36];	/* 0xa9   Same */
	/*                 ----   ---- */
	/* Length Total    0xcd */
	/* The last 3 bytes of this block are: x00000A */
	/* I.e. at offset  (cnodesize - 3) */
};

/*****************************************************/
/* C/D-ISAM style dictionary block                   */
/* This is the format of the control block on disk   */
/* This will be copied to/from the VBDICTNODE format */
/*****************************************************/
struct CIDICTNODE {				/* Offset   C-ISAM  */
	VB_UCHAR    cvalidation[2];			/* 0x00     0xfe53  */
	VB_CHAR     cheaderrsvd;			/* 0x02     0x02    */
	VB_CHAR     cfooterrsvd;			/* 0x03     0x02    */
	VB_CHAR     crsvdperkey;			/* 0x04     0x04    */
	VB_CHAR     crfu1;					/* 0x05     0x04    */
	VB_CHAR     cnodesize[INTSIZE];		/* 0x06     Index block size minus 1 */
	VB_CHAR     cindexcount[INTSIZE];	/* 0x08     Number of indexes   */
	VB_CHAR     crfu2[2];				/* 0x0A     0x070x  2nd byte DUPLEN, 2 or 4 */
	VB_CHAR     cfileversion;			/* 0x0C     0x00    */
	VB_CHAR     cminrowlength[INTSIZE];	/* 0x0D     Varies  */
	VB_CHAR     cnodekeydesc[QUADSIZ4];	/* 0x0F     Normally 2 */
	VB_CHAR     clocalindex;			/* 0x13     0x00     */
	VB_CHAR     crfu3[5];				/* 0x14     0x00...  */
	VB_CHAR     cdatafree[QUADSIZ4];	/* 0x19     data free list */
	VB_CHAR     cnodefree[QUADSIZ4];	/* 0x1D     index free list */
	VB_CHAR     cdatacount[QUADSIZ4];	/* 0x21     next data rec */
	VB_CHAR     cnodecount[QUADSIZ4];	/* 0x25     next index rec */
	VB_CHAR     ctransnumber[QUADSIZ4];	/* 0x29     header updates */
	VB_CHAR     cuniqueid[QUADSIZ4];	/* 0x2D     unique # */
	VB_CHAR     cnodeaudit[QUADSIZ4];	/* 0x31     audit trail */
	VB_CHAR     crfu4[2];				/* 0x35     locking method 0x0008 */
	VB_CHAR     crfu5[4];				/* 0x37     filler */
	VB_CHAR     cmaxrowlength[INTSIZE];	/* 0x3B     */
	VB_CHAR     cvarleng[5][QUADSIZ4];	/* 0x3D     */
	VB_CHAR     crfulocalindex[36];		/* 0x4F     0x00... collate Sequence */
	/*                 ----  */
	/* Length Total    0x75  */
	/* The last 4 bytes of this block are: "dism" for DISAM 7.0 */
	/* The last 4 bytes of this block are: "DISa" for DISAM 7.2 */
	/* I.e. at offset  (cnodesize - 4) */
	/* The last 3 bytes of the 1st index block are: xff7e00 */
	/* I.e. at offset  (cnodesize - 3) */
};

struct DICTINFO {
	int         inkeys;			/* Number of keys */
	int         iactivekey;		/* Which key is the active key */
	int         idatahandle;	/* file descriptor of the .dat file */
	int         iindexhandle;	/* file descriptor of the .idx file */
	int         iisopen;		/* 0: Table open, Files open, Buffers OK */
	/* 1: Table closed, Files open, Buffers OK */
	/*    Used to retain locks */
	/* 2: Table closed, Files closed, Buffers OK */
	/*    Basically, just caching */
	int         iopenmode;		/* The type of open which was used */
	int         ivarlenlength;	/* Length of varlen component */
	int         ivarlenslot;	/* The slot number within tvarlennode */
	int         iformat;		/* File Format: 0=VBISAM, 1=C-ISAM */
#define V_ISAM_FILE	 0
#define C_ISAM_FILE	 1
	off_t       trownumber;		/* Which data row is "CURRENT" 0 if none */
	off_t       tdupnumber;		/* Which duplicate number is "CURRENT" (0=First) */
	off_t       trowstart;		/* ONLY set to nonzero by isstart() */
	off_t       ttranslast;		/* Used to see whether to set iindexchanged */
	off_t       tnrows;			/* Number of rows (0 IF EMPTY, 1 IF NOT) */
	off_t       tvarlennode;	/* Node containing 1st varlen data */
	VB_CHAR    *cfilename;
	VB_CHAR    *ppcrowbuffer;	/* tminrowlength buffer for key (re)construction */
	VB_UCHAR   *collating_sequence;	/* Collating sequence */

	VB_UCHAR    iisdisjoint;	/* If set, CURR & NEXT give same result */
	VB_UCHAR    iisdatalocked;	/* If set, islock() is active */
	VB_UCHAR    iisdictlocked;	/* Relates to pdictbuf below */
	/* 0x00: Content on file MIGHT be different */
	/* 0x01: Dictionary node is LOCKED */
	/* 0x02: pdictbuf needs to be rewritten */
	/* 0x04: do NOT increment Dict.Trans */
	/*       isrollback () is in progress */
	/*      (Thus, suppress some ivbenter/ivbexit) */
	VB_UCHAR    iindexchanged;	/* Various */
	/* 0: Index has NOT changed since last time */
	/* 1: Index has changed, blocks invalid */
	/* 2: Index has changed, blocks are valid */
	VB_UCHAR    itransyet;		/* Relates to isbegin () et al */
	/* 0: FO Trans not yet written */
	/* 1: FO Trans written outside isbegin */
	/* 2: FO Trans written within isbegin */
	VB_UCHAR    iauditactive;	/* 0: no audit, 1: audit trail active */
	VB_UCHAR    dspare2[2];		/* spare (to complete 8 bytes) */
	int         iaudithandle;	/* fd of audit trail file (-1 = none) */
	VB_CHAR    *pcauditfilename;	/* audit trail filename (malloc'd) */
	/****************************************************
	 * Following are a copy of the data in the dictionary node
	 * but in normal C variable format
	 ****************************************************/
	int         irsvdperkey;	/* Reserved bytes per key entry */
	int         inodesize;		/* Index block size */
	int         iduplen;		/* Size of the duplicates counter */
	int         iquadsize;		/* Size of value holding index/data block/record # */
	/* 8 for VB-ISAM and 4 for C-ISAM */
	int         iminrowlength;	/* Minimum data row length */
	int         imaxrowlength;	/* Maximum data row length */
	int         inodekeydesc;	/* Index node# of first key description */
	off_t       idatafree;		/* Index node# of free data list */
	off_t       inodefree;		/* Index node# of free index list */
	off_t       idatacount;		/* Record# of next record in data file */
	off_t       inodecount;		/* Index node# of next index block in file */
	off_t       itransnumber;	/* Transaction number */
	off_t       iuniqueid;		/* Unique id */
	off_t       inodeaudit;		/* Pointer to audit trail information */
	off_t       ivarleng[9];	/* Group 0-8 hash pointer */
	/****************************************************/
	VB_UCHAR   *pdictbuf;		/* Area to holding the dictionary block of index */
	struct keydesc *pskeydesc[MAXSUBS];	/* Array of key description info */
	struct VBTREE *pstree[MAXSUBS];	/* Linked list of index nodes */
	struct VBKEY *pskeyfree[MAXSUBS];	/* An array of linked lists of free VBKEYs */
	struct VBKEY *pskeycurr[MAXSUBS];	/* An array of 'current' VBKEY pointers */
};

#define	VBL_BUILD	("BU")
#define	VBL_BEGIN	("BW")
#define	VBL_CREINDEX	("CI")
#define	VBL_CLUSTER	("CL")
#define	VBL_COMMIT	("CW")
#define	VBL_DELETE	("DE")
#define	VBL_DELINDEX	("DI")
#define	VBL_FILEERASE	("ER")
#define	VBL_FILECLOSE	("FC")
#define	VBL_FILEOPEN	("FO")
#define	VBL_INSERT	("IN")
#define	VBL_RENAME	("RE")
#define	VBL_ROLLBACK	("RW")
#define	VBL_SETUNIQUE	("SU")
#define	VBL_UNIQUEID	("UN")
#define	VBL_UPDATE	("UP")

struct SLOGHDR {
	VB_CHAR     clength[INTSIZE];
	VB_CHAR     coperation[2];
	VB_CHAR     cpid[INTSIZE];
	VB_CHAR     cuid[INTSIZE];
	VB_CHAR     ctime[LONGSIZE];
	VB_CHAR     crfu1[INTSIZE];
	VB_CHAR     clastposn[INTSIZE];
	VB_CHAR     clastlength[INTSIZE];
};



/* isopen.c */
VB_HIDDEN extern int ivbclose2 (const int ihandle);
VB_HIDDEN extern void ivbclose3 (const int ihandle);

/* isbuild.c */
VB_HIDDEN int VBiaddkeydescriptor (const int ihandle, struct keydesc *pskeydesc);

/* isread.c */
VB_HIDDEN extern int ivbcheckkey (const int ihandle, struct keydesc *pskey,
								  const int imode, int irowlength,
								  const int iisbuild);

/* istrans.c */
VB_HIDDEN extern int ivbtransbuild (const VB_CHAR * pcfilename, const int iminrowlen,
									const int imaxrowlen, struct keydesc *pskeydesc,
									const int imode);
VB_HIDDEN extern int ivbtranscreateindex (int ihandle, struct keydesc *pskeydesc);
VB_HIDDEN extern int ivbtranscluster (void);
VB_HIDDEN extern int ivbtransdelete (int ihandle, off_t trownumber, int irowlength);
VB_HIDDEN extern int ivbtransdeleteindex (int ihandle, struct keydesc *pskeydesc);
VB_HIDDEN extern int ivbtranserase (VB_CHAR * pcfilename);
VB_HIDDEN extern int ivbtransclose (int ihandle, VB_CHAR * pcfilename);
VB_HIDDEN extern int ivbtransopen (int ihandle, const VB_CHAR * pcfilename);
VB_HIDDEN extern int ivbtransinsert (int ihandle, off_t trownumber, int irowlength,
									 VB_CHAR * pcrow);
VB_HIDDEN extern int ivbtransrename (VB_CHAR * pcoldname, VB_CHAR * pcnewname);
VB_HIDDEN extern int ivbtranssetunique (int ihandle, off_t tuniqueid);
VB_HIDDEN extern int ivbtransuniqueid (int ihandle, off_t tuniqueid);
VB_HIDDEN extern int ivbtransupdate (int ihandle, off_t trownumber, int ioldrowlen,
									 int inewrowlen, VB_CHAR * pcrow);

/* isaudit.c */
VB_HIDDEN extern int ivbauditrecord (const int ihandle, const VB_CHAR *pctype,
									  off_t trownumber, const VB_CHAR *pcrow,
									  int irowlength);

/* iswrite.c */
VB_HIDDEN extern int ivbwriterow (const int ihandle, VB_CHAR * pcrow,
								  off_t trownumber);

/* vbdataio.c */
VB_HIDDEN extern int ivbdataread (const int ihandle, VB_CHAR * pcbuffer,
								  int *pideletedrow, off_t trownumber);
VB_HIDDEN extern int ivbdatawrite (const int ihandle, VB_CHAR * pcbuffer,
								   int ideletedrow, off_t trownumber);

/* vbindexio.c */
VB_HIDDEN extern off_t tvbnodecountgetnext (const int ihandle);
VB_HIDDEN extern int ivbnodefree (const int ihandle, off_t tnodenumber);
VB_HIDDEN extern int ivbdatafree (const int ihandle, off_t trownumber);
VB_HIDDEN extern off_t tvbnodeallocate (const int ihandle);
VB_HIDDEN extern off_t tvbdataallocate (const int ihandle);
VB_HIDDEN extern int ivbforcedataallocate (const int ihandle, off_t trownumber);
VB_HIDDEN extern int ivbreaddict (const int ihandle);
VB_HIDDEN extern int ivbwritedict (const int ihandle);

/* vbkeysio.c */
VB_HIDDEN extern void vvbmakekey (const struct keydesc *pskeydesc,
								  VB_CHAR * pcrow_buffer, VB_UCHAR * pckeyvalue);
VB_HIDDEN extern int ivbkeysearch (const int ihandle, const int imode,
								   const int ikeynumber, int ilength,
								   VB_UCHAR * pckeyvalue, off_t tdupnumber);
VB_HIDDEN extern int ivbkeylocaterow (const int ihandle, const int ikeynumber,
									  off_t trownumber);
VB_HIDDEN extern int ivbkeyload (const int ihandle, const int ikeynumber,
								 const int imode, const int isetcurr,
								 struct VBKEY **ppskey);
VB_HIDDEN extern void vvbkeyvalueset (const int ihigh, struct keydesc *pskeydesc,
									  VB_UCHAR * pckeyvalue, int quadsize);
VB_HIDDEN extern int ivbkeyinsert (const int ihandle, struct VBTREE *pstree,
								   const int ikeynumber, VB_UCHAR * pckeyvalue,
								   off_t trownode, off_t tdupnumber,
								   struct VBTREE *pschild);
VB_HIDDEN extern int ivbkeydelete (const int ihandle, const int ikeynumber);
VB_HIDDEN extern int ivbkeycompare (const int ihandle, const int ikeynumber,
									int ilength, VB_UCHAR * pckey1,
									VB_UCHAR * pckey2, int quadsize);
#ifdef	VBDEBUG
VB_HIDDEN extern int idumptree (int ihandle, int ikeynumber);
VB_HIDDEN extern int ichktree (int ihandle, int ikeynumber);
#endif /* VBDEBUG */

/* vblocking.c */
extern void vbsetstatus (void);
extern void vbclrstatus (void);
VB_HIDDEN extern void vbdatfilename (char *withdat, char *filename);
VB_HIDDEN extern int ivbenter (const int ihandle, const int imodifying);
VB_HIDDEN extern int ivbexit (const int ihandle);
VB_HIDDEN extern int ivbfileopenlock (const int ihandle, const int imode);
VB_HIDDEN extern int ivbdatalock (const int ihandle, const int imode,
								  off_t trownumber);

/* vblowlovel.c */
extern int  ivbopen (VB_CHAR * pcfilename, const int iflags, const mode_t tmode);
VB_HIDDEN extern int ivbclose (const int ihandle);
VB_HIDDEN extern off_t tvblseek (const int ihandle, off_t toffset,
								 const int iwhence);

#ifdef	VBDEBUG
VB_HIDDEN extern ssize_t tvbread (const int ihandle, void *pvbuffer,
								  const size_t tcount);
VB_HIDDEN extern ssize_t tvbwrite (const int ihandle, void *pvbuffer,
								   const size_t tcount);
#else
#define	tvbread(x,y,z)	read(vb_rtd->svbfile[(x)].ihandle,(void *)(y),(size_t)(z))
#define	tvbwrite(x,y,z)	write(vb_rtd->svbfile[(x)].ihandle,(void *)(y),(size_t)(z))
#endif

VB_HIDDEN extern int ivbblockread (const int ihandle, const int iisindex,
								   off_t tblocknumber, VB_CHAR * cbuffer);
VB_HIDDEN extern int ivbblockwrite (const int ihandle, const int iisindex,
									off_t tblocknumber, VB_CHAR * cbuffer);
VB_HIDDEN extern int ivblock (const int ihandle, off_t toffset, off_t tlength,
							  const int imode);

/* vbmemio.c */
VB_HIDDEN extern struct VBLOCK *psvblockallocate (const int ihandle);
VB_HIDDEN extern void vvblockfree (struct VBLOCK *pslock);
VB_HIDDEN extern struct VBTREE *psvbtreeallocate (const int ihandle);
VB_HIDDEN extern void vvbtreeallfree (const int ihandle, const int ikeynumber,
									  struct VBTREE *pstree);
VB_HIDDEN extern struct VBKEY *psvbkeyallocate (const int ihandle,
												const int ikeynumber);
VB_HIDDEN extern void vvbkeyallfree (const int ihandle, const int ikeynumber,
									 struct VBTREE *pstree);
VB_HIDDEN extern void vvbkeyfree (const int ihandle, const int ikeynumber,
								  struct VBKEY *pskey);
VB_HIDDEN extern void vvbkeyunmalloc (const int ihandle, const int ikeynumber);
VB_HIDDEN extern void vvbfreedict (struct DICTINFO *tvbptr);
#ifdef	VBDEBUG
VB_HIDDEN extern void *pvvbmalloc (const size_t tlength);
VB_HIDDEN extern void vvbfree (void *pvpointer, size_t tlength);
VB_HIDDEN extern void vvbunmalloc (void);
#else
#define	vvbfree(x,y)	free((void *)(x))
#define	pvvbmalloc(x)	calloc(1, (size_t)(x))
#endif /* VBDEBUG */

/* vbnodememio.c */
VB_HIDDEN extern int ivbnodeload (const int ihandle, const int ikeynumber,
								  struct VBTREE *pstree, off_t tnodenumber,
								  int iprevlvl);
VB_HIDDEN extern int ivbnodesave (const int ihandle, const int ikeynumber,
								  struct VBTREE *pstree, off_t tnodenumber,
								  int imode, int iposn);

#endif /* VB_LIBVBISAM_H */
