/*
 * Copyright (C) 2003 Trevor van Bremen
 * Copyright (C) 2020 Ron Norman
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

#include        "isinternal.h"

int         iserrno = 0;
int         iserrio = 0;
int         isreclen = 0;
int         ispid = 0;			/* Process Id blocking a record lock */
unsigned long isrecnum = 0;
char        isstat1 = '0';
char        isstat2 = '0';
char        isstat3 = '0';
char        isstat4 = '0';
const char *is_errlist[] = {
	"Duplicate record",				   /* 100 */
	"File not open",				   /* 101 */
	"Illegal argument",				   /* 102 */
	"Bad key descriptor",			   /* 103 */
	"Too many files",				   /* 104 */
	"Corrupted isam file",			   /* 105 */
	"Need exclusive access",		   /* 106 */
	"Record or file locked",		   /* 107 */
	"Index already exists",			   /* 108 */
	"Illegal primary key operation",   /* 109 */
	"End of file",					   /* 110 */
	"Record not found",				   /* 111 */
	"No current record",			   /* 112 */
	"File is in use",				   /* 113 */
	"File name too long",			   /* 114 */
	"Bad lock device",				   /* 115 */
	"Can't allocate memory",		   /* 116 */
	"Bad collating table",			   /* 117 */
	"Can't read log record",		   /* 118 */
	"Bad log record",				   /* 119 */
	"Can't open log file",			   /* 120 */
	"Can't write log record",		   /* 121 */
	"No transaction",				   /* 122 */
	"No shared memory",				   /* 123 */
	"No begin work yet",			   /* 124 */
	"Can't use nfs",				   /* 125 */
	"Bad rowid",					   /* 126 */
	"No primary key",				   /* 127 */
	"No logging",					   /* 128 */
	"Too many users",				   /* 129 */
	"No such dbspace",				   /* 130 */
	"No free disk space",			   /* 131 */
	"Rowsize too big",				   /* 132 */
	"Audit trail exists",			   /* 133 */
	"No more locks",				   /* 134 */
	"Library expired",				   /* 135 */
	"No remote connection",			   /* 136 */
	"Number of clients exceeded",	   /* 137 */
	"Auto repair in progress",		   /* 138 */
	"Invald IP specified",			   /* 139 */
	"No schema defined",			   /* 140 */
	"Schema already defined",		   /* 141 */
	"Function not available"		   /* 142 */
};

void
vbclrstatus (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	errno = 0;
	iserrno = vb_rtd->iserrno = 0;
	iserrio = vb_rtd->iserrio = 0;
	ispid = vb_rtd->ispid = 0;
	isstat1 = isstat2 = '0';
	isstat3 = isstat4 = '0';
	if (isrecnum > 0)
		vb_rtd->isrecnum = isrecnum;
	if (isreclen > 0)
		vb_rtd->isreclen = isreclen;
	vb_rtd->isstat1 = vb_rtd->isstat2 = '0';
	vb_rtd->isstat3 = vb_rtd->isstat4 = '0';
}

#define STAT0 '0'
#define STAT1 '1'
#define STAT2 '2'
#define STAT3 '3'
#define STAT5 '5'
#define STAT6 '6'
#define STAT9 '9'
void
vbsetstatus (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	iserrno = vb_rtd->iserrno;
	iserrio = vb_rtd->iserrio;
	ispid = vb_rtd->ispid;
	isrecnum = vb_rtd->isrecnum;
	isreclen = vb_rtd->isreclen;
	isstat1 = isstat2 = '0';
	isstat3 = isstat4 = '0';

	switch (iserrno) {
	case EDUPL:
		isstat1 = STAT2;
		isstat2 = STAT2;
		break;
	case EEXIST:
	case EBADFILE:
		isstat1 = STAT3;
		isstat2 = STAT0;
		break;
	case ENOENT:
		isstat1 = STAT3;
		isstat2 = STAT5;
		break;
	case ENOTOPEN:
		isstat1 = STAT9;
		isstat2 = STAT0;
		break;
	case EBADARG:
		if (iserrno == 0)
			iserrno = EBADARG;
		isstat1 = STAT9;
		isstat2 = STAT0;
		break;
	case EBADKEY:
	case ETOOMANY:
	case ENOTEXCL:
	case ELOCKED:
	case EKEXISTS:
	case EPRIMKEY:
	case EBADMEM:
	case EFNAME:
		isstat1 = STAT9;
		isstat2 = STAT0;
		break;
	case EFLOCKED:
		isstat1 = STAT6;
		isstat2 = STAT1;
		break;
	case EENDFILE:
		isstat1 = STAT1;
		isstat2 = STAT0;
		break;
	case ENOREC:
		isstat1 = STAT2;
		isstat2 = STAT3;
		break;
	case ENOCURR:
		isstat1 = STAT2;
		isstat2 = STAT1;
		break;
	case ELOGREAD:
	case EBADLOG:
	case ELOGOPEN:
	case ELOGWRIT:
	case ENOTRANS:
	case ENOBEGIN:
	case ENOPRIM:
	case ENOLOG:
	case ENOFREE:
	case EROWSIZE:
	case EAUDIT:
	case ENOLOCKS:
		isstat1 = STAT9;
		isstat2 = STAT0;
		break;
	default:
		isstat1 = vb_rtd->isstat1;
		isstat2 = vb_rtd->isstat2;
		isstat3 = vb_rtd->isstat3;
		isstat4 = vb_rtd->isstat4;
		break;
	}
}

static char *dat = NULL;
void
vbdatfilename (char *withdat, char *filename)
{
	char       *pctemp;
	if (dat == NULL) {
		if ((pctemp = getenv ("TIPISAMDAT")) != NULL) {	/* Inglenet */
			if (*pctemp != 'N') {
				dat = (char *) ".dat";
			} else {
				dat = (char *) "";
			}
		} else if ((pctemp = getenv ("ISDATEXT")) == NULL) {	/* D-ISAM */
			dat = (char *) ".dat";
		} else {
			dat = pctemp;
		}
	}
	sprintf ((char *) withdat, "%s%s", filename, dat);
}

#define IVBBUFFERLEVEL  4

/* Local functions */

/* the following function is part of a patch by Sergey */
static int
ilockexist (const int ihandle, const off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	struct VBLOCK *pslock;
	int         iindexhandle;

	psvbptr = vb_rtd->psvbfile[ihandle];
	iindexhandle = psvbptr->iindexhandle;
	pslock = vb_rtd->svbfile[iindexhandle].pslockhead;
	/* Head of list */
	if (pslock == NULL || trownumber < pslock->trownumber) {
		return 0;
	}
	/* Tail of list */
	if (trownumber > vb_rtd->svbfile[iindexhandle].pslocktail->trownumber) {
		return 0;
	}
	/* Position pslock to insertion point (Keep in mind, we insert AFTER) */
	while (pslock->psnext && trownumber >= pslock->psnext->trownumber) {
		pslock = pslock->psnext;
	}
	/* Already locked? */
	if (pslock->trownumber == trownumber) {
		if (pslock->ihandle == ihandle) {
			return 1;
		}
	}
	return 0;
}


static int
ilockinsert (const int ihandle, off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	struct VBLOCK *psnewlock = NULL, *pslock;
	int         iindexhandle;

	psvbptr = vb_rtd->psvbfile[ihandle];
	iindexhandle = psvbptr->iindexhandle;
	pslock = vb_rtd->svbfile[iindexhandle].pslockhead;
	/* Insertion at head of list */
	if (pslock == NULL || trownumber < pslock->trownumber) {
		psnewlock = psvblockallocate (ihandle);
		if (psnewlock == NULL) {
			return errno;
		}
		psnewlock->ihandle = ihandle;
		psnewlock->trownumber = trownumber;
		psnewlock->psnext = pslock;
		vb_rtd->svbfile[iindexhandle].pslockhead = psnewlock;
		if (vb_rtd->svbfile[iindexhandle].pslocktail == NULL) {
			vb_rtd->svbfile[iindexhandle].pslocktail = psnewlock;
		}
		return 0;
	}
	/* Insertion at tail of list */
	if (trownumber > vb_rtd->svbfile[iindexhandle].pslocktail->trownumber) {
		psnewlock = psvblockallocate (ihandle);
		if (psnewlock == NULL) {
			return errno;
		}
		psnewlock->ihandle = ihandle;
		psnewlock->trownumber = trownumber;
		vb_rtd->svbfile[iindexhandle].pslocktail->psnext = psnewlock;
		vb_rtd->svbfile[iindexhandle].pslocktail = psnewlock;
		return 0;
	}
	/* Position pslock to insertion point (Keep in mind, we insert AFTER) */
	while (pslock->psnext && trownumber > pslock->psnext->trownumber) {
		pslock = pslock->psnext;
	}
	/* Already locked? */
	if (pslock->trownumber == trownumber) {
		if (pslock->ihandle == ihandle) {
			return 0;
		} else {
#ifdef  CISAMLOCKS
			return 0;
#else /* CISAMLOCKS */
			return ELOCKED;
#endif /* CISAMLOCKS */
		}
	}
	psnewlock = psvblockallocate (ihandle);
	if (psnewlock == NULL) {
		return errno;
	}
	psnewlock->ihandle = ihandle;
	psnewlock->trownumber = trownumber;
	/* Insert psnewlock AFTER pslock */
	psnewlock->psnext = pslock->psnext;
	pslock->psnext = psnewlock;

	return 0;
}

static int
ilockdelete (const int ihandle, off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	struct VBLOCK *pslocktodelete;
	struct VBLOCK *pslock;
	int         iindexhandle;

	psvbptr = vb_rtd->psvbfile[ihandle];
	iindexhandle = psvbptr->iindexhandle;
	pslock = vb_rtd->svbfile[iindexhandle].pslockhead;
	/* Sanity check #1.  If it wasn't locked, ignore it! */
	if (!pslock || pslock->trownumber > trownumber) {
		return 0;
	}
	/* Check if deleting first entry in list */
	if (pslock->trownumber == trownumber) {
#ifndef CISAMLOCKS
		if (pslock->ihandle != ihandle) {
			return ELOCKED;
		}
#endif /* CISAMLOCKS */
		vb_rtd->svbfile[iindexhandle].pslockhead = pslock->psnext;
		if (!vb_rtd->svbfile[iindexhandle].pslockhead) {
			vb_rtd->svbfile[iindexhandle].pslocktail = NULL;
		}
		vvblockfree (pslock);
		return 0;
	}
	/* Position pslock pointer to previous */
	while (pslock->psnext && pslock->psnext->trownumber < trownumber) {
		pslock = pslock->psnext;
	}
	/* Sanity check #2 */
	if (!pslock->psnext || pslock->psnext->trownumber != trownumber) {
		return 0;
	}
	pslocktodelete = pslock->psnext;
#ifndef CISAMLOCKS
	if (pslocktodelete->ihandle != ihandle) {
		return ELOCKED;
	}
#endif /* CISAMLOCKS */
	pslock->psnext = pslocktodelete->psnext;
	if (pslocktodelete == vb_rtd->svbfile[iindexhandle].pslocktail) {
		vb_rtd->svbfile[iindexhandle].pslocktail = pslock;
	}
	vvblockfree (pslocktodelete);

	return 0;
}

/* Global functions */

/*
 *      Overview
 *      ========
 *      Ideally, I'd prefer using making use of the high bit for the file open
 *      locks and the row locks.  However, this is NOT allowed by Linux.
 *
 *      After a good deal of deliberation (followed by some snooping with good
 *      old strace), I've decided to make the locking strategy 100% compatible
 *      with Informix(R) CISAM 7.24UC2 as it exists for Linux.
 *      When used in 64-bit mode, it simply extends the values accordingly.
 *
 *      Index file locks (ALL locks are on the index file)
 *      ==================================================
 *      Non exclusive file open lock
 *              Off:0x7fffffff  Len:0x00000001  Typ:RDLOCK
 *      Exclusive file open lock (i.e. ISEXCLLOCK)
 *              Off:0x7fffffff  Len:0x00000001  Typ:WRLOCK
 *      Enter a primary function - Non modifying
 *              Off:0x00000000  Len:0x3fffffff  Typ:RDLCKW
 *      Enter a primary function - Modifying
 *              Off:0x00000000  Len:0x3fffffff  Typ:WRLCKW
 *      Lock a data row (Add the row number to the offset)
 *              Off:0x40000000  Len:0x00000001  Typ:WRLOCK
 *      Lock *ALL* the data rows (i.e. islock is called)
 *              Off:0x40000000  Len:0x3fffffff  Typ:WRLOCK
 */

int
ivbenter (const int ihandle, const int imodifying)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tlength, toffset;
	int         ilockmode, iloop, iresult;

	vbclrstatus ();
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!psvbptr) {
		vb_rtd->iserrno = ENOTOPEN;
		vbsetstatus ();
		return -1;
	}
	for (iloop = 0; iloop < MAXSUBS; iloop++) {
		if (psvbptr->pskeycurr[iloop]
		 && psvbptr->pskeycurr[iloop]->trownode == -1) {
			psvbptr->pskeycurr[iloop] = NULL;
		}
	}
	vb_rtd->iserrno = 0;
	if (psvbptr->iisopen
	 && vb_rtd->ivbintrans != VBCOMMIT 
	 && vb_rtd->ivbintrans != VBROLLBACK) {
		vb_rtd->iserrno = ENOTOPEN;
		vbsetstatus ();
		return -1;
	}
	if ((psvbptr->iopenmode & ISTRANS)
	 && vb_rtd->ivbintrans == VBNOTRANS) {
		if (vb_rtd->ivblogfilehandle < 0) {
			vb_rtd->iserrno = ENOTRANS;
			vbsetstatus ();
			return -1;
		}
		vb_rtd->ivbintrans = VBBEGIN;  /* Just flag that we've BEGUN */
	}
	psvbptr->iindexchanged = 0;
	if (psvbptr->iopenmode & ISEXCLLOCK) {
		psvbptr->iisdictlocked |= 0x01;
		return 0;
	}
	if (psvbptr->iisdictlocked & 0x03) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}

	if (imodifying) {
		if (psvbptr->iisopen == 0 && (psvbptr->iopenmode & 0x03) == ISINPUT) {	/* OPEN INPUT */
			ilockmode = VBRDLOCK;
		} else {
			ilockmode = VBWRLOCK;
		}
	} else {
		ilockmode = VBRDLOCK;
	}
	toffset = VBLKFILEOFF;
	tlength = 1;
	psvbptr->iindexchanged = 0;
	if (!(psvbptr->iopenmode & ISEXCLLOCK)) {
		iresult = ivblock (psvbptr->iindexhandle, toffset, tlength, ilockmode);
		if (iresult) {
			vb_rtd->iserrno = EFLOCKED;
			vbsetstatus ();
			return -1;
		}
		psvbptr->iisdictlocked |= 0x01;
		iresult = ivbreaddict (ihandle);
		if (iresult) {
			psvbptr->iisdictlocked = 0;
			ivbexit (ihandle);
			vb_rtd->iserrno = EBADFILE;
			ivblock (psvbptr->iindexhandle, toffset, tlength, VBUNLOCK);
			vbsetstatus ();
			return -1;
		}
	}
	psvbptr->iisdictlocked |= 0x01;
/*
 * If we're in C-ISAM mode, then there is NO way to determine if a given node
 * has been updated by some other process.  Thus *ANY* change to the index
 * file needs to invalidate the ENTIRE cache for that table!!!
 * If we're in VBISAM 64-bit mode, then each node in the index table has a
 * stamp on it stating the transaction number when it was updated.  If this
 * stamp is BELOW that of the current transaction number, we know that the
 * associated VBTREE / VBKEY linked lists are still coherent!
 */
	if (psvbptr->ttranslast != psvbptr->itransnumber) {
		for (iloop = 0; iloop < MAXSUBS; iloop++) {
			if (psvbptr->pstree[iloop]) {
				vvbtreeallfree (ihandle, iloop, psvbptr->pstree[iloop]);
			}
			psvbptr->pstree[iloop] = NULL;
		}
	}
	return 0;
}

int
ivbexit (const int ihandle)
{
	struct DICTINFO *psvbptr;
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct VBKEY *pskey, *pskeycurr;
	off_t       tlength, toffset, ttransnumber;
	int         iresult = 0, iloop, iloop2, isaveerror;

	isaveerror = vb_rtd->iserrno;
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!psvbptr || (!psvbptr->iisdictlocked && !(psvbptr->iopenmode & ISEXCLLOCK))) {
		if (vb_rtd->iserrno != EFLOCKED)
			vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	ttransnumber = psvbptr->itransnumber;
	psvbptr->ttranslast = ttransnumber;
	if (psvbptr->iopenmode & ISEXCLLOCK) {
		vbsetstatus ();
		return 0;
	}
	if (psvbptr->iisdictlocked & 0x02) {
		if (!(psvbptr->iisdictlocked & 0x04)) {
			psvbptr->ttranslast = ttransnumber + 1;
			psvbptr->itransnumber = ttransnumber + 1;
		}
		iresult = ivbwritedict (ihandle);
		if (iresult) {
			vb_rtd->iserrno = EBADFILE;
		} else {
			vb_rtd->iserrno = 0;
		}
	}
	toffset = VBLKBUSYOFF;
	tlength = VBLKBUSYLEN;
	if (ivblock (psvbptr->iindexhandle, toffset, tlength, VBUNLOCK)) {
		vb_rtd->iserrno = errno;
		vbsetstatus ();
		return -1;
	}
	psvbptr->iisdictlocked = 0;
	/* Free up any key/tree no longer wanted */
	for (iloop2 = 0; iloop2 < psvbptr->inkeys; iloop2++) {
		pskey = psvbptr->pskeycurr[iloop2];
		/*
		 * This is a REAL fudge factor...
		 * We simply free up all the dynamically allocated memory
		 * associated with non-current nodes above a certain level.
		 * In other words, we wish to keep the CURRENT key and all data
		 * in memory above it for IVBBUFFERLEVEL levels.
		 * Anything *ABOVE* that in the tree is to be purged with
		 * EXTREME prejudice except for the 'current' tree up to the
		 * root.
		 */
		for (iloop = 0; pskey && iloop < IVBBUFFERLEVEL; iloop++) {
			if (pskey->psparent->psparent) {
				pskey = pskey->psparent->psparent->pskeycurr;
			} else {
				pskey = NULL;
			}
		}
		if (!pskey) {
			vb_rtd->iserrno = isaveerror;
			vbsetstatus ();
			return 0;
		}
		while (pskey) {
			for (pskeycurr = pskey->psparent->pskeyfirst; pskeycurr;
				 pskeycurr = pskeycurr->psnext) {
				if (pskeycurr != pskey && pskeycurr->pschild) {
					vvbtreeallfree (ihandle, iloop2, pskeycurr->pschild);
					pskeycurr->pschild = NULL;
				}
			}
			if (pskey->psparent->psparent) {
				pskey = pskey->psparent->psparent->pskeycurr;
			} else {
				pskey = NULL;
			}
		}
	}
	if (iresult) {
		vbsetstatus ();
		return -1;
	}
	vb_rtd->iserrno = isaveerror;
	vbsetstatus ();
	return 0;
}

int
ivbfileopenlock (const int ihandle, const int imode)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       toffset, tlength = 1;
	int         ilocktype, iresult;

	/* Sanity check - Is ihandle a currently open table? */
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		return ENOTOPEN;
	}
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!psvbptr) {
		return ENOTOPEN;
	}

	toffset = VBLKFILEOFF;

	switch (imode) {
	case 0:
		ilocktype = VBUNLOCK;
		break;

	case 1:
		ilocktype = VBRDLOCK;
		break;

	case 2:
		ilocktype = VBWRLOCK;
		iresult = ivbreaddict (ihandle);
		break;

	default:
		return EBADARG;
	}

	iresult = ivblock (psvbptr->iindexhandle, toffset, tlength, ilocktype);
	if (iresult != 0) {
		return EFLOCKED;
	}

	return 0;
}

int
ivbdatalock (const int ihandle, const int imode, off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	struct VBLOCK *pslock;
	off_t       tlength = 1, toffset;
	int         iresult = 0;

	/* Sanity check - Is ihandle a currently open table? */
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		return ENOTOPEN;
	}
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!psvbptr) {
		return ENOTOPEN;
	}
	if (psvbptr->iopenmode & ISEXCLLOCK) {
		return 0;
	}
	/*
	 * If this is a FILE (un)lock (row = 0), then we may as well free ALL
	 * locks. Even if CISAMLOCKS is set, we do this!
	 */
	if (trownumber == 0) {
		for (pslock = vb_rtd->svbfile[psvbptr->iindexhandle].pslockhead; pslock;
			 pslock = pslock->psnext) {
			ivbdatalock (ihandle, VBUNLOCK, pslock->trownumber);
		}
		tlength = VBLKDATALEN;
		if (imode == VBUNLOCK) {
			psvbptr->iisdatalocked = 0;
		} else {
			psvbptr->iisdatalocked = 1;
		}
	} else if (imode == VBUNLOCK) {
		iresult = ilockdelete (ihandle, trownumber);
	}
	if (!iresult && (imode == VBUNLOCK || !ilockexist (ihandle, trownumber))) {
		toffset = VBLKDATAOFF;
		iresult = ivblock (psvbptr->iindexhandle, toffset + trownumber,
						   tlength, imode);
		if (iresult != 0) {
			return ELOCKED;
		}
	}
	if ((imode != VBUNLOCK) && trownumber) {
		return ilockinsert (ihandle, trownumber);
	}
	return iresult;
}
