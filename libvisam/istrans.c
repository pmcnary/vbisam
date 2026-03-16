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

#include	"isinternal.h"

/* Globals */


/* Local functions */

#if (VBLOGGING == 1)
/* Only used if LOGGING is configured */
static void
vinitpiduid (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	if (vb_rtd->iinitialized) {
		return;
	}
	vb_rtd->iinitialized = 1;
#ifdef	_WIN32
	vb_rtd->tvbpid = GetCurrentProcessId ();
	/* No getuid ? */
	vb_rtd->tvbuid = 1;
#else
	vb_rtd->tvbpid = getpid ();
	vb_rtd->tvbuid = getuid ();
#endif
}

/*
 * Name:
 *	static	void	vtranshdr (VB_CHAR *pctranstype);
 * Arguments:
 *	VB_CHAR	*pctranstype
 *		The two character transaction type.  One of:
 *			VBL_BUILD
 *			VBL_BEGIN
 *			VBL_CREINDEX
 *			VBL_CLUSTER
 *			VBL_COMMIT
 *			VBL_DELETE
 *			VBL_DELINDEX
 *			VBL_FILEERASE
 *			VBL_FILECLOSE
 *			VBL_FILEOPEN
 *			VBL_INSERT
 *			VBL_RENAME
 *			VBL_ROLLBACK
 *			VBL_SETUNIQUE
 *			VBL_UNIQUEID
 *			VBL_UPDATE
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	None known
 */
static void
vtranshdr (const VB_CHAR * pctranstype)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	vinitpiduid ();
	vb_rtd->psvblogheader = (struct SLOGHDR *) vb_rtd->cvbtransbuffer;
	memcpy (vb_rtd->psvblogheader->coperation, pctranstype, 2);
	inl_stint ((int) vb_rtd->tvbpid, vb_rtd->psvblogheader->cpid);	/* Assumes pid_t is short */
	inl_stint ((int) vb_rtd->tvbuid, vb_rtd->psvblogheader->cuid);	/* Assumes uid_t is short */
	inl_stlong (time (NULL), vb_rtd->psvblogheader->ctime);	/* Assumes time_t is long */
	inl_stint (0, vb_rtd->psvblogheader->crfu1);	/* BUG - WTF is this? */
}

/*
 * Name:
 *	static	int	iwritetrans (int itranslength, int irollback);
 * Arguments:
 *	int	itranslength
 *		The length of the transaction to write (exluding hdr/ftr)
 *	int	irollback
 *		0
 *			This transaction CANNOT be rolled back!
 *		1
 *			Take a wild guess!
 * Prerequisites:
 *	NONE
 * Returns:
 *	0
 *		Success
 *	ELOGWRIT
 *		Ooops, some problem occurred!
 * Problems:
 *	FUTURE
 *	When we begin to support rows > 32k, the buffer is too small.
 *	In that case, we'll need to perform SEVERAL writes and this means we
 *	will need to implement a crude locking scheme to guarantee atomicity.
 */
static int
iwritetrans (int itranslength, const int irollback)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;

#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
#else
	int         iresult;

	itranslength += sizeof (struct SLOGHDR) + INTSIZE;
	inl_stint (itranslength, vb_rtd->cvbtransbuffer);
	inl_stint (itranslength, vb_rtd->cvbtransbuffer + itranslength - INTSIZE);
	iresult = ivblock (vb_rtd->ivblogfilehandle, (off_t) 0, (off_t) 0, VBWRLCKW);
	if (iresult) {
		return ELOGWRIT;
	}
	vb_rtd->psvblogheader = (struct SLOGHDR *) vb_rtd->cvbtransbuffer;
	if (irollback) {
		inl_stint ((int) vb_rtd->toffset, vb_rtd->psvblogheader->clastposn);
		inl_stint (vb_rtd->iprevlen, vb_rtd->psvblogheader->clastlength);
		vb_rtd->toffset = tvblseek (vb_rtd->ivblogfilehandle, (off_t) 0, SEEK_END);
		if (vb_rtd->toffset == -1) {
			return ELOGWRIT;
		}
		vb_rtd->iprevlen = itranslength;
	} else {
		inl_stint (0, vb_rtd->psvblogheader->clastposn);
		inl_stint (0, vb_rtd->psvblogheader->clastlength);
		if (tvblseek (vb_rtd->ivblogfilehandle, (off_t) 0, SEEK_END) == -1) {
			return ELOGWRIT;
		}
	}
	if (tvbwrite
		(vb_rtd->ivblogfilehandle, (void *) vb_rtd->cvbtransbuffer,
		 (size_t) itranslength) != (ssize_t) itranslength) {
		return ELOGWRIT;
	}
	iresult = ivblock (vb_rtd->ivblogfilehandle, (off_t) 0, (off_t) 0, VBUNLOCK);
	if (iresult) {
		return ELOGWRIT;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		vb_rtd->ivbintrans = VBNEEDFLUSH;
	}
#endif
	return 0;
}

/*
 * Name:
 *	static	int	iwritebegin (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	0
 *		Success
 *	ELOGWRIT
 *		Ooops, some problem occurred!
 * Problems:
 *	None known
 */
static int
iwritebegin (void)
{
	vtranshdr ((VB_CHAR *) VBL_BEGIN);
	return iwritetrans (0, 1);
}

/*
 * Name:
 *	static	int	idemotelocks (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	0
 *		Success
 *	EBADFILE
 *		Ooops, some problem occurred!
 * Problems:
 *	See comments
 * Comments:
 *	P1-3 FIX: Only release locks on files actually touched during this
 *	transaction (itransyet != 0).  Files with pre-existing record locks
 *	that were not part of the transaction are left intact — prevents
 *	isrollback() / iscommit() from releasing locks the caller still needs.
 *	Limitation: a file locked BEFORE isbegin() AND written during the
 *	transaction will still have its locks released. A full per-row lock
 *	registry would be required to handle that case precisely.
 * Caveat:
 *	Files exclusively opened (ISEXCLLOCK) or locked with islock() remain
 *	locked regardless (iisdatalocked guard below).
 */
static int
idemotelocks (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	int         ihandle, iresult = 0;

	for (ihandle = 0; ihandle <= vb_rtd->ivbmaxusedhandle; ihandle++) {
		psvbptr = vb_rtd->psvbfile[ihandle];
		if (!psvbptr || psvbptr->iisopen == 2) {
			continue;
		}
		if (psvbptr->iopenmode & ISEXCLLOCK) {
			continue;
		}
		if (psvbptr->iisdatalocked) {
			continue;
		}
		/* P1-3 FIX: only release locks on files touched in this transaction.
		 * itransyet == 0 means no write/lock occurred during isbegin()..now,
		 * so pre-existing record locks on this file are left in place. */
		if (psvbptr->itransyet == 0) {
			continue;
		}
		if (ivbdatalock (ihandle, VBUNLOCK, (off_t) 0)) {
			iresult = -1;
		}
	}
	if (iresult) {
		vb_rtd->iserrno = EBADFILE;
	}
	return iresult;
}

/*
 * Name:
 *	int	ivbrollmeback (off_t toffset, int iinrecover);
 * Arguments:
 *	off_t	toffset
 *		The position within the logfile to begin the backwards scan
 *		It is assumed that this is at a true transaction 'boundary'
 *	int	iinrecover
 *		0	Nope
 *		1	Yep, we need to force-allocate the data rows
 * Prerequisites:
 *	NONE
 * Returns:
 *	0
 *		Success
 *	OTHER
 *		Ooops, some problem occurred!
 * Problems:
 *	None known
 */
static int
ivbrollmeback (off_t toffset, const int iinrecover)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	VB_CHAR    *pcbuffer, *pcrow;
	off_t       tlength, trownumber;
	int         ierrorencountered = 0;
	int         ifoundbegin = 0;
	int         ihandle, iloop;
	int         ilocalhandle[VB_MAX_FILES + 1], isavedhandle[VB_MAX_FILES + 1];

	for (iloop = 0; iloop <= VB_MAX_FILES; iloop++) {
		if (vb_rtd->psvbfile[iloop]) {
			ilocalhandle[iloop] = iloop;
		} else {
			ilocalhandle[iloop] = -1;
		}
		isavedhandle[iloop] = ilocalhandle[iloop];
	}
	vb_rtd->psvblogheader = (struct SLOGHDR *) (vb_rtd->cvbtransbuffer + INTSIZE);
	pcbuffer = vb_rtd->cvbtransbuffer + INTSIZE + sizeof (struct SLOGHDR);
	/* Begin by reading the footer of the previous transaction */
	toffset -= INTSIZE;
	if (tvblseek (vb_rtd->ivblogfilehandle, toffset, SEEK_SET) != toffset) {
		return EBADFILE;
	}
	if (tvbread (vb_rtd->ivblogfilehandle, vb_rtd->cvbtransbuffer, INTSIZE) !=
		INTSIZE) {
		return EBADFILE;
	}
	/* Now, recurse backwards */
	while (!ifoundbegin) {
		tlength = inl_ldint (vb_rtd->cvbtransbuffer);
		if (!tlength) {
			return EBADFILE;
		}
		toffset -= tlength;
		/* Special case: Handle where the FIRST log entry is our BW */
		if (toffset == -(INTSIZE)) {
			if (tvblseek (vb_rtd->ivblogfilehandle, (off_t) 0, SEEK_SET) != 0) {
				return EBADFILE;
			}
			if (tvbread (vb_rtd->ivblogfilehandle, vb_rtd->cvbtransbuffer + INTSIZE,
						 tlength - INTSIZE) != tlength - INTSIZE) {
				return EBADFILE;
			}
			if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_BEGIN, 2)) {
				break;
			}
			return EBADFILE;
		} else {
			if (toffset < INTSIZE) {
				return EBADFILE;
			}
			if (tvblseek (vb_rtd->ivblogfilehandle, toffset, SEEK_SET) != toffset) {
				return EBADFILE;
			}
			if (tvbread (vb_rtd->ivblogfilehandle, vb_rtd->cvbtransbuffer, tlength)
				!= tlength) {
				return EBADFILE;
			}
		}
		/* Is it OURS? */
		if (inl_ldint (vb_rtd->psvblogheader->cpid) != vb_rtd->tvbpid) {
			continue;
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_BEGIN, 2)) {
			break;
		}
		ihandle = inl_ldint (pcbuffer);
		trownumber = ld_compx8 (pcbuffer + INTSIZE);
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_FILECLOSE, 2)) {
			if (ilocalhandle[ihandle] != -1
				&& vb_rtd->psvbfile[ihandle]->iisopen == 0) {
				return EBADFILE;
			}
			ilocalhandle[ihandle] =
				isopen (pcbuffer + INTSIZE + INTSIZE, ISMANULOCK + ISINOUT);
			if (ilocalhandle[ihandle] == -1) {
				return ETOOMANY;
			}
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_INSERT, 2)) {
			if (ilocalhandle[ihandle] == -1) {
				return EBADFILE;
			}
			if (isdelrec (ilocalhandle[ihandle], trownumber)) {
				return vb_rtd->iserrno;
			}
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_UPDATE, 2)) {
			if (ilocalhandle[ihandle] == -1) {
				return EBADFILE;
			}
			{
				/* P1-4: Row verification before revert.
				 * Log layout (from ivbtransupdate):
				 *   [ihandle:INT][trownumber:QUAD][ioldrowlen:INT][inewrowlen:INT]
				 *   [old row bytes][new row bytes]
				 * Before writing back the old row, confirm the on-disk row
				 * still matches the "new" row we wrote during the transaction.
				 * A mismatch means another writer touched this row after our
				 * transaction committed its write — we cannot safely revert. */
				int ioldrowlen = inl_ldint (pcbuffer + INTSIZE + QUADSIZ8);
				int inewrowlen = inl_ldint (pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE);
				VB_CHAR *pcoldrow = pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE + INTSIZE;
				VB_CHAR *pcnewrow = pcoldrow + ioldrowlen;
				int ideleted = 0;
				VB_CHAR *cvbverify = malloc ((size_t)(inewrowlen + 1));
				if (!cvbverify) {
					return EBADFILE;
				}
				if (ivbdataread (ilocalhandle[ihandle], cvbverify, &ideleted, trownumber)
					|| ideleted
					|| memcmp (cvbverify, pcnewrow, (size_t)inewrowlen)) {
					/* Tampered or unreadable — skip this revert, flag corruption */
					free (cvbverify);
					ierrorencountered = EBADFILE;
				} else {
					free (cvbverify);
					vb_rtd->isreclen = ioldrowlen;
					pcrow = pcoldrow;
					if (isrewrec (ilocalhandle[ihandle], trownumber, pcrow)) {
						return vb_rtd->iserrno;
					}
				}
			}
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_DELETE, 2)) {
			if (ilocalhandle[ihandle] == -1) {
				return EBADFILE;
			}
			vb_rtd->isreclen = inl_ldint (pcbuffer + INTSIZE + QUADSIZ8);
			pcrow = pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE;
			ivbenter (ilocalhandle[ihandle], 1);
			vb_rtd->psvbfile[ilocalhandle[ihandle]]->iisdictlocked |= 0x02;
			if (iinrecover
				&& ivbforcedataallocate (ilocalhandle[ihandle], trownumber)) {
				ierrorencountered = EBADFILE;
			} else {
				if (ivbwriterow (ilocalhandle[ihandle], pcrow, trownumber)) {
					ierrorencountered = EDUPL;
					ivbdatafree (ilocalhandle[ihandle], trownumber);
				}
			}
			ivbexit (ilocalhandle[ihandle]);
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_FILEOPEN, 2)) {
			if (ilocalhandle[ihandle] == -1) {
				return EBADFILE;
			}
#if (VB_CLOSE_ON_TREN == 1)			   /* Why Close all ISAM files on a isrollback? */
			isclose (ilocalhandle[ihandle]);
#endif
			ilocalhandle[ihandle] = -1;
		}
	}
	for (iloop = 0; iloop <= VB_MAX_FILES; iloop++) {
		if (isavedhandle[iloop] != -1 && vb_rtd->psvbfile[isavedhandle[iloop]]) {
#if (VB_CLOSE_ON_TREN == 1)			   /* Why Close all ISAM files on a isrollback? */
			isclose (isavedhandle[iloop]);
#elif (VB_FLUSH_ON_TREN == 1)
			/* A flush to disk is wanted here */
			isflush (isavedhandle[ihandle]);
#endif
		}
	}
	return ierrorencountered;
}

/*
 * Name:
 *	int	ivbrollmeforward (off_t toffset);
 * Arguments:
 *	off_t	toffset
 *		The position within the logfile to begin the backwards scan
 *		It is assumed that this is at a true transaction 'boundary'
 * Prerequisites:
 *	NONE
 * Returns:
 *	0
 *		Success
 *	OTHER
 *		Ooops, some problem occurred!
 * Problems:
 *	None known
 */
static int
ivbrollmeforward (off_t toffset)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	VB_CHAR    *pcbuffer;
	off_t       tlength, trownumber;
	int         ifoundbegin = 0;
	int         ihandle, iloop;
	int         ilocalhandle[VB_MAX_FILES + 1], isavedhandle[VB_MAX_FILES + 1];

	vinitpiduid ();
	for (iloop = 0; iloop <= VB_MAX_FILES; iloop++) {
		if (vb_rtd->psvbfile[iloop]) {
			ilocalhandle[iloop] = iloop;
		} else {
			ilocalhandle[iloop] = -1;
		}
		isavedhandle[iloop] = ilocalhandle[iloop];
	}
	vb_rtd->psvblogheader = (struct SLOGHDR *) (vb_rtd->cvbtransbuffer + INTSIZE);
	pcbuffer = vb_rtd->cvbtransbuffer + INTSIZE + sizeof (struct SLOGHDR);
	/* Begin by reading the footer of the previous transaction */
	toffset -= INTSIZE;
	if (tvblseek (vb_rtd->ivblogfilehandle, toffset, SEEK_SET) != toffset) {
		return EBADFILE;
	}
	if (tvbread (vb_rtd->ivblogfilehandle, vb_rtd->cvbtransbuffer, INTSIZE) !=
		INTSIZE) {
		return EBADFILE;
	}
	/* Now, recurse backwards */
	while (!ifoundbegin) {
		tlength = inl_ldint (vb_rtd->cvbtransbuffer);
		if (!tlength) {
			return EBADFILE;
		}
		toffset -= tlength;
		/* Special case: Handle where the FIRST log entry is our BW */
		if (toffset == -(INTSIZE)) {
			if (tvblseek (vb_rtd->ivblogfilehandle, (off_t) 0, SEEK_SET) != 0) {
				return EBADFILE;
			}
			if (tvbread (vb_rtd->ivblogfilehandle, vb_rtd->cvbtransbuffer + INTSIZE,
						 tlength - INTSIZE) != tlength - INTSIZE) {
				return EBADFILE;
			}
			if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_BEGIN, 2)) {
				break;
			}
			return EBADFILE;
		} else {
			if (toffset < INTSIZE) {
				return EBADFILE;
			}
			if (tvblseek (vb_rtd->ivblogfilehandle, toffset, SEEK_SET) != toffset) {
				return EBADFILE;
			}
			if (tvbread (vb_rtd->ivblogfilehandle, vb_rtd->cvbtransbuffer, tlength)
				!= tlength) {
				return EBADFILE;
			}
		}
		/* Is it OURS? */
		if (inl_ldint (vb_rtd->psvblogheader->cpid) != vb_rtd->tvbpid) {
			continue;
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_BEGIN, 2)) {
			break;
		}
		ihandle = inl_ldint (pcbuffer);
		trownumber = ld_compx8 (pcbuffer + INTSIZE);
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_FILECLOSE, 2)) {
			if (ilocalhandle[ihandle] != -1) {
				return EBADFILE;
			}
			ilocalhandle[ihandle] =
				isopen (pcbuffer + INTSIZE + INTSIZE, ISMANULOCK + ISINOUT);
			if (ilocalhandle[ihandle] == -1) {
				return ETOOMANY;
			}
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_DELETE, 2)) {
			if (ilocalhandle[ihandle] == -1) {
				return EBADFILE;
			}
			ivbenter (ilocalhandle[ihandle], 1);
			vb_rtd->psvbfile[ilocalhandle[ihandle]]->iisdictlocked |= 0x02;
			if (trownumber == vb_rtd->psvbfile[ilocalhandle[ihandle]]->idatacount) {
				vb_rtd->psvbfile[ilocalhandle[ihandle]]->iisdictlocked |= 0x02;
				vb_rtd->psvbfile[ilocalhandle[ihandle]]->idatacount = trownumber - 1;
			} else if (ivbdatafree (ilocalhandle[ihandle], trownumber)) {
				ivbexit (ilocalhandle[ihandle]);
				return EBADFILE;
			}
			ivbexit (ilocalhandle[ihandle]);
		}
		if (!memcmp (vb_rtd->psvblogheader->coperation, VBL_FILEOPEN, 2)) {
			if (ilocalhandle[ihandle] == -1) {
				return EBADFILE;
			}
#if (VB_CLOSE_ON_TREN == 1)			   /* Why Close the ISAM file on iscommit? */
			isclose (ilocalhandle[ihandle]);
#elif (VB_FLUSH_ON_TREN == 1)
			/* A flush to disk is wanted here */
			isflush (ilocalhandle[ihandle]);
#endif
		}
	}
	for (iloop = 0; iloop <= VB_MAX_FILES; iloop++) {
		if (isavedhandle[iloop] != -1 && vb_rtd->psvbfile[isavedhandle[iloop]]) {
#if (VB_CLOSE_ON_TREN == 1)
			isclose (isavedhandle[iloop]);
#elif (VB_FLUSH_ON_TREN == 1)
			/* A flush to disk is wanted here */
			isflush (isavedhandle[iloop]);
#endif
		}
	}
	return 0;
}
#endif

/* Global functions */

/*
 * Name:
 *	int	isbegin (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
isbegin (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 1)
	vbclrstatus ();
	if (vb_rtd->ivblogfilehandle < 0) {
		vb_rtd->iserrno = ELOGOPEN;
		vbsetstatus ();
		return -1;
	}
	/* If we're already *IN* a transaction, don't start another! */
	if (vb_rtd->ivbintrans) {
		return 0;
	}
	vb_rtd->ivbintrans = VBBEGIN;	   /* Just flag that we've BEGUN */
	return 0;
#else
	vb_rtd->iserrno = ENOLOG;
	vbsetstatus ();
	return -1;
#endif
}

/*
 * Name:
 *	int	iscommit (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
iscommit (void)
{
#if (VBLOGGING == 1)
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       toffset;
	int         iholdstatus = vb_rtd->ivbintrans, iloop, iresult = 0;

	vbclrstatus ();
	if (vb_rtd->ivblogfilehandle == -1) {
		return 0;
	}
	if (!vb_rtd->ivbintrans) {
		vb_rtd->iserrno = ENOBEGIN;
		vbsetstatus ();
		return -1;
	}
	vinitpiduid ();
	vb_rtd->ivbintrans = VBCOMMIT;
	if (iholdstatus != VBBEGIN) {
		toffset = tvblseek (vb_rtd->ivblogfilehandle, (off_t) 0, SEEK_END);
		vb_rtd->iserrno = ivbrollmeforward (toffset);
	}
	for (iloop = 0; iloop <= vb_rtd->ivbmaxusedhandle; iloop++) {
		psvbptr = vb_rtd->psvbfile[iloop];
		if (psvbptr && psvbptr->iisopen == 1) {
			iresult = vb_rtd->iserrno;
			if (!ivbclose2 (iloop)) {
				vb_rtd->iserrno = iresult;
			}
		}
	}
	/* Don't write out a 'null' transaction! */
	if (iholdstatus != VBBEGIN) {
		vtranshdr ((VB_CHAR *) VBL_COMMIT);
		iresult = iwritetrans (0, 1);
		if (iresult) {
			vb_rtd->iserrno = iresult;
		}
		idemotelocks ();
	}
	vb_rtd->ivbintrans = VBNOTRANS;
	if (vb_rtd->iserrno) {
		vbsetstatus ();
		return -1;
	}
#endif
	vbsetstatus ();
	return 0;
}

/*
 * Name:
 *	int	islogclose (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
islogclose (void)
{
#if (VBLOGGING == 1)
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	int         iresult = 0;

	if (vb_rtd->ivbintrans == VBNEEDFLUSH) {
		if (isrollback ()) {
			iresult = vb_rtd->iserrno;
		}
	}
	vb_rtd->ivbintrans = VBNOTRANS;
	if (vb_rtd->ivblogfilehandle != -1) {
		if (ivbclose (vb_rtd->ivblogfilehandle)) {
			iresult = errno;
		}
	}
	vb_rtd->ivblogfilehandle = -1;
	return iresult;
#else
	return 0;
#endif
}

/*
 * Name:
 *	int	islogopen (VB_CHAR *pcfilename);
 * Arguments:
 *	VB_CHAR	*pcfilename
 *		The name of the log file
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
islogopen (VB_CHAR * pcfilename)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	vbclrstatus ();
#if (VBLOGGING == 1)
	if (vb_rtd->ivblogfilehandle != -1) {
		islogclose ();				   /* Ignore the return value! */
	}
	vb_rtd->ivblogfilehandle = ivbopen (pcfilename, O_RDWR | O_BINARY, 0);
	if (vb_rtd->ivblogfilehandle < 0) {
		vb_rtd->iserrno = ELOGOPEN;
		vbsetstatus ();
		return -1;
	}
	return 0;
#else
	vb_rtd->ivblogfilehandle = -1;
	vb_rtd->iserrno = ENOLOG;
	vbsetstatus ();
	return -1;
#endif
}

/*
 * Name:
 *	int	isrollback (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
isrollback (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	off_t       toffset;
	int         iloop, iresult = 0;

	vbclrstatus ();
	if (vb_rtd->ivblogfilehandle < 0) {
		return 0;
	}
	if (!vb_rtd->ivbintrans) {
		vb_rtd->iserrno = ENOBEGIN;
		vbsetstatus ();
		return -1;
	}
	vinitpiduid ();
	/* Don't write out a 'null' transaction! */
	for (iloop = 0; iloop <= vb_rtd->ivbmaxusedhandle; iloop++) {
		psvbptr = vb_rtd->psvbfile[iloop];
		if (psvbptr && psvbptr->iisopen == 1) {
			iresult = vb_rtd->iserrno;
			if (!ivbclose2 (iloop)) {
				vb_rtd->iserrno = iresult;
			}
			iresult = 0;
		}
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		vbsetstatus ();
		return 0;
	}
	vb_rtd->ivbintrans = VBROLLBACK;
	toffset = tvblseek (vb_rtd->ivblogfilehandle, (off_t) 0, SEEK_END);
	/* Write out the log entry */
	vtranshdr ((VB_CHAR *) VBL_ROLLBACK);
	vb_rtd->iserrno = iwritetrans (0, 1);
	if (!vb_rtd->iserrno) {
		vb_rtd->iserrno = ivbrollmeback (toffset, 0);
	}
	idemotelocks ();
	vb_rtd->ivbintrans = VBNOTRANS;
	if (vb_rtd->iserrno) {
		vbsetstatus ();
		return -1;
	}
	for (iloop = 0; iloop <= vb_rtd->ivbmaxusedhandle; iloop++) {
		psvbptr = vb_rtd->psvbfile[iloop];
		if (psvbptr && psvbptr->iisopen == 1) {
			if (ivbclose2 (iloop)) {
				iresult = vb_rtd->iserrno;
			}
		}
		if (psvbptr && psvbptr->iisdictlocked & 0x04) {
			iresult |= ivbexit (iloop);
		}
	}
	vbsetstatus ();
	return (iresult ? -1 : 0);
#endif
}

/*
 * Name:
 *	int	ivbtransbuild (const VB_CHAR *pcfilename, int iminrowlen, int imaxrowlen, struct keydesc *pskeydesc);
 * Arguments:
 *	VB_CHAR	*pcfilename
 *		The name of the file being created
 *	int	iminrowlen
 *		The minimum row length
 *	int	imaxrowlen
 *		The maximum row length
 *	struct	keydesc	*pskeydesc
 *		The primary index being created
 *	int	imode
 *		The open mode (Only used to determine is ISNOLOG is set)
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransbuild (const VB_CHAR * pcfilename, const int iminrowlen,
			   const int imaxrowlen, struct keydesc *pskeydesc, const int imode)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	VB_CHAR    *pcbuffer;
	int         ilength = 0, ilength2, iloop;

	if (vb_rtd->ivblogfilehandle < 0 || imode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	vtranshdr ((VB_CHAR *) VBL_BUILD);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (imode, pcbuffer);
	inl_stint (iminrowlen, pcbuffer + INTSIZE);
	inl_stint (imaxrowlen, pcbuffer + (2 * INTSIZE));
	inl_stint (pskeydesc->k_flags, pcbuffer + (3 * INTSIZE));
	inl_stint (pskeydesc->k_nparts, pcbuffer + (4 * INTSIZE));
	pcbuffer += (INTSIZE * 6);
	for (iloop = 0; iloop < pskeydesc->k_nparts; iloop++) {
		inl_stint (pskeydesc->k_part[iloop].kp_start,
				   pcbuffer + (iloop * 3 * INTSIZE));
		inl_stint (pskeydesc->k_part[iloop].kp_leng,
				   pcbuffer + INTSIZE + (iloop * 3 * INTSIZE));
		inl_stint (pskeydesc->k_part[iloop].kp_type,
				   pcbuffer + (INTSIZE * 2) + (iloop * 3 * INTSIZE));
		ilength += pskeydesc->k_part[iloop].kp_leng;
	}
	inl_stint (ilength, pcbuffer - INTSIZE);
	ilength = (INTSIZE * 6) + (INTSIZE * 3 * (pskeydesc->k_nparts));
	ilength2 = strlen ((char *) pcfilename) + 1;
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR) + ilength;
	memcpy (pcbuffer, pcfilename, (size_t) ilength2);
	vb_rtd->iserrno = iwritetrans (ilength + ilength2, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtranscreateindex (int ihandle, struct keydesc *pskeydesc);
 * Arguments:
 *	int	ihandle
 *		An (exclusively) open VBISAM table
 *	struct	keydesc	*pskeydesc
 *		The index being created
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtranscreateindex (int ihandle, struct keydesc *pskeydesc)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	VB_CHAR    *pcbuffer;
	int         ilength = 0, iloop;

	if (vb_rtd->ivblogfilehandle < 0
		|| vb_rtd->psvbfile[ihandle]->iopenmode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	vtranshdr ((VB_CHAR *) VBL_CREINDEX);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	inl_stint (pskeydesc->k_flags, pcbuffer + INTSIZE);
	inl_stint (pskeydesc->k_nparts, pcbuffer + (2 * INTSIZE));
	pcbuffer += (INTSIZE * 4);
	for (iloop = 0; iloop < pskeydesc->k_nparts; iloop++) {
		inl_stint (pskeydesc->k_part[iloop].kp_start,
				   pcbuffer + (iloop * 3 * INTSIZE));
		inl_stint (pskeydesc->k_part[iloop].kp_leng,
				   pcbuffer + INTSIZE + (iloop * 3 * INTSIZE));
		inl_stint (pskeydesc->k_part[iloop].kp_type,
				   pcbuffer + (INTSIZE * 2) + (iloop * 3 * INTSIZE));
		ilength += pskeydesc->k_part[iloop].kp_leng;
	}
	inl_stint (ilength, pcbuffer - INTSIZE);
	ilength = (INTSIZE * 4) + (INTSIZE * 3 * (pskeydesc->k_nparts));
	vb_rtd->iserrno = iwritetrans (ilength, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtranscluster (void);
 * Arguments:
 *	BUG - Unknown args
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtranscluster ()
{
	/* BUG - Write ivbtranscluster */
	return 0;
}

/*
 * Name:
 *	int	ivbtransdelete (int ihandle, off_t trownumber, int irowlength);
 * Arguments:
 *	int	ihandle
 *		The (open) VBISAM handle
 *	off_t	trownumber
 *		The row number being deleted
 *	int	irowlength
 *		The length of the row being deleted
 * Prerequisites:
 *	Assumed deleted row is in vb_rtd->psvbfile [ihandle]->ppcrowbuffer
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransdelete (int ihandle, off_t trownumber, int irowlength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (vb_rtd->ivblogfilehandle < 0 || psvbptr->iopenmode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	if (psvbptr->itransyet == 0) {
		ivbtransopen (ihandle, psvbptr->cfilename);
	}
	vtranshdr ((VB_CHAR *) VBL_DELETE);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	st_compx8 (trownumber, pcbuffer + INTSIZE);
	inl_stint (irowlength, pcbuffer + INTSIZE + QUADSIZ8);
	memcpy (pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE, psvbptr->ppcrowbuffer,
			(size_t) irowlength);
	irowlength += (INTSIZE * 2) + QUADSIZ8;
	vb_rtd->iserrno = iwritetrans (irowlength, 1);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransdeleteindex (int ihandle, struct keydesc *pskeydesc);
 * Arguments:
 *	int	ihandle
 *		An (exclusively) open VBISAM table
 *	struct	keydesc	*pskeydesc
 *		The index being created
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransdeleteindex (int ihandle, struct keydesc *pskeydesc)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	VB_CHAR    *pcbuffer;
	int         ilength = 0, iloop;

	if (vb_rtd->ivblogfilehandle < 0
		|| vb_rtd->psvbfile[ihandle]->iopenmode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	vtranshdr ((VB_CHAR *) VBL_DELINDEX);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	inl_stint (pskeydesc->k_flags, pcbuffer + INTSIZE);
	inl_stint (pskeydesc->k_nparts, pcbuffer + (2 * INTSIZE));
	pcbuffer += (INTSIZE * 4);
	for (iloop = 0; iloop < pskeydesc->k_nparts; iloop++) {
		inl_stint (pskeydesc->k_part[iloop].kp_start,
				   pcbuffer + (iloop * 3 * INTSIZE));
		inl_stint (pskeydesc->k_part[iloop].kp_leng,
				   pcbuffer + INTSIZE + (iloop * 3 * INTSIZE));
		inl_stint (pskeydesc->k_part[iloop].kp_type,
				   pcbuffer + (INTSIZE * 2) + (iloop * 3 * INTSIZE));
		ilength += pskeydesc->k_part[iloop].kp_leng;
	}
	inl_stint (ilength, pcbuffer - INTSIZE);
	ilength = (INTSIZE * 4) + (INTSIZE * 3 * (pskeydesc->k_nparts));
	vb_rtd->iserrno = iwritetrans (ilength, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtranserase (VB_CHAR *pcfilename);
 * Arguments:
 *	VB_CHAR	*pcfilename
 *		The name of the VBISAM table to erase
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtranserase (VB_CHAR * pcfilename)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	VB_CHAR    *pcbuffer;
	int         ilength;

	if (vb_rtd->ivblogfilehandle < 0) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	vtranshdr ((VB_CHAR *) VBL_FILEERASE);
	ilength = strlen ((char *) pcfilename) + 1;
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	memcpy (pcbuffer, pcfilename, (size_t) ilength);
	vb_rtd->iserrno = iwritetrans (ilength, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransclose (int ihandle, VB_CHAR *pcfilename);
 * Arguments:
 *	int	ihandle
 *		The handle of the VBISAM table being closed
 *	VB_CHAR	*pcfilename
 *		The name of the VBISAM table to erase
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransclose (int ihandle, VB_CHAR * pcfilename)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;
	int         ilength;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (vb_rtd->ivblogfilehandle < 0 || psvbptr->iopenmode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBROLLBACK) {
		return 0;
	}
	if (psvbptr->itransyet == 0) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	vtranshdr ((VB_CHAR *) VBL_FILECLOSE);
	ilength = strlen ((char *) pcfilename) + 1;
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	inl_stint (0, pcbuffer + INTSIZE); /* VARLEN flag! */
	memcpy (pcbuffer + INTSIZE + INTSIZE, pcfilename, (size_t) ilength);
	ilength += (INTSIZE * 2);
	vb_rtd->iserrno = iwritetrans (ilength, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransopen (int ihandle, VB_CHAR *pcfilename);
 * Arguments:
 *	int	ihandle
 *		The handle of the VBISAM table being opened
 *	VB_CHAR	*pcfilename
 *		The name of the VBISAM table to erase
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransopen (int ihandle, const VB_CHAR * pcfilename)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;
	int         ilength;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (vb_rtd->ivblogfilehandle < 0 || psvbptr->iopenmode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	psvbptr->itransyet = 2;
	vtranshdr ((VB_CHAR *) VBL_FILEOPEN);
	ilength = strlen ((char *) pcfilename) + 1;
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	inl_stint (0, pcbuffer + INTSIZE); /* VARLEN flag! */
	memcpy (pcbuffer + INTSIZE + INTSIZE, pcfilename, (size_t) ilength);
	ilength += (INTSIZE * 2);
	vb_rtd->iserrno = iwritetrans (ilength, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransinsert (int ihandle, off_t trownumber, int irowlength, VB_CHAR *pcrow);
 * Arguments:
 *	int	ihandle
 *		The (open) VBISAM handle
 *	off_t	trownumber
 *		The row number being inserted
 *	int	irowlength
 *		The length of the inserted row
 *	VB_CHAR	*pcrow
 *		The *NEW* row
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransinsert (int ihandle, off_t trownumber, int irowlength, VB_CHAR * pcrow)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if ((vb_rtd->ivblogfilehandle < 0)
		|| (psvbptr->iopenmode & ISNOLOG)) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	if (psvbptr->itransyet == 0) {
		ivbtransopen (ihandle, psvbptr->cfilename);
	}
	vtranshdr ((VB_CHAR *) VBL_INSERT);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	st_compx8 (trownumber, pcbuffer + INTSIZE);
	inl_stint (irowlength, pcbuffer + INTSIZE + QUADSIZ8);
	memcpy (pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE, pcrow, (size_t) irowlength);
	irowlength += (INTSIZE * 2) + QUADSIZ8;
	vb_rtd->iserrno = iwritetrans (irowlength, 1);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransrename (VB_CHAR *pcoldname, VB_CHAR *pcnewname);
 * Arguments:
 *	VB_CHAR	*pcoldname
 *		The original filename
 *	VB_CHAR	*pcnewname
 *		The new filename
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransrename (VB_CHAR * pcoldname, VB_CHAR * pcnewname)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	VB_CHAR    *pcbuffer;
	int         ilength, ilength1, ilength2;

	if (vb_rtd->ivblogfilehandle < 0) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	vtranshdr ((VB_CHAR *) VBL_RENAME);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	ilength1 = strlen ((char *) pcoldname) + 1;
	ilength2 = strlen ((char *) pcnewname) + 1;
	inl_stint (ilength1, pcbuffer);
	inl_stint (ilength2, pcbuffer + INTSIZE);
	memcpy (pcbuffer + (INTSIZE * 2), pcoldname, (size_t) ilength1);
	memcpy (pcbuffer + (INTSIZE * 2) + ilength1, pcnewname, (size_t) ilength2);
	ilength = (INTSIZE * 2) + ilength1 + ilength2;
	vb_rtd->iserrno = iwritetrans (ilength, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtranssetunique (int ihandle, off_t tuniqueid);
 * Arguments:
 *	int	ihandle
 *		The handle of an open VBISAM file
 *	off_t	tuniqueid
 *		The new unique ID starting number
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtranssetunique (int ihandle, off_t tuniqueid)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (vb_rtd->ivblogfilehandle < 0 || psvbptr->iopenmode & ISNOLOG) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	if (psvbptr->itransyet == 0) {
		ivbtransopen (ihandle, psvbptr->cfilename);
	}
	vtranshdr ((VB_CHAR *) VBL_SETUNIQUE);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	st_compx8 (tuniqueid, pcbuffer + INTSIZE);
	vb_rtd->iserrno = iwritetrans (INTSIZE + QUADSIZ8, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransuniqueid (int ihandle, off_t tuniqueid);
 * Arguments:
 *	int	ihandle
 *		The handle of an open VBISAM file
 *	off_t	tuniqueid
 *		The extracted unique ID
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransuniqueid (int ihandle, off_t tuniqueid)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (vb_rtd->ivblogfilehandle < 0 || (psvbptr->iopenmode & ISNOLOG)) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	if (psvbptr->itransyet == 0) {
		ivbtransopen (ihandle, psvbptr->cfilename);
	}
	vtranshdr ((VB_CHAR *) VBL_UNIQUEID);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	st_compx8 (tuniqueid, pcbuffer + INTSIZE);
	vb_rtd->iserrno = iwritetrans (INTSIZE + QUADSIZ8, 0);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}

/*
 * Name:
 *	int	ivbtransupdate (int ihandle, off_t trownumber, int ioldrowlen, int inewrowlen, VB_CHAR *pcrow);
 * Arguments:
 *	int	ihandle
 *		The (open) VBISAM handle
 *	off_t	trownumber
 *		The row number being rewritten
 *	int	ioldrowlen
 *		The length of the original row
 *	int	inewrowlen
 *		The length of the replacement row
 *	VB_CHAR	*pcrow
 *		The *UPDATED* row
 * Prerequisites:
 *	Assumes non-modified row is in vb_rtd->psvbfile [ihandle]->ppcrowbuffer
 * Returns:
 *	-1	Failure (vb_rtd->iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
ivbtransupdate (int ihandle, off_t trownumber, int ioldrowlen, int inewrowlen,
				VB_CHAR * pcrow)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
#if (VBLOGGING == 0)
	vb_rtd->ivblogfilehandle = -1;
	return 0;
#else
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcbuffer;
	int         ilength;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (vb_rtd->ivblogfilehandle < 0 || (psvbptr->iopenmode & ISNOLOG)) {
		return 0;
	}
	/* Don't log transactions if we're in rollback / recover mode */
	if (vb_rtd->ivbintrans > VBNEEDFLUSH) {
		return 0;
	}
	if (vb_rtd->ivbintrans == VBBEGIN) {
		if (iwritebegin ()) {
			return -1;
		}
	}
	if (psvbptr->itransyet == 0) {
		ivbtransopen (ihandle, psvbptr->cfilename);
	}
	vtranshdr ((VB_CHAR *) VBL_UPDATE);
	pcbuffer = vb_rtd->cvbtransbuffer + sizeof (struct SLOGHDR);
	inl_stint (ihandle, pcbuffer);
	st_compx8 (trownumber, pcbuffer + INTSIZE);
	inl_stint (ioldrowlen, pcbuffer + INTSIZE + QUADSIZ8);
	inl_stint (inewrowlen, pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE);
	memcpy (pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE + INTSIZE, psvbptr->ppcrowbuffer,
			(size_t) ioldrowlen);
	memcpy (pcbuffer + INTSIZE + QUADSIZ8 + INTSIZE + INTSIZE + ioldrowlen, pcrow,
			(size_t) inewrowlen);
	ilength = INTSIZE + QUADSIZ8 + (INTSIZE * 2) + ioldrowlen + inewrowlen;
	vb_rtd->iserrno = iwritetrans (ilength, 1);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
#endif
}
