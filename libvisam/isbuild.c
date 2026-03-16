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

int
VBiaddkeydescriptor (const int ihandle, struct keydesc *pskeydesc)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcdstptr;
	off_t       theadnode, tnodenumber = 0, tnewnode;
	int         iloop, ilenkeyuncomp = 0, ilenkeydesc, inodeused;
	VB_CHAR     ckeydesc[INTSIZE + 8 + 1 + (NPARTS * ((INTSIZE * 2) + 1))];
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	psvbptr = vb_rtd->psvbfile[ihandle];
	pcdstptr = ckeydesc + INTSIZE;
	/*
	 * Step 1:
	 *      Create a new 'root node' for the new index
	 */
	tnewnode = tvbnodecountgetnext (ihandle);
	if (tnewnode == -1) {
		return -1;
	}
	memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
	if (psvbptr->iformat == V_ISAM_FILE) {
		inl_stint (INTSIZE + psvbptr->iquadsize, cvbnodetmp);
		inl_stcompx ((off_t) 1, cvbnodetmp + INTSIZE, psvbptr->iquadsize);
	} else {
		inl_stint (INTSIZE, cvbnodetmp);
	}
	vb_rtd->iserrno = ivbblockwrite (ihandle, 1, tnewnode, cvbnodetmp);
	if (vb_rtd->iserrno) {
		return -1;
	}
	/*
	 * Step 2:
	 *      Append the new key description to the keydesc list
	 */
	theadnode = psvbptr->inodekeydesc;
	if (theadnode < 1) {
		return -1;
	}
	while (theadnode) {
		tnodenumber = theadnode;
		vb_rtd->iserrno = ivbblockread (ihandle, 1, tnodenumber, cvbnodetmp);
		if (vb_rtd->iserrno) {
			return -1;
		}
		theadnode = inl_ldcompx (cvbnodetmp + INTSIZE, psvbptr->iquadsize);
	}
	inl_stcompx (tnewnode, pcdstptr, psvbptr->iquadsize);
	pcdstptr += psvbptr->iquadsize;
	*pcdstptr = pskeydesc->k_flags / 2;
	pcdstptr++;
	for (iloop = 0; iloop < pskeydesc->k_nparts; iloop++) {
		ilenkeyuncomp += pskeydesc->k_part[iloop].kp_leng;
		inl_stint (pskeydesc->k_part[iloop].kp_leng, pcdstptr);
		if (iloop == 0 && pskeydesc->k_flags & ISDUPS) {
			*pcdstptr |= 0x80;
		}
		pcdstptr += INTSIZE;
		inl_stint (pskeydesc->k_part[iloop].kp_start, pcdstptr);
		pcdstptr += INTSIZE;
		*pcdstptr = pskeydesc->k_part[iloop].kp_type & BYTEMASK;
		pcdstptr++;
		if ((pskeydesc->k_flags & NULLKEY)) {
			if (psvbptr->iformat == V_ISAM_FILE) {
				*pcdstptr =
					(pskeydesc->k_part[iloop].kp_type >> BYTESHFT) & BYTEMASK;
				pcdstptr++;
			} else {
				pcdstptr -= 1;
				inl_stint (pskeydesc->k_part[iloop].kp_type, pcdstptr);
				pcdstptr += INTSIZE;
			}
		}
	}
	inodeused = inl_ldint (cvbnodetmp);
	ilenkeydesc = pcdstptr - ckeydesc;
	inl_stint (ilenkeydesc, ckeydesc);
	if (psvbptr->inodesize - (inodeused + 4) < ilenkeydesc) {
		VB_CHAR     cvbnodetmp2[MAX_NODE_LENGTH];

		tnewnode = tvbnodecountgetnext (ihandle);
		if (tnewnode == -1) {
			return -1;
		}
		memset (cvbnodetmp2, 0, MAX_NODE_LENGTH);
		inl_stint (INTSIZE + psvbptr->iquadsize + ilenkeydesc, cvbnodetmp2);
		memcpy (cvbnodetmp2 + INTSIZE + psvbptr->iquadsize, ckeydesc,
				(size_t) ilenkeydesc);
		vb_rtd->iserrno = ivbblockwrite (ihandle, 1, tnewnode, cvbnodetmp2);
		if (vb_rtd->iserrno) {
			return -1;
		}
		inl_stcompx (tnewnode, cvbnodetmp + INTSIZE, psvbptr->iquadsize);
		vb_rtd->iserrno = ivbblockwrite (ihandle, 1, tnodenumber, cvbnodetmp);
		if (vb_rtd->iserrno) {
			return -1;
		}
		return 0;
	}
	inl_stint (inodeused + ilenkeydesc, cvbnodetmp);
	pskeydesc->k_len = ilenkeyuncomp;
	pskeydesc->k_rootnode = tnewnode;
	memcpy (cvbnodetmp + inodeused, ckeydesc, (size_t) ilenkeydesc);
	vb_rtd->iserrno = ivbblockwrite (ihandle, 1, tnodenumber, cvbnodetmp);
	if (vb_rtd->iserrno) {
		return -1;
	}
	return 0;
}

static      off_t
tdelkeydescriptor (const int ihandle, struct keydesc *pskeydesc,
				   const int ikeynumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcsrcptr;
	off_t       theadnode;
	int         iloop = 0, inodeused;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	psvbptr = vb_rtd->psvbfile[ihandle];
	vb_rtd->iserrno = EBADFILE;
	theadnode = psvbptr->inodekeydesc;
	while (1) {
		if (!theadnode) {
			return -1;
		}
		memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
		vb_rtd->iserrno = ivbblockread (ihandle, 1, theadnode, cvbnodetmp);
		if (vb_rtd->iserrno) {
			return -1;
		}
		pcsrcptr = cvbnodetmp + INTSIZE + psvbptr->iquadsize;
		inodeused = inl_ldint (cvbnodetmp);
		while (pcsrcptr - cvbnodetmp < inodeused) {
			if (iloop < ikeynumber) {
				iloop++;
				pcsrcptr += inl_ldint (pcsrcptr);
				continue;
			}
			inodeused -= inl_ldint (pcsrcptr);
			inl_stint (inodeused, cvbnodetmp);
			memcpy (pcsrcptr, pcsrcptr + inl_ldint (pcsrcptr),
					(size_t) (psvbptr->inodesize - (pcsrcptr - cvbnodetmp +
													inl_ldint (pcsrcptr))));
			vb_rtd->iserrno = ivbblockwrite (ihandle, 1, theadnode, cvbnodetmp);
			if (vb_rtd->iserrno) {
				return -1;
			}
			return psvbptr->pskeydesc[ikeynumber]->k_rootnode;
		}
		theadnode = inl_ldcompx (cvbnodetmp + INTSIZE, psvbptr->iquadsize);
	}
	return -1;						   /* Just to keep the compiler happy :) */
}

static int
idelnodes (const int ihandle, const int ikeynumber, off_t trootnode)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcsrcptr;
	struct keydesc *pskeydesc;
	int         iduplicate, ikeylength, icomplength = 0;
	int         inodeused, iresult = 0;
	VB_CHAR     clclnode[MAX_NODE_LENGTH];

	psvbptr = vb_rtd->psvbfile[ihandle];
	pskeydesc = psvbptr->pskeydesc[ikeynumber];
	iresult = ivbblockread (ihandle, 1, trootnode, clclnode);
	if (iresult) {
		return iresult;
	}
	/* Recurse for non-leaf nodes */
	if (*(clclnode + psvbptr->inodesize - 2)) {
		inodeused = inl_ldint (clclnode);
		if (psvbptr->iformat == V_ISAM_FILE) {
			pcsrcptr = clclnode + INTSIZE + psvbptr->iquadsize;
		} else {
			pcsrcptr = clclnode + INTSIZE;
		}
		iduplicate = 0;
		while (pcsrcptr - clclnode < inodeused) {
			if (iduplicate) {
				if (!(*(pcsrcptr + psvbptr->iquadsize) & 0x80)) {
					iduplicate = 0;
				}
				*(pcsrcptr + psvbptr->iquadsize) &= ~0x80;
				iresult = idelnodes (ihandle, ikeynumber,
									 inl_ldcompx (pcsrcptr + psvbptr->iquadsize,
												  psvbptr->iquadsize));
				if (iresult) {
					return iresult;
				}
				pcsrcptr += (psvbptr->iquadsize * 2);
			}
			ikeylength = pskeydesc->k_len;
			if (pskeydesc->k_flags & LCOMPRESS) {
				if (psvbptr->iformat == V_ISAM_FILE) {
					icomplength = inl_ldint (pcsrcptr);
					pcsrcptr += INTSIZE;
					ikeylength -= (icomplength - 2);
				} else {
					icomplength = *(pcsrcptr);
					pcsrcptr++;
					ikeylength -= (icomplength - 1);
				}
			}
			if (pskeydesc->k_flags & TCOMPRESS) {
				if (psvbptr->iformat == V_ISAM_FILE) {
					icomplength = inl_ldint (pcsrcptr);
					pcsrcptr += INTSIZE;
					ikeylength -= (icomplength - 2);
				} else {
					icomplength = *(pcsrcptr);
					pcsrcptr++;
					ikeylength -= (icomplength - 1);
				}
			}
			pcsrcptr += ikeylength;
			if (pskeydesc->k_flags & ISDUPS) {
				pcsrcptr += psvbptr->iquadsize;
				if (*pcsrcptr & 0x80) {
					iduplicate = 1;
				}
			}
			iresult =
				idelnodes (ihandle, ikeynumber,
						   inl_ldcompx (pcsrcptr, psvbptr->iquadsize));
			if (iresult) {
				return iresult;
			}
			pcsrcptr += psvbptr->iquadsize;
		}
	}
	iresult = ivbnodefree (ihandle, trootnode);
	return iresult;
}

static int
imakekeysfromdata (const int ihandle, const int ikeynumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	struct VBKEY *pskey;
	off_t       tdupnumber, tloop;
	int         ideleted, iresult;
	VB_UCHAR    ckeyvalue[VB_MAX_KEYLEN];

	psvbptr = vb_rtd->psvbfile[ihandle];
	/* Don't have to insert if the key is a NULL key! */
	if (psvbptr->pskeydesc[ikeynumber]->k_nparts == 0) {
		return 0;
	}

	for (tloop = 1; tloop < psvbptr->idatacount; tloop++) {
		/*
		 * Step 1:
		 *      Read in the existing data row (Just the min rowlength)
		 */
		vb_rtd->iserrno = ivbdataread (ihandle, (void *) psvbptr->ppcrowbuffer,
									   &ideleted, tloop);
		if (vb_rtd->iserrno) {
			return -1;
		}
		if (ideleted) {
			continue;
		}
		/*
		 * Step 2:
		 *      Check the index for a potential ISNODUPS error (EDUPL)
		 *      Also, calculate the duplicate number as needed
		 */
		vvbmakekey (psvbptr->pskeydesc[ikeynumber], psvbptr->ppcrowbuffer,
					ckeyvalue);
		iresult =
			ivbkeysearch (ihandle, ISGREAT, ikeynumber, 0, ckeyvalue, (off_t) 0);
		tdupnumber = 0;
		if (iresult >= 0
		 && !ivbkeyload (ihandle, ikeynumber, ISPREV, 0, &pskey)
		 && !memcmp (pskey->ckey, ckeyvalue,
						(size_t) psvbptr->pskeydesc[ikeynumber]->k_len)) {
			vb_rtd->iserrno = EDUPL;
			if (psvbptr->pskeydesc[ikeynumber]->k_flags & ISDUPS) {
				tdupnumber = pskey->tdupnumber + 1;
			} else {
				return -1;
			}
		}

		/*
		 * Step 3:
		 * Perform the actual insertion into the index
		 */
		iresult = ivbkeyinsert (ihandle, NULL, ikeynumber, ckeyvalue, tloop,
								tdupnumber, NULL);
		if (iresult) {
			return iresult;
		}
	}
	return 0;
}

/* Global functions */

int
isbuild (const VB_CHAR * pcfilename, int imaxrowlength, struct keydesc *pskey,
		 int imode)
{
	static int  fmask = 0666;
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	VB_CHAR    *pctemp;
	struct DICTINFO *tvbptr;
	int         iflags, ihandle, iloop, iminrowlength;
	struct stat sstat;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];
	VB_CHAR     tmpfname[1024];
	int         idxblk = ISMIDXSIZE (imode);
	int         duplen = ISMDUPLEN (imode);

	if (duplen != 2 && duplen != 4) {
#if defined(ISDUPLEN)
		duplen = ISDUPLEN;
#else
		duplen = 2;
#endif
	}

	vbclrstatus ();
	/* STEP 1: Sanity checks */
	if (imode & ISVARLEN) {
		iminrowlength = vb_rtd->isreclen;
	} else {
		iminrowlength = imaxrowlength;
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
	if (pskey == NULL) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}

	/* Sanity checks passed (so far) */
	for (ihandle = 0; ihandle <= vb_rtd->ivbmaxusedhandle; ihandle++) {
		if (vb_rtd->psvbfile[ihandle] != NULL) {
			if (!strcmp
				((char *) vb_rtd->psvbfile[ihandle]->cfilename,
				 (char *) pcfilename)) {
				isclose (ihandle);
				ivbclose3 (ihandle);
				break;
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
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (tvbptr == NULL) {
		errno = EBADMEM;
		goto build_err;
	}
	tvbptr->cfilename = (VB_CHAR *) strdup ((char *) pcfilename);
	if (tvbptr->cfilename == NULL) {
		errno = EBADMEM;
		goto build_err;
	}
	tvbptr->ppcrowbuffer = pvvbmalloc (MAX_RESERVED_LENGTH);
	if (tvbptr->ppcrowbuffer == NULL) {
		errno = EBADMEM;
		goto build_err;
	}
	vb_rtd->iserrno = EBADARG;
	tvbptr->iminrowlength = iminrowlength;
	tvbptr->imaxrowlength = imaxrowlength;
	tvbptr->pskeydesc[0] = pvvbmalloc (sizeof (struct keydesc));
	if (tvbptr->pskeydesc[0] == NULL) {
		errno = EBADMEM;
		goto build_err;
	}
	memcpy (tvbptr->pskeydesc[0], pskey, sizeof (struct keydesc));
	if (ivbcheckkey (ihandle, pskey, 0, iminrowlength, 1)) {
		return -1;
	}
	if ((imode & ISMNODAT))
		strcpy ((char *) tmpfname, (char *) pcfilename);
	else
		vbdatfilename ((char *) tmpfname, (char *) pcfilename);
	if (!stat ((char *) tmpfname, &sstat)) {
		errno = EEXIST;
		goto build_err;
	}
	sprintf ((char *) tmpfname, "%s.idx", pcfilename);
	if (!stat ((char *) tmpfname, &sstat)) {
		errno = EEXIST;
		goto build_err;
	}
	tvbptr->iindexhandle = ivbopen (tmpfname, O_RDWR | O_CREAT | O_BINARY, fmask);
	if (tvbptr->iindexhandle < 0) {
		vb_rtd->iserrno = errno = ENOENT;
		goto build_err;
	}
	if ((imode & ISMNODAT))
		strcpy ((char *) tmpfname, (char *) pcfilename);
	else
		vbdatfilename ((char *) tmpfname, (char *) pcfilename);
	tvbptr->idatahandle = ivbopen (tmpfname, O_RDWR | O_CREAT | O_BINARY, fmask);
	if (tvbptr->idatahandle < 0) {
		ivbclose (tvbptr->iindexhandle);	/* Ignore ret */
		vb_rtd->iserrno = errno = ENOENT;
		goto build_err;
	}
	tvbptr->inkeys = 1;
	tvbptr->iopenmode = imode & 0xFFFF;
	tvbptr->iisdictlocked |= 0x01;
	if ((imode & ISMMODEMASK) == 0)
		imode |= ISMMODE;			   /* Set default file format */
	if (imode & ISMVBISAM) {
		tvbptr->iformat = V_ISAM_FILE;
		tvbptr->irsvdperkey = 8;
		tvbptr->iquadsize = 8;
		duplen = 8;
		if (idxblk <= 0)
			idxblk = V_DFLT_NODE_LENGTH / 512;
	} else if (imode & ISMCISAM) {
		tvbptr->irsvdperkey = 4;
		tvbptr->iquadsize = 4;
		tvbptr->iformat = C_ISAM_FILE;
		if (idxblk <= 0)
			idxblk = C_DFLT_NODE_LENGTH / 512;
	} else {						   /* C-ISAM */
		tvbptr->irsvdperkey = 4;
		tvbptr->iquadsize = 4;
		tvbptr->iformat = C_ISAM_FILE;
		if (idxblk <= 0)
			idxblk = C_DFLT_NODE_LENGTH / 512;
	}
	if (idxblk > MAX_NODE_LENGTH / 512) {
		idxblk = MAX_NODE_LENGTH / 512;
	}

	/* Setup root (dictionary) node (Node 1) */
	tvbptr->inodesize = idxblk * 512;
	tvbptr->iduplen = duplen;
	tvbptr->inkeys = 1;
	tvbptr->inodekeydesc = 2;
	tvbptr->idatafree = 0;
	tvbptr->inodefree = 0;
	tvbptr->inodefree = 0;
	tvbptr->inodeaudit = 0;
	tvbptr->itransnumber = 1;
	tvbptr->iuniqueid = 1;
	if (pskey->k_nparts) {
		tvbptr->inodecount = 3;
	} else {
		tvbptr->inodecount = 2;
	}
	if (imode & ISVARLEN) {
		tvbptr->imaxrowlength = imaxrowlength;
	} else {
		tvbptr->imaxrowlength = 0;
	}
	if (ivbwritedict (ihandle)) {
		vvbfreedict (tvbptr);
		vb_rtd->psvbfile[ihandle] = NULL;
		return -1;
	}

	/* Setup first keydesc node (Node 2) */
	memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
	pctemp = cvbnodetmp;
	pctemp += INTSIZE;
	inl_stcompx ((off_t) 0, pctemp, tvbptr->iquadsize);	/* Next keydesc node */
	pctemp += tvbptr->iquadsize;
	/* keydesc length */
	inl_stint (INTSIZE + tvbptr->iquadsize + 1 +
			   (((INTSIZE * 2) + 1) * pskey->k_nparts), pctemp);
	pctemp += INTSIZE;
	if (pskey->k_nparts) {
		inl_stcompx ((off_t) 3, pctemp, tvbptr->iquadsize);	/* Root node for this key */
	} else {
		inl_stcompx ((off_t) 0, pctemp, tvbptr->iquadsize);	/* Root node for this key */
	}
	pctemp += tvbptr->iquadsize;
	*pctemp = pskey->k_flags / 2;	   /* Compression / Dups flags */
	pctemp++;
	for (iloop = 0; iloop < pskey->k_nparts; iloop++) {
		inl_stint (pskey->k_part[iloop].kp_leng, pctemp);	/* Length */
		if (iloop == 0 && (pskey->k_flags & ISDUPS)) {
			*pctemp |= 0x80;
		}
		pctemp += INTSIZE;
		inl_stint (pskey->k_part[iloop].kp_start, pctemp);	/* Offset */
		pctemp += INTSIZE;
		*pctemp = pskey->k_part[iloop].kp_type;	/* Type */
		pctemp++;
	}
	inl_stint (pctemp - cvbnodetmp, cvbnodetmp);	/* Length used */
	inl_stint (0xff7e, cvbnodetmp + tvbptr->inodesize - 3);
	*(cvbnodetmp + tvbptr->inodesize - 1) = 0;
#if 0								   /* DISAM no longer adds LF to index blocks */
	if (tvbptr->iformat == C_ISAM_FILE) {
		*(cvbnodetmp + tvbptr->inodesize - 1) = '\n';
	}
#endif
	if (ivbblockwrite (ihandle, 1, (off_t) 2, cvbnodetmp)) {
		vvbfreedict (tvbptr);
		vb_rtd->psvbfile[ihandle] = NULL;
		return -1;
	}

	if (pskey->k_nparts) {
		/* Setup key root node (Node 3) */
		memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
		if (tvbptr->iformat == V_ISAM_FILE) {
			inl_stint (INTSIZE + tvbptr->iquadsize, cvbnodetmp);
			inl_stcompx ((off_t) 1, cvbnodetmp + INTSIZE, tvbptr->iquadsize);	/* Transaction number */
		} else {
			inl_stint (INTSIZE, cvbnodetmp);
		}
		if (ivbblockwrite (ihandle, 1, (off_t) 3, cvbnodetmp)) {
			vvbfreedict (tvbptr);
			vb_rtd->psvbfile[ihandle] = NULL;
			return -1;
		}
	}

	tvbptr->iisopen = 0;			   /* Mark it as FULLY open */
	if (imode & ISEXCLLOCK) {
		ivbfileopenlock (ihandle, 2);
	} else {
		ivbfileopenlock (ihandle, 1);
	}
	isclose (ihandle);
	ivbclose3 (ihandle);
	vb_rtd->iserrno = 0;
	ivbtransbuild (pcfilename, iminrowlength, imaxrowlength, pskey, imode);
	return isopen (pcfilename, imode);

  build_err:
	if (vb_rtd->psvbfile[ihandle] != NULL) {
		vvbfreedict (vb_rtd->psvbfile[ihandle]);
	}
	vb_rtd->psvbfile[ihandle] = NULL;
	if (errno != 0 && vb_rtd->iserrno == 0)
		vb_rtd->iserrno = errno;
	vbsetstatus ();
	return -1;
}

int
isaddindex (int ihandle, struct keydesc *pskeydesc)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	int         iresult, ikeynumber;

	if (ivbenter (ihandle, 1)) {
		return -1;
	}

	iresult = -1;
	vb_rtd->iserrno = ENOTEXCL;
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (!(psvbptr->iopenmode & ISEXCLLOCK)) {
		goto addindexexit;
	}
	vb_rtd->iserrno = EKEXISTS;
	ikeynumber = ivbcheckkey (ihandle, pskeydesc, 1, 0, 0);
	if (ikeynumber == -1) {
		goto addindexexit;
	}
	ikeynumber = VBiaddkeydescriptor (ihandle, pskeydesc);
	if (ikeynumber) {
		goto addindexexit;
	}
	psvbptr->iisdictlocked |= 0x02;
	ikeynumber = ivbcheckkey (ihandle, pskeydesc, 1, 0, 0);
	if (ikeynumber < 0) {
		goto addindexexit;
	}
	for (ikeynumber = 0; ikeynumber < MAXSUBS && psvbptr->pskeydesc[ikeynumber];
		 ikeynumber++);
	vb_rtd->iserrno = ETOOMANY;
	if (ikeynumber >= MAXSUBS) {
		goto addindexexit;
	}
	ikeynumber = psvbptr->inkeys;
	psvbptr->pskeydesc[ikeynumber] = pvvbmalloc (sizeof (struct keydesc));
	psvbptr->inkeys++;
	vb_rtd->iserrno = errno;
	if (!psvbptr->pskeydesc[ikeynumber]) {
		goto addindexexit;
	}
	memcpy (psvbptr->pskeydesc[ikeynumber], pskeydesc, sizeof (struct keydesc));
	if (imakekeysfromdata (ihandle, ikeynumber)) {
/* BUG - Handle this better! */
		iresult = vb_rtd->iserrno;
		ivbexit (ihandle);
		isdelindex (ihandle, pskeydesc);
		vb_rtd->iserrno = iresult;
		goto addindexexit;
	}
	vb_rtd->iserrno = 0;
	iresult = ivbtranscreateindex (ihandle, pskeydesc);

  addindexexit:
	iresult |= ivbexit (ihandle);
	if (iresult) {
		return -1;
	}
	return 0;
}

int
isdelindex (int ihandle, struct keydesc *pskeydesc)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;
	off_t       trootnode;
	int         iresult = -1, ikeynumber, iloop;

	if (ivbenter (ihandle, 1)) {
		return -1;
	}

	psvbfptr = vb_rtd->psvbfile[ihandle];
	if (!(psvbfptr->iopenmode & ISEXCLLOCK)) {
		vb_rtd->iserrno = ENOTEXCL;
		goto delindexexit;
	}
	ikeynumber = ivbcheckkey (ihandle, pskeydesc, 2, 0, 0);
	if (ikeynumber == -1) {
		vb_rtd->iserrno = EKEXISTS;
		goto delindexexit;
	}
	if (!ikeynumber && !(pskeydesc->k_flags & 0x80)) {
		vb_rtd->iserrno = EPRIMKEY;
		goto delindexexit;
	}
	trootnode = tdelkeydescriptor (ihandle, pskeydesc, ikeynumber);
	if (trootnode < 1) {
		goto delindexexit;
	}
	if (idelnodes (ihandle, ikeynumber, trootnode)) {
		goto delindexexit;
	}
	vvbfree (psvbfptr->pskeydesc[ikeynumber], sizeof (struct keydesc));
	psvbfptr->pskeydesc[ikeynumber] = NULL;
	vvbtreeallfree (ihandle, ikeynumber, psvbfptr->pstree[ikeynumber]);
	vvbkeyunmalloc (ihandle, ikeynumber);
	for (iloop = ikeynumber; iloop < MAXSUBS; iloop++) {
		psvbfptr->pskeydesc[iloop] = psvbfptr->pskeydesc[iloop + 1];
		psvbfptr->pstree[iloop] = psvbfptr->pstree[iloop + 1];
		psvbfptr->pskeyfree[iloop] = psvbfptr->pskeyfree[iloop + 1];
		psvbfptr->pskeycurr[iloop] = psvbfptr->pskeycurr[iloop + 1];
	}
	psvbfptr->pskeydesc[MAXSUBS - 1] = NULL;
	psvbfptr->pstree[MAXSUBS - 1] = NULL;
	psvbfptr->pskeyfree[MAXSUBS - 1] = NULL;
	psvbfptr->pskeycurr[MAXSUBS - 1] = NULL;
	psvbfptr->inkeys--;
	psvbfptr->iisdictlocked |= 0x02;
	iresult = ivbtransdeleteindex (ihandle, pskeydesc);

  delindexexit:
	iresult |= ivbexit (ihandle);
	return iresult;
}
