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

/* Local functions */

static int
ivbforceexit (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	int         iresult = 0;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (psvbptr->iisdictlocked & 0x02) {
		iresult = ivbwritedict (ihandle);
		if (iresult) {
			vb_rtd->iserrno = EBADFILE;
		} else {
			vb_rtd->iserrno = 0;
		}
	}
	psvbptr->iisdictlocked = 0;
	if (iresult) {
		return -1;
	}
	return 0;
}

static      off_t
tcountrows (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	off_t       tnodenumber, tdatacount;
	int         inodeused, iqdsz;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	tnodenumber = vb_rtd->psvbfile[ihandle]->idatafree;
	tdatacount = vb_rtd->psvbfile[ihandle]->idatacount;
	iqdsz = vb_rtd->psvbfile[ihandle]->iquadsize;
	while (tnodenumber) {
		if (ivbblockread (ihandle, 1, tnodenumber, cvbnodetmp)) {
			return -1;
		}
		inodeused = inl_ldint (cvbnodetmp);
		inodeused -= INTSIZE + iqdsz;
		tdatacount -= (inodeused / iqdsz);
		tnodenumber = inl_ldcompx (cvbnodetmp + INTSIZE, iqdsz);
	}
	return tdatacount;
}

/* Global functions */

/* Comments:
*	The isclose () function does not *COMPLETELY* close a table *IF* the
*	call to isclose () occurred during a transaction.  This is to make sure
*	that rowlocks held during a transaction are retained.  This function is
*	the 'middle half' that performs the (possibly delayed) physical close.
*	The 'lower half' (ivbclose3) frees up the cached stuff.
*/
int
ivbclose2 (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct VBLOCK *psrowlock;
	struct DICTINFO *psvbfptr;
	int         iloop;
	int         iindexhandle;

	psvbfptr = vb_rtd->psvbfile[ihandle];
	psvbfptr->iisopen = 0;			   /* It's a LIE, but so what! */
	isrelease (ihandle);
	vb_rtd->iserrno = ivbtransclose (ihandle, psvbfptr->cfilename);
	if (ivbclose (psvbfptr->idatahandle)) {
		vb_rtd->iserrno = errno;
	}
	psvbfptr->idatahandle = -1;
	if (ivbclose (psvbfptr->iindexhandle)) {
		vb_rtd->iserrno = errno;
	}
	iindexhandle = psvbfptr->iindexhandle;
	while (vb_rtd->svbfile[iindexhandle].pslockhead) {
		psrowlock = vb_rtd->svbfile[iindexhandle].pslockhead->psnext;
		vvblockfree (vb_rtd->svbfile[iindexhandle].pslockhead);
		vb_rtd->svbfile[iindexhandle].pslockhead = psrowlock;
	}
	vb_rtd->svbfile[iindexhandle].pslocktail = NULL;
	psvbfptr->iindexhandle = -1;
/* RXW
    psvbfptr->trownumber = -1;
    psvbfptr->tdupnumber = -1;
*/
	psvbfptr->trownumber = 0;
	psvbfptr->tdupnumber = 0;
	psvbfptr->iisopen = 2;			   /* Only buffers remain! */
	psvbfptr->itransyet = 0;
	for (iloop = 0; iloop < MAXSUBS; iloop++) {
		psvbfptr->pskeycurr[iloop] = NULL;
	}

	return (vb_rtd->iserrno ? -1 : 0);
}

void
ivbclose3 (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	int         iloop;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!psvbptr) {
		return;
	}
	for (iloop = 0; iloop < MAXSUBS; iloop++) {
		vvbtreeallfree (ihandle, iloop, psvbptr->pstree[iloop]);
		if (psvbptr->pskeydesc[iloop]) {
			vvbkeyunmalloc (ihandle, iloop);
			vvbfree (psvbptr->pskeydesc[iloop], sizeof (struct keydesc));
			psvbptr->pskeydesc[iloop] = NULL;
		}
	}
	vvbfreedict (psvbptr);
	vb_rtd->psvbfile[ihandle] = NULL;
}

int
iscleanup (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	int         iloop, iresult, iresult2 = 0;

	for (iloop = 0; iloop <= vb_rtd->ivbmaxusedhandle; iloop++)
		if (vb_rtd->psvbfile[iloop]) {
			if (vb_rtd->psvbfile[iloop]->iisopen == 0) {
				iresult = isclose (iloop);
				if (iresult) {
					iresult2 = vb_rtd->iserrno;
				}
			}
			if (vb_rtd->psvbfile[iloop]->iisopen == 1) {
				iresult = ivbclose2 (iloop);
				if (iresult) {
					iresult2 = vb_rtd->iserrno;
				}
			}
			ivbclose3 (iloop);
		}
	if (vb_rtd->ivblogfilehandle >= 0) {
		iresult = islogclose ();
		if (iresult) {
			iresult2 = vb_rtd->iserrno;
		}
	}
	return iresult2;
}

int
isclose (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;

	vbclrstatus ();
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!psvbptr || psvbptr->iisopen) {
		vb_rtd->iserrno = ENOTOPEN;
		vbsetstatus ();
		return -1;
	}
	if (psvbptr->iopenmode & ISEXCLLOCK) {
		ivbforceexit (ihandle);		   /* BUG retval */
	}
	psvbptr->iindexchanged = 0;
	psvbptr->iisopen = 1;
	if (psvbptr->iformat == C_ISAM_FILE) {
		off_t       imaxfile;
		if ((psvbptr->iopenmode & ISVARLEN))
			imaxfile = (psvbptr->iminrowlength + 1 + 6) * psvbptr->idatacount;
		else
			imaxfile = (psvbptr->iminrowlength + 1) * psvbptr->idatacount;
		ftruncate (vb_rtd->svbfile[psvbptr->idatahandle].ihandle, imaxfile);
	}
	if (!(vb_rtd->ivbintrans == VBBEGIN
		  || vb_rtd->ivbintrans == VBNEEDFLUSH || vb_rtd->ivbintrans == VBRECOVER)) {
		if (ivbclose2 (ihandle)) {
			vbsetstatus ();
			return -1;
		}
	}
	vbsetstatus ();
	return 0;
}

int
isfullclose (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	vbclrstatus ();
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	isclose (ihandle);
	ivbclose3 (ihandle);
	vbsetstatus ();
	return 0;
}

int
isindexinfo (int ihandle, void *pskeydesc, int ikeynumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;
	struct dictinfo sdict;

	vbclrstatus ();
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	psvbfptr = vb_rtd->psvbfile[ihandle];
	if (!psvbfptr || psvbfptr->iisopen) {
		vb_rtd->iserrno = ENOTOPEN;
		vbsetstatus ();
		return -1;
	}
	vb_rtd->iserrno = EBADKEY;
	if (ikeynumber < 0 || ikeynumber > psvbfptr->inkeys) {
		vbsetstatus ();
		return -1;
	}
	vb_rtd->iserrno = 0;
	if (ikeynumber) {
		memcpy (pskeydesc, psvbfptr->pskeydesc[ikeynumber - 1], sizeof (struct keydesc));
		vbsetstatus ();
		return 0;
	}

	if (ivbenter (ihandle, 0)) {
		vbsetstatus ();
		return -1;
	}

	sdict.di_nkeys = psvbfptr->inkeys;
	if (psvbfptr->iopenmode & ISVARLEN) {
		sdict.di_nkeys |= (short) (1 << 15);
	}
	sdict.di_recsize = psvbfptr->imaxrowlength;
	sdict.di_idxsize = psvbfptr->inodesize;
	sdict.di_nrecords = tcountrows (ihandle);
	vb_rtd->isreclen = psvbfptr->iminrowlength;
	memcpy (pskeydesc, &sdict, sizeof (struct dictinfo));

	ivbexit (ihandle);
	vbsetstatus ();
	return 0;
}

int
isopen (const VB_CHAR * pcfilename, int imode)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psfile;
	VB_CHAR    *pctemp;
	struct keydesc *pkptr;
	off_t       tnodenumber;
	int         iflags, retsts, ivbopenmode;
	int         ihandle, iindexnumber = 0, iindexpart;
	int         ikeydesclength, ilengthused, iresult;
	struct stat sstat;
	VB_CHAR     tmpfname[1024], tmpdatname[1024];
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	vbclrstatus ();
	ivbopenmode = O_RDWR | O_BINARY;
#if defined(ISSYNCWR) && defined(O_SYNC)
	if ((imode & ISSYNCWR)) {
		ivbopenmode |= O_SYNC;
	}
#endif
	if ((imode & ISTRANS) && vb_rtd->ivblogfilehandle < 0) {
		vb_rtd->iserrno = ENOTRANS;
		vbsetstatus ();
		return -1;
	}
	iflags = imode & 0x03;
	if (iflags == 3) {
		/* Cannot be BOTH ISOUTPUT and ISINOUT */
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	if (strlen ((char *) pcfilename) > MAX_PATH_LENGTH - 4) {
		vb_rtd->iserrno = EFNAME;
		vbsetstatus ();
		return -1;
	}
	/*
	 * The following for loop deals with the concept of re-opening a file
	 * that was closed within the SAME transaction.  Since we were not
	 * allowed to perform the FULL close during the transaction because we
	 * needed to retain all the transactional locks, we can tremendously
	 * simplify the re-opening process.
	 */
	for (ihandle = 0; ihandle <= vb_rtd->ivbmaxusedhandle; ihandle++) {
		psfile = vb_rtd->psvbfile[ihandle];
		if (psfile && psfile->iisopen != 0) {
			if (!strcmp ((char *) psfile->cfilename, (char *) pcfilename)) {
				if (psfile->iisopen == 2) {
					sprintf ((char *) tmpfname, "%s.idx", pcfilename);
					psfile->iindexhandle = ivbopen (tmpfname, ivbopenmode, 0);
					if ((imode & ISMNODAT))
						strcpy ((char *) tmpfname, (char *) pcfilename);
					else
						vbdatfilename ((char *) tmpfname, (char *) pcfilename);
					psfile->idatahandle = ivbopen (tmpfname, ivbopenmode, 0);
					if (psfile->idatahandle < 0 && psfile->iformat == C_ISAM_FILE) {
						sprintf ((char *) tmpfname, "%s", pcfilename);
						psfile->idatahandle = ivbopen (tmpfname, ivbopenmode, 0);
						if (psfile->idatahandle >= 0)
							vb_rtd->iserrno = errno = 0;
					}
					if ((imode & ISEXCLLOCK)) {
						iresult = ivbfileopenlock (ihandle, 2);
					} else {
						iresult = ivbfileopenlock (ihandle, 1);
					}
					if (iresult) {
						errno = EFLOCKED;
						goto open_err;
					}
				}
				psfile->iisopen = 0;
				if (imode & ISREBUILD) {
					if (psfile->imaxrowlength != psfile->iminrowlength) {
						imode |= ISVARLEN;
					}
				}
				psfile->iopenmode = imode;
				return ihandle;
			}
		}
	}
	for (ihandle = 0;; ihandle++) {
		if (ihandle > vb_rtd->ivbmaxusedhandle) {
			if (vb_rtd->ivbmaxusedhandle >= VB_MAX_FILES) {
				vb_rtd->iserrno = ETOOMANY;
				vbsetstatus ();
				return -1;
			}
			vb_rtd->ivbmaxusedhandle = ihandle;
			break;
		}
		if (vb_rtd->psvbfile[ihandle] == NULL) {
			break;
		}
	}
	vb_rtd->psvbfile[ihandle] = pvvbmalloc (sizeof (struct DICTINFO));
	if (vb_rtd->psvbfile[ihandle] == NULL) {
		errno = EBADMEM;
		goto open_err;
	}
	psfile = vb_rtd->psvbfile[ihandle];
	psfile->cfilename = (VB_CHAR *) strdup ((char *) pcfilename);
	if (psfile->cfilename == NULL) {
		errno = EBADMEM;
		goto open_err;
	}
	psfile->ppcrowbuffer = pvvbmalloc (MAX_RESERVED_LENGTH);
	if (psfile->ppcrowbuffer == NULL) {
		errno = EBADMEM;
		goto open_err;
	}
	psfile->idatahandle = -1;
	psfile->iindexhandle = -1;
	sprintf ((char *) tmpfname, "%s.idx", pcfilename);
	if (stat ((char *) tmpfname, &sstat)) {
		errno = ENOENT;
		goto open_err;
	}
	psfile->iindexhandle = ivbopen (tmpfname, ivbopenmode, 0);
	if (psfile->iindexhandle < 0) {
		goto open_err;
	}
	if ((imode & ISMNODAT))
		strcpy ((char *) tmpdatname, (char *) pcfilename);
	else
		vbdatfilename ((char *) tmpdatname, (char *) pcfilename);
	if (stat ((char *) tmpdatname, &sstat)) {
		sprintf ((char *) tmpdatname, "%s", pcfilename);
		if (stat ((char *) tmpdatname, &sstat)) {
			errno = ENOENT;
			goto open_err;
		}
	}
	psfile->idatahandle = ivbopen (tmpdatname, ivbopenmode, 0);
	if (psfile->idatahandle < 0) {
		goto open_err;
	}
	psfile->iisopen = 0;

	psfile->inodesize = C_DFLT_NODE_LENGTH;
	if (ivbenter (ihandle, iflags == ISINPUT ? 0 : 1)) {	/* Reads in dictionary node */
		errno = vb_rtd->iserrno;
		psfile->iopenmode = 0;
		goto open_err;
	}
	if (iflags == ISINPUT) {
		iresult = ivbreaddict (ihandle);
		if (psfile->imaxrowlength && psfile->imaxrowlength != psfile->iminrowlength) {
			imode |= ISVARLEN;
		}
	}
	errno = EBADFILE;
	if (psfile->inodesize > MAX_NODE_LENGTH) {
		errno = EBADFILE;
		goto open_err;
	}
	if (imode & ISREBUILD) {
		if (psfile->imaxrowlength != psfile->iminrowlength) {
			imode |= ISVARLEN;
		}
	} else {
		if (!(imode & ISVARLEN)) {
			psfile->imaxrowlength = psfile->iminrowlength;
		}

		errno = EROWSIZE;
		if (psfile->imaxrowlength && psfile->imaxrowlength != psfile->iminrowlength) {
			if (!(imode & ISVARLEN)) {
				goto open_err;
			}
		} else {
			if (imode & ISVARLEN) {
				goto open_err;
			}
		}
		errno = 0;
	}
	psfile->iopenmode = imode;
	tnodenumber = psfile->inodekeydesc;

	/* Fill in the keydesc stuff */
	while (tnodenumber) {
		iresult = ivbblockread (ihandle, 1, tnodenumber, cvbnodetmp);
		errno = EBADFILE;
		if (iresult) {
			goto open_err;
		}
		pctemp = cvbnodetmp;
		if (*(cvbnodetmp + psfile->inodesize - 3) != -1
			|| *(cvbnodetmp + psfile->inodesize - 2) != 0x7e) {
			goto open_err;
		}
		ilengthused = inl_ldint (pctemp);
		pctemp += INTSIZE;
		tnodenumber = inl_ldcompx (pctemp, psfile->iquadsize);
		pctemp += psfile->iquadsize;
		ilengthused -= (INTSIZE + psfile->iquadsize);
		while (ilengthused > 0) {
			errno = EBADFILE;
			if (iindexnumber >= MAXSUBS) {
				goto open_err;
			}
			ikeydesclength = inl_ldint (pctemp);
			ilengthused -= ikeydesclength;
			pctemp += INTSIZE;
			psfile->pskeydesc[iindexnumber] = pvvbmalloc (sizeof (struct keydesc));
			pkptr = psfile->pskeydesc[iindexnumber];
			if (pkptr == NULL) {
				errno = EBADMEM;
				goto open_err;
			}
			pkptr->k_nparts = 0;
			pkptr->k_len = 0;
			pkptr->k_rootnode = inl_ldcompx (pctemp, psfile->iquadsize);
			pctemp += psfile->iquadsize;
			pkptr->k_flags = (*pctemp) * 2;
			pctemp++;
			ikeydesclength -= (psfile->iquadsize + INTSIZE + 1);
			iindexpart = 0;
			if (*pctemp & 0x80) {
				pkptr->k_flags |= ISDUPS;
			}
			*pctemp &= ~0x80;
			while (ikeydesclength > 0) {
				pkptr->k_nparts++;
				pkptr->k_part[iindexpart].kp_leng = inl_ldint (pctemp);
				pkptr->k_len += pkptr->k_part[iindexpart].kp_leng;
				pctemp += INTSIZE;
				pkptr->k_part[iindexpart].kp_start = inl_ldint (pctemp);
				pctemp += INTSIZE;
				pkptr->k_part[iindexpart].kp_type = *pctemp;
				pctemp++;
				ikeydesclength -= ((INTSIZE * 2) + 1);
				errno = EBADFILE;
				if (ikeydesclength < 0 && !(pkptr->k_flags & NULLKEY)) {
					goto open_err;
				}
				if ((pkptr->k_flags & NULLKEY)) {
					if (psfile->iformat == V_ISAM_FILE) {
						pkptr->k_part[iindexpart].kp_type |= (*pctemp << BYTESHFT);
						pctemp++;
					} else {
						pctemp--;
						pkptr->k_part[iindexpart].kp_type = inl_ldint (pctemp);
						pctemp += INTSIZE;
					}
					ikeydesclength--;
				}
				iindexpart++;
			}
			iindexnumber++;
		}
		if (ilengthused < 0) {
			goto open_err;
		}
	}
	errno = 0;
	if (imode & ISEXCLLOCK) {
		iresult = ivbfileopenlock (ihandle, 2);
	} else {
		iresult = ivbfileopenlock (ihandle, 1);
	}
	if (iresult) {
		errno = EFLOCKED;
		goto open_err;
	}
	ivbexit (ihandle);
	if (!(imode & ISREBUILD)) {
		iresult = isstart (ihandle, psfile->pskeydesc[0], 0, NULL, ISFIRST);
		if (iresult) {
			errno = vb_rtd->iserrno;
			goto open_err;
		}
	}
#if (VBLOGGING == 1)
	if (vb_rtd->ivbintrans == VBNOTRANS) {
		ivbtransopen (ihandle, pcfilename);
		psfile->itransyet = 1;
	}
#endif
	vbsetstatus ();
	return ihandle;
  open_err:
	if (errno != 0 && vb_rtd->iserrno == 0)
		vb_rtd->iserrno = errno;
	retsts = vb_rtd->iserrno;
	ivbexit (ihandle);
	psfile = vb_rtd->psvbfile[ihandle];
	if (psfile != NULL) {
		vvbfreedict (psfile);
	}
	vb_rtd->psvbfile[ihandle] = NULL;
	vb_rtd->iserrno = retsts;
	vbsetstatus ();
	return -1;
}

int
issetcollate (int ihandle, VB_UCHAR * collating_sequence)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADARG;
		return -1;
	}
	vb_rtd->psvbfile[ihandle]->collating_sequence = collating_sequence;
	return 0;
}
