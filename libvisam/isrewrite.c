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

/* Key mutation states for rollback tracking (P1-1 fix) */
#define KEY_UNCHANGED  0   /* key not modified — no rollback needed     */
#define KEY_DELETED    1   /* old key removed, new key NOT yet inserted */
#define KEY_UPDATED    2   /* old key removed, new key inserted         */

static int
irowupdate (const int ihandle, VB_CHAR * pcrow, off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct VBKEY *pskey;
	struct DICTINFO *psvbfptr;
	struct keydesc *pskeyptr;
	off_t       tdupnumber = 0;
	int         ikeynumber, iresult;
	char        keypresent[64];
	VB_UCHAR    ckeyvalue[VB_MAX_KEYLEN];
	/* P1-1: track mutation state of each index for correct rollback */
	int         ikeystate[MAXSUBS];
	memset (ikeystate, KEY_UNCHANGED, sizeof (ikeystate));

	psvbfptr = vb_rtd->psvbfile[ihandle];
	/*
	 * Step 1:
	 *      For each index that's changing, confirm that the NEW value
	 *      doesn't conflict with an existing ISNODUPS flag.
	 */
	for (ikeynumber = 0; ikeynumber < psvbfptr->inkeys; ikeynumber++) {
		pskeyptr = psvbfptr->pskeydesc[ikeynumber];
		if (pskeyptr->k_nparts == 0) {
			continue;
		}
		if (pskeyptr->k_flags & ISDUPS) {
			continue;
		}
		vvbmakekey (pskeyptr, pcrow, ckeyvalue);
		iresult =
			ivbkeysearch (ihandle, ISGTEQ, ikeynumber, 0, ckeyvalue, (off_t) 0);
		if (iresult != 1 || trownumber == psvbfptr->pskeycurr[ikeynumber]->trownode
			|| psvbfptr->pskeycurr[ikeynumber]->iisdummy) {
			continue;
		}
		vb_rtd->iserrno = EDUPL;
		return -1;
	}

	/*
	 * Step 2:
	 *      Check each index for existance of trownumber
	 *      This 'preload' additionally helps determine which indexes change
	 */
	for (ikeynumber = 0; ikeynumber < psvbfptr->inkeys; ikeynumber++) {
		pskeyptr = psvbfptr->pskeydesc[ikeynumber];
		if (pskeyptr->k_nparts == 0) {
			continue;
		}
		iresult = ivbkeylocaterow (ihandle, ikeynumber, trownumber);
		if (iresult) {
			keypresent[ikeynumber] = 0;
			if ((pskeyptr->k_flags & NULLKEY))
				continue;
			vb_rtd->iserrno = EBADFILE;
			return -1;
		}
		keypresent[ikeynumber] = 1;
	}

	/*
	 * Step 3:
	 *      Perform the actual deletion / insertion with each index
	 *      But *ONLY* for those indexes that have actually CHANGED!
	 */
	vb_rtd->isstat2 = '0';
	for (ikeynumber = 0; ikeynumber < psvbfptr->inkeys; ikeynumber++) {
		pskeyptr = psvbfptr->pskeydesc[ikeynumber];
		if (pskeyptr->k_nparts == 0) {
			continue;
		}
		/* pcrow is the UPDATED key! */
		vvbmakekey (pskeyptr, pcrow, ckeyvalue);
		if (keypresent[ikeynumber] == 0) {
			iresult = 0;
		} else {
			iresult = memcmp (ckeyvalue, psvbfptr->pskeycurr[ikeynumber]->ckey,
							  (size_t) pskeyptr->k_len);
			/* If NEW key is DIFFERENT than CURRENT, remove CURRENT */
			if (iresult) {
				iresult = ivbkeydelete (ihandle, ikeynumber);
			} else {
				continue;
			}
			if (iresult) {
				/*
				 * P1-1 FIX: ivbkeydelete failed for ikeynumber.
				 * That index's old key is still present — do not touch it.
				 * For every index already in KEY_UPDATED state (old removed,
				 * new inserted), reverse the operation: delete the new key
				 * and re-insert the original key from ppcrowbuffer.
				 */
				int irestore;
				VB_UCHAR coldkey[VB_MAX_KEYLEN];
				for (irestore = 0; irestore < ikeynumber; irestore++) {
					if (ikeystate[irestore] != KEY_UPDATED) {
						continue;
					}
					struct keydesc *psrkey = psvbfptr->pskeydesc[irestore];
					/* Remove the new key we just inserted */
					vvbmakekey (psrkey, pcrow, ckeyvalue);
					ivbkeydelete (ihandle, irestore);
					/* Re-insert the original key */
					vvbmakekey (psrkey, psvbfptr->ppcrowbuffer, coldkey);
					ivbkeysearch (ihandle, ISGTEQ, irestore, 0, coldkey, (off_t) 0);
					ivbkeyinsert (ihandle, NULL, irestore, coldkey,
								  trownumber,
								  psvbfptr->pskeycurr[irestore]->tdupnumber,
								  NULL);
					ikeystate[irestore] = KEY_UNCHANGED;
				}
				vb_rtd->iserrno = EBADFILE;
				return -1;
			}
			ikeystate[ikeynumber] = KEY_DELETED;
			iresult =
				ivbkeysearch (ihandle, ISGREAT, ikeynumber, 0, ckeyvalue, (off_t) 0);
		}
		tdupnumber = 0;
		if (iresult >= 0) {
			vb_rtd->nodupchk = 1;
			iresult = ivbkeyload (ihandle, ikeynumber, ISPREV, 1, &pskey);
			vb_rtd->nodupchk = 0;
			if (!iresult) {
				if (pskeyptr->k_flags & ISDUPS
					&& !memcmp (pskey->ckey, ckeyvalue, (size_t) pskeyptr->k_len)) {
					tdupnumber = pskey->tdupnumber + 1;
					vb_rtd->isstat2 = '2';
				}
				psvbfptr->pskeycurr[ikeynumber] =
					psvbfptr->pskeycurr[ikeynumber]->psnext;
				psvbfptr->pskeycurr[ikeynumber]->psparent->pskeycurr =
					psvbfptr->pskeycurr[ikeynumber];
			}
			iresult = ivbkeysearch (ihandle, ISGTEQ, ikeynumber, 0, ckeyvalue,
									tdupnumber);
			iresult = ivbkeyinsert (ihandle, NULL, ikeynumber, ckeyvalue,
									trownumber, tdupnumber, NULL);
		}
		ikeystate[ikeynumber] = KEY_UPDATED;
		if (iresult) {
			/*
			 * P1-1 FIX: ivbkeyinsert failed for ikeynumber.
			 * ikeystate[ikeynumber] = KEY_DELETED (old removed, new not added).
			 * All indexes 0..ikeynumber-1 marked KEY_UPDATED need reversal:
			 * delete the new key, re-insert the original from ppcrowbuffer.
			 * For ikeynumber itself: only re-insert old (new was never added).
			 */
			int irestore;
			VB_UCHAR coldkey[VB_MAX_KEYLEN];
			for (irestore = 0; irestore <= ikeynumber; irestore++) {
				if (ikeystate[irestore] == KEY_UNCHANGED) {
					continue;
				}
				struct keydesc *psrkey = psvbfptr->pskeydesc[irestore];
				if (ikeystate[irestore] == KEY_UPDATED) {
					/* Remove the new key we successfully inserted */
					vvbmakekey (psrkey, pcrow, ckeyvalue);
					ivbkeydelete (ihandle, irestore);
				}
				/* Re-insert the original key from the saved row buffer */
				vvbmakekey (psrkey, psvbfptr->ppcrowbuffer, coldkey);
				ivbkeysearch (ihandle, ISGTEQ, irestore, 0, coldkey, (off_t) 0);
				ivbkeyinsert (ihandle, NULL, irestore, coldkey,
							  trownumber,
							  psvbfptr->pskeycurr[irestore]->tdupnumber,
							  NULL);
				ikeystate[irestore] = KEY_UNCHANGED;
			}
			vb_rtd->iserrno = EBADFILE;
			return iresult;
		}
	}

	vb_rtd->iserrno = 0;
	return 0;
}

/* Global functions */

int
isrewrite (int ihandle, VB_CHAR * pcrow)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;
	off_t       trownumber;
	int         ideleted, inewreclen, iresult = 0;
#if (VBLOGGING == 1)
	int         ioldreclen = 0;
#endif
	VB_UCHAR    ckeyvalue[VB_MAX_KEYLEN];

	if (ivbenter (ihandle, 1)) {
		return -1;
	}
	psvbfptr = vb_rtd->psvbfile[ihandle];
	if ((psvbfptr->iopenmode & ISVARLEN)
		&& (vb_rtd->isreclen > psvbfptr->imaxrowlength
			|| vb_rtd->isreclen < psvbfptr->iminrowlength)) {
		ivbexit (ihandle);
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}

	inewreclen = vb_rtd->isreclen;
	if (psvbfptr->pskeydesc[0]->k_flags & ISDUPS) {
		iresult = -1;
		vb_rtd->iserrno = ENOREC;
	} else {
		vvbmakekey (psvbfptr->pskeydesc[0], pcrow, ckeyvalue);
		iresult = ivbkeysearch (ihandle, ISEQUAL, 0, 0, ckeyvalue, (off_t) 0);
		switch (iresult) {
		case 1:					   /* Exact match */
			iresult = 0;
			psvbfptr->iisdictlocked |= 0x02;
			trownumber = psvbfptr->pskeycurr[0]->trownode;
			if (psvbfptr->iopenmode & ISTRANS) {
				vb_rtd->iserrno = ivbdatalock (ihandle, VBWRLOCK, trownumber);
				if (vb_rtd->iserrno) {
					iresult = -1;
					goto isrewrite_exit;
				}
			}
			vb_rtd->iserrno = ivbdataread (ihandle, (void *) psvbfptr->ppcrowbuffer,
										   &ideleted, trownumber);
			if (!vb_rtd->iserrno && ideleted) {
				vb_rtd->iserrno = ENOREC;
			}
			if (vb_rtd->iserrno) {
				iresult = -1;
#if (VBLOGGING == 1)
			} else {
				ioldreclen = vb_rtd->isreclen;
#endif
			}

			if (!iresult) {
				iresult = irowupdate (ihandle, pcrow, trownumber);
			}
			if (!iresult) {
				vb_rtd->isrecnum = trownumber;
				vb_rtd->isreclen = inewreclen;
				iresult =
					ivbdatawrite (ihandle, (void *) pcrow, 0,
								  (off_t) vb_rtd->isrecnum);
			}
#if (VBLOGGING == 1)
			if (!iresult) {
				if (psvbfptr->iopenmode & ISVARLEN) {
					iresult =
						ivbtransupdate (ihandle, trownumber, ioldreclen,
										inewreclen, pcrow);
				} else {
					iresult =
						ivbtransupdate (ihandle, trownumber,
										psvbfptr->iminrowlength,
										psvbfptr->iminrowlength, pcrow);
				}
			}
#endif
			break;

		case 0:					   /* LESS than */
		case 2:					   /* GREATER than */
		case 3:					   /* EMPTY file */
			vb_rtd->iserrno = ENOREC;
			iresult = -1;
			break;

		default:
			vb_rtd->iserrno = EBADFILE;
			iresult = -1;
			break;
		}
	}

  isrewrite_exit:
	iresult |= ivbexit (ihandle);
	return iresult;
}

int
isrewcurr (int ihandle, VB_CHAR * pcrow)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;
	int         ideleted, inewreclen, iresult = 0;
#if (VBLOGGING == 1)
	int         ioldreclen = 0;
#endif

	if (ivbenter (ihandle, 1)) {
		return -1;
	}

	psvbfptr = vb_rtd->psvbfile[ihandle];
	if ((psvbfptr->iopenmode & ISVARLEN)
		&& (vb_rtd->isreclen > psvbfptr->imaxrowlength
			|| vb_rtd->isreclen < psvbfptr->iminrowlength)) {
		ivbexit (ihandle);
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}

	inewreclen = vb_rtd->isreclen;
	if (psvbfptr->trownumber > 0) {
		if (psvbfptr->iopenmode & ISTRANS) {
			vb_rtd->iserrno = ivbdatalock (ihandle, VBWRLOCK, psvbfptr->trownumber);
			if (vb_rtd->iserrno) {
				iresult = -1;
				goto isrewcurr_exit;
			}
		}
		vb_rtd->iserrno =
			ivbdataread (ihandle, (void *) psvbfptr->ppcrowbuffer, &ideleted,
						 psvbfptr->trownumber);
		if (!vb_rtd->iserrno && ideleted) {
			vb_rtd->iserrno = ENOREC;
#if (VBLOGGING == 1)
		} else {
			ioldreclen = vb_rtd->isreclen;
#endif
		}
		if (vb_rtd->iserrno) {
			iresult = -1;
		}
		if (!iresult) {
			iresult = irowupdate (ihandle, pcrow, psvbfptr->trownumber);
		}
		if (!iresult) {
			vb_rtd->isrecnum = psvbfptr->trownumber;
			vb_rtd->isreclen = inewreclen;
			iresult =
				ivbdatawrite (ihandle, (void *) pcrow, 0, (off_t) vb_rtd->isrecnum);
		}
#if (VBLOGGING == 1)
		if (!iresult) {
			if (psvbfptr->iopenmode & ISVARLEN) {
				iresult =
					ivbtransupdate (ihandle, psvbfptr->trownumber,
									ioldreclen, inewreclen, pcrow);
			} else {
				iresult =
					ivbtransupdate (ihandle, psvbfptr->trownumber,
									psvbfptr->iminrowlength,
									psvbfptr->iminrowlength, pcrow);
			}
		}
#endif
	}
  isrewcurr_exit:
	psvbfptr->iisdictlocked |= 0x02;
	iresult |= ivbexit (ihandle);
	return iresult;
}

int
isrewrec (int ihandle, vbisam_rec_n trownumber, VB_CHAR * pcrow)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;
	int         ideleted, inewreclen, iresult = 0;
#if (VBLOGGING == 1)
	int         ioldreclen = 0;
#endif

	if (ivbenter (ihandle, 1)) {
		return -1;
	}

	psvbfptr = vb_rtd->psvbfile[ihandle];
	if ((psvbfptr->iopenmode & ISVARLEN)
		&& (vb_rtd->isreclen > psvbfptr->imaxrowlength
			|| vb_rtd->isreclen < psvbfptr->iminrowlength)) {
		ivbexit (ihandle);
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}

	inewreclen = vb_rtd->isreclen;
	if (trownumber < 1) {
		iresult = -1;
		vb_rtd->iserrno = ENOREC;
	} else {
		if (psvbfptr->iopenmode & ISTRANS) {
			vb_rtd->iserrno = ivbdatalock (ihandle, VBWRLOCK, trownumber);
			if (vb_rtd->iserrno) {
				iresult = -1;
				goto isrewrec_exit;
			}
		}
		vb_rtd->iserrno =
			ivbdataread (ihandle, (void *) psvbfptr->ppcrowbuffer, &ideleted,
						 trownumber);
		if (!vb_rtd->iserrno && ideleted) {
			vb_rtd->iserrno = ENOREC;
		}
		if (vb_rtd->iserrno) {
			iresult = -1;
#if (VBLOGGING == 1)
		} else {
			ioldreclen = vb_rtd->isreclen;
#endif
		}
		if (!iresult) {
			iresult = irowupdate (ihandle, pcrow, trownumber);
		}
		if (!iresult) {
			vb_rtd->isrecnum = trownumber;
			vb_rtd->isreclen = inewreclen;
			iresult =
				ivbdatawrite (ihandle, (void *) pcrow, 0, (off_t) vb_rtd->isrecnum);
		}
#if (VBLOGGING == 1)
		if (!iresult) {
			if (psvbfptr->iopenmode & ISVARLEN) {
				iresult = ivbtransupdate (ihandle, trownumber,
										  ioldreclen, inewreclen, pcrow);
			} else {
				iresult = ivbtransupdate (ihandle, trownumber,
										  psvbfptr->iminrowlength,
										  psvbfptr->iminrowlength, pcrow);
			}
		}
#endif
	}
  isrewrec_exit:
	if (!iresult) {
		psvbfptr->iisdictlocked |= 0x02;
	}
	iresult |= ivbexit (ihandle);
	return iresult;
}
