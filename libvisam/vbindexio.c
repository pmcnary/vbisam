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

#include	"isinternal.h"

/* Local functions */

static      off_t
tvbdatacountgetnext (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       tvalue;

	tvbptr = vb_rtd->psvbfile[ihandle];
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	tvalue = tvbptr->idatacount + 1;
	tvbptr->idatacount = tvalue;
	tvbptr->iisdictlocked |= 0x02;
	return tvalue;
}

/* Global functions */

off_t
tvbnodecountgetnext (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       tvalue;

	tvbptr = vb_rtd->psvbfile[ihandle];
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	tvalue = tvbptr->inodecount + 1;
	tvbptr->inodecount = tvalue;
	tvbptr->iisdictlocked |= 0x02;
	return tvalue;
}

int
ivbnodefree (const int ihandle, off_t tnodenumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       theadnode;
	int         ilengthused, iresult;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];
	VB_CHAR     cvbnodetmp2[MAX_NODE_LENGTH];

	/* Sanity check - Is ihandle a currently open table? */
	vb_rtd->iserrno = ENOTOPEN;
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (!tvbptr) {
		return -1;
	}
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	memset (cvbnodetmp2, 0, (size_t) tvbptr->inodesize);
	inl_stint (INTSIZE, cvbnodetmp2);

	theadnode = tvbptr->inodefree;
	/* If the list is empty, node tnodenumber becomes the whole list */
	if (theadnode == 0) {
		inl_stint (INTSIZE + tvbptr->iquadsize, cvbnodetmp2);
		inl_stcompx ((off_t) 0, cvbnodetmp2 + INTSIZE, tvbptr->iquadsize);
		cvbnodetmp2[tvbptr->inodesize - 2] = 0x7f;
		cvbnodetmp2[tvbptr->inodesize - 3] = -2;
		iresult = ivbblockwrite (ihandle, 1, tnodenumber, cvbnodetmp2);
		if (iresult) {
			return iresult;
		}
		tvbptr->inodefree = tnodenumber;
		tvbptr->iisdictlocked |= 0x02;
		return 0;
	}

	/* Read in the head of the current free list */
	iresult = ivbblockread (ihandle, 1, theadnode, cvbnodetmp);
	if (iresult) {
		return iresult;
	}
	if (tvbptr->iformat == V_ISAM_FILE && cvbnodetmp[tvbptr->inodesize - 2] != 0x7f) {
		return EBADFILE;
	}
	if (cvbnodetmp[tvbptr->inodesize - 3] != -2) {
		return EBADFILE;
	}
	ilengthused = inl_ldint (cvbnodetmp);
	if (ilengthused >= tvbptr->inodesize - (tvbptr->iquadsize + 3)) {
		/* If there was no space left, tnodenumber becomes the head */
		cvbnodetmp2[tvbptr->inodesize - 2] = 0x7f;
		cvbnodetmp2[tvbptr->inodesize - 3] = -2;
		inl_stint (INTSIZE + tvbptr->iquadsize, cvbnodetmp2);
		inl_stcompx (theadnode, &cvbnodetmp2[INTSIZE], tvbptr->iquadsize);
		iresult = ivbblockwrite (ihandle, 1, tnodenumber, cvbnodetmp2);
		if (!iresult) {
			tvbptr->inodefree = tnodenumber;
			tvbptr->iisdictlocked |= 0x02;
		}
		return iresult;
	}

	/* If we got here, there's space left in the theadnode to store it */
	cvbnodetmp2[tvbptr->inodesize - 2] = 0x7f;
	cvbnodetmp2[tvbptr->inodesize - 3] = 0xfe;
	iresult = ivbblockwrite (ihandle, 1, tnodenumber, cvbnodetmp2);
	if (iresult) {
		return iresult;
	}
	inl_stcompx (tnodenumber, &cvbnodetmp[ilengthused], tvbptr->iquadsize);
	ilengthused += tvbptr->iquadsize;
	inl_stint (ilengthused, cvbnodetmp);
	iresult = ivbblockwrite (ihandle, 1, theadnode, cvbnodetmp);

	return iresult;
}

int
ivbdatafree (const int ihandle, off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       theadnode, tnodenumber;
	int         ilengthused, iresult;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];
	/* Sanity check - Is ihandle a currently open table? */
	vb_rtd->iserrno = ENOTOPEN;
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (!tvbptr) {
		return -1;
	}
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	if (tvbptr->idatacount == trownumber) {
		tvbptr->idatacount = trownumber - 1;
		tvbptr->iisdictlocked |= 0x02;
		return 0;
	}

	theadnode = tvbptr->idatafree;
	if (theadnode != 0) {
		iresult = ivbblockread (ihandle, 1, theadnode, cvbnodetmp);
		if (iresult) {
			return iresult;
		}
		if (tvbptr->iformat == V_ISAM_FILE
		 && cvbnodetmp[tvbptr->inodesize - 2] != 0x7f) {
			return EBADFILE;
		}
		if (cvbnodetmp[tvbptr->inodesize - 3] != -1) {
			return EBADFILE;
		}
		ilengthused = inl_ldint (cvbnodetmp);
		if (ilengthused < tvbptr->inodesize - (tvbptr->iquadsize + 3)) {
			/* We need to add trownumber to the current node */
			inl_stcompx ((off_t) trownumber, cvbnodetmp + ilengthused,
						 tvbptr->iquadsize);
			ilengthused += tvbptr->iquadsize;
			inl_stint (ilengthused, cvbnodetmp);
			iresult = ivbblockwrite (ihandle, 1, theadnode, cvbnodetmp);
			return iresult;
		}
	}
	/* We need to allocate a new row-free node! */
	/* We append any existing nodes using the next pointer from the new node */
	tnodenumber = tvbnodeallocate (ihandle);
	if (tnodenumber == (off_t) - 1) {
		return vb_rtd->iserrno;
	}
	memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
	cvbnodetmp[tvbptr->inodesize - 3] = -1;
	cvbnodetmp[tvbptr->inodesize - 2] = 0x7f;
	inl_stint (INTSIZE + (2 * tvbptr->iquadsize), cvbnodetmp);
	inl_stcompx (theadnode, &cvbnodetmp[INTSIZE], tvbptr->iquadsize);
	inl_stcompx (trownumber, &cvbnodetmp[INTSIZE + tvbptr->iquadsize],
				 tvbptr->iquadsize);
	iresult = ivbblockwrite (ihandle, 1, tnodenumber, cvbnodetmp);
	if (iresult) {
		return iresult;
	}
	tvbptr->idatafree = tnodenumber;
	tvbptr->iisdictlocked |= 0x02;
	return 0;
}

off_t
tvbnodeallocate (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       theadnode, tvalue;
	int         ilengthused;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];
	/* Sanity check - Is ihandle a currently open table? */
	vb_rtd->iserrno = ENOTOPEN;
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (!tvbptr) {
		return -1;
	}
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	/* If there's *ANY* nodes in the free list, use them first! */
	theadnode = tvbptr->inodefree;
	if (theadnode != 0) {
		vb_rtd->iserrno = ivbblockread (ihandle, 1, theadnode, cvbnodetmp);
		if (vb_rtd->iserrno) {
			return -1;
		}
		vb_rtd->iserrno = EBADFILE;
		if (tvbptr->iformat == V_ISAM_FILE
		 && cvbnodetmp[tvbptr->inodesize - 2] != 0x7f) {
			return -1;
		}
		if (cvbnodetmp[tvbptr->inodesize - 3] != -2) {
			return -1;
		}
		ilengthused = inl_ldint (cvbnodetmp);
		if (ilengthused > (INTSIZE + tvbptr->iquadsize)) {
			tvalue =
				inl_ldcompx (cvbnodetmp + INTSIZE + tvbptr->iquadsize,
							 tvbptr->iquadsize);
			memcpy (cvbnodetmp + INTSIZE + tvbptr->iquadsize,
					cvbnodetmp + INTSIZE + tvbptr->iquadsize + tvbptr->iquadsize,
					(size_t) (ilengthused -
							  (INTSIZE + tvbptr->iquadsize + tvbptr->iquadsize)));
			ilengthused -= tvbptr->iquadsize;
			memset (cvbnodetmp + ilengthused, 0, tvbptr->iquadsize);
			inl_stint (ilengthused, cvbnodetmp);
			vb_rtd->iserrno = ivbblockwrite (ihandle, 1, theadnode, cvbnodetmp);
			if (vb_rtd->iserrno) {
				return -1;
			}
			return tvalue;
		}
		/* If it's last entry in the node, use the node itself! */
		tvalue = inl_ldcompx (cvbnodetmp + INTSIZE, tvbptr->iquadsize);
		tvbptr->inodefree = tvalue;
		tvbptr->iisdictlocked |= 0x02;
		return theadnode;
	}
	/* If we get here, we need to allocate a NEW node. */
	/* Since we already hold a dictionary lock, we don't need another */
	tvalue = tvbnodecountgetnext (ihandle);
	return tvalue;
}

off_t
tvbdataallocate (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       theadnode, tnextnode, tvalue;
	int         ilengthused, iresult;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	tvbptr = vb_rtd->psvbfile[ihandle];
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	/* If there's *ANY* rows in the free list, use them first! */
	theadnode = tvbptr->idatafree;
	while (theadnode != 0) {
		vb_rtd->iserrno = ivbblockread (ihandle, 1, theadnode, cvbnodetmp);
		if (vb_rtd->iserrno) {
			return -1;
		}
		vb_rtd->iserrno = EBADFILE;
		if (tvbptr->iformat == V_ISAM_FILE
		 && cvbnodetmp[tvbptr->inodesize - 2] != 0x7f) {
			return -1;
		}
		if (cvbnodetmp[tvbptr->inodesize - 3] != -1) {
			return -1;
		}
		ilengthused = inl_ldint (cvbnodetmp);
		if (ilengthused > INTSIZE + tvbptr->iquadsize) {
			tvbptr->iisdictlocked |= 0x02;
			ilengthused -= tvbptr->iquadsize;
			inl_stint (ilengthused, cvbnodetmp);
			tvalue = inl_ldcompx (&cvbnodetmp[ilengthused], tvbptr->iquadsize);
			inl_stcompx ((off_t) 0, &cvbnodetmp[ilengthused], tvbptr->iquadsize);
			if (ilengthused > INTSIZE + tvbptr->iquadsize) {
				vb_rtd->iserrno = ivbblockwrite (ihandle, 1, theadnode, cvbnodetmp);
				if (vb_rtd->iserrno) {
					return -1;
				}
				return tvalue;
			}
			/* If we're using the last entry in the node, advance */
			tnextnode = inl_ldcompx (&cvbnodetmp[INTSIZE], tvbptr->iquadsize);
			iresult = ivbnodefree (ihandle, theadnode);
			if (iresult) {
				return -1;
			}
			tvbptr->idatafree = tnextnode;
			return tvalue;
		}
		/* Ummmm, this is an INTEGRITY ERROR of sorts! */
		/* However, let's fix it anyway! */
		tnextnode = inl_ldcompx (&cvbnodetmp[INTSIZE], tvbptr->iquadsize);
		iresult = ivbnodefree (ihandle, theadnode);
		if (iresult) {
			return -1;
		}
		tvbptr->idatafree = tnextnode;
		tvbptr->iisdictlocked |= 0x02;
		theadnode = tnextnode;
	}
	/* If we get here, we need to allocate a NEW row number. */
	/* Since we already hold a dictionary lock, we don't need another */
	tvalue = tvbdatacountgetnext (ihandle);
	return tvalue;
}

int
ivbforcedataallocate (const int ihandle, off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       theadnode, tprevnode, tnextnode;
	int         iloop, ilengthused;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	/* Sanity check - Is ihandle a currently open table? */
	vb_rtd->iserrno = ENOTOPEN;
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (!tvbptr) {
		return -1;
	}
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		return -1;
	}
	vb_rtd->iserrno = 0;

	/* Test 1: Is it already beyond EOF (the SIMPLE test) */
	theadnode = tvbptr->idatacount;
	if (theadnode < trownumber) {
		tvbptr->iisdictlocked |= 0x02;
		tvbptr->idatacount = trownumber;
		theadnode++;
		while (theadnode < trownumber) {
			if (theadnode != 0) {
				ivbdatafree (ihandle, theadnode);
			}
			theadnode++;
		}
		return 0;
	}
	/* <SIGH> It SHOULD be *SOMEWHERE* in the data free list! */
	tprevnode = 0;
	theadnode = tvbptr->idatafree;
	while (theadnode != 0) {
		vb_rtd->iserrno = ivbblockread (ihandle, 1, theadnode, cvbnodetmp);
		if (vb_rtd->iserrno) {
			return -1;
		}
		vb_rtd->iserrno = EBADFILE;
		if (tvbptr->iformat == V_ISAM_FILE
		 && cvbnodetmp[tvbptr->inodesize - 2] != 0x7f) {
			return -1;
		}
		if (cvbnodetmp[tvbptr->inodesize - 3] != -1) {
			return -1;
		}
		ilengthused = inl_ldint (cvbnodetmp);
		for (iloop = INTSIZE + tvbptr->iquadsize; iloop < ilengthused;
			 iloop += tvbptr->iquadsize) {
			if (inl_ldcompx (&cvbnodetmp[iloop], tvbptr->iquadsize) == trownumber) {	/* Extract it */
				memcpy (&(cvbnodetmp[iloop]),
						&(cvbnodetmp[iloop + tvbptr->iquadsize]),
						(size_t) (ilengthused - iloop));
				ilengthused -= tvbptr->iquadsize;
				if (ilengthused > INTSIZE + tvbptr->iquadsize) {
					inl_stcompx ((off_t) 0, &cvbnodetmp[ilengthused],
								 tvbptr->iquadsize);
					inl_stint (ilengthused, cvbnodetmp);
					return ivbblockwrite (ihandle, 1, theadnode, cvbnodetmp);
				} else {			   /* It was the last one in the node! */
					tnextnode =
						inl_ldcompx (&cvbnodetmp[INTSIZE], tvbptr->iquadsize);
					if (tprevnode) {
						vb_rtd->iserrno =
							ivbblockread (ihandle, 1, tprevnode, cvbnodetmp);
						if (vb_rtd->iserrno) {
							return -1;
						}
						inl_stcompx (tnextnode, &cvbnodetmp[INTSIZE],
									 tvbptr->iquadsize);
						return ivbblockwrite (ihandle, 1, tprevnode, cvbnodetmp);
					} else {
						tvbptr->iisdictlocked |= 0x02;
						tvbptr->idatafree = tnextnode;
					}
					return ivbnodefree (ihandle, theadnode);
				}
			}
		}
		tprevnode = theadnode;
		theadnode = inl_ldcompx (&cvbnodetmp[INTSIZE], tvbptr->iquadsize);
	}
	/* If we get here, we've got a MAJOR integrity error in that the */
	/* nominated row number was simply *NOT FREE* */
	vb_rtd->iserrno = EBADFILE;
	return -1;
}

/*
 * Read the 'dictionary' block and move information to DICTINFO
 */
int
ivbreaddict (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	VB_UCHAR    buffer[256];
	int         iidxhandle, k;
	struct DICTINFO *tvbptr;
	struct VBDICTNODE *vbd;
	struct CIDICTNODE *cid;

	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADFILE;
		return -1;
	}
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (tvbptr == NULL) {
		vb_rtd->iserrno = EBADFILE;
		return -1;
	}
	iidxhandle = tvbptr->iindexhandle;

	/* Peek at header to allocate I/O buffer */
	if (tvbptr->pdictbuf == NULL || tvbptr->inodesize < sizeof (struct VBDICTNODE)) {
		memset (buffer, 0, 64);
		tvblseek (iidxhandle, 0, SEEK_SET);
		tvbread (iidxhandle, buffer, sizeof (buffer));
		if (buffer[0] == 'V' && buffer[1] == 'B') {	/* VB-ISAM format file */
			vbd = (struct VBDICTNODE *) buffer;
			tvbptr->inodesize = inl_ldint (vbd->cnodesize) + 1;
			if (tvbptr->inodesize < sizeof (struct VBDICTNODE)
				|| tvbptr->inodesize > MAX_NODE_LENGTH) {
				vb_rtd->iserrno = EBADFILE;
				return -1;
			}
			tvbptr->iformat = V_ISAM_FILE;
			if (tvbptr->pdictbuf == NULL) {
				tvbptr->pdictbuf = pvvbmalloc (tvbptr->inodesize);
			}
		} else if (buffer[0] == 0xFE && buffer[1] == 0x53) {	/* C-ISAM format file */
			cid = (struct CIDICTNODE *) buffer;
			tvbptr->inodesize = inl_ldint (cid->cnodesize) + 1;
			if (tvbptr->inodesize < sizeof (struct VBDICTNODE)
				|| tvbptr->inodesize > MAX_NODE_LENGTH) {
				vb_rtd->iserrno = EBADFILE;
				return -1;
			}
			tvbptr->iformat = C_ISAM_FILE;
			if (tvbptr->pdictbuf == NULL) {
				tvbptr->pdictbuf = pvvbmalloc (tvbptr->inodesize);
			}
		} else {
			vb_rtd->iserrno = EBADFILE;
			return -1;
		}
	}

	/* read the dictionary block and populate DICTINFO */
	tvblseek (iidxhandle, 0, SEEK_SET);
	tvbread (iidxhandle, tvbptr->pdictbuf, tvbptr->inodesize);
	if (tvbptr->pdictbuf[0] == 'V' && tvbptr->pdictbuf[1] == 'B') {
		vbd = (struct VBDICTNODE *) tvbptr->pdictbuf;
		tvbptr->iquadsize = 8;
		tvbptr->inodesize = inl_ldint (vbd->cnodesize) + 1;
		tvbptr->iformat = V_ISAM_FILE;
		tvbptr->irsvdperkey = vbd->crsvdperkey;
		tvbptr->inkeys = inl_ldint (vbd->cindexcount);
		tvbptr->iduplen = vbd->crfu2[1];
		if (tvbptr->iduplen != 2 && tvbptr->iduplen != 4 && tvbptr->iduplen != 8)
			tvbptr->iduplen = 8;
		tvbptr->iduplen = 8;		   /* VB-ISAM always has DUPLEN of 8 */
		tvbptr->iminrowlength = inl_ldint (vbd->cminrowlength);
		tvbptr->imaxrowlength = inl_ldint (vbd->cmaxrowlength);
		if (tvbptr->imaxrowlength == 0)
			tvbptr->imaxrowlength = tvbptr->iminrowlength;
		tvbptr->inodekeydesc = ld_compx (vbd->cnodekeydesc);
		tvbptr->idatafree = ld_compx (vbd->cdatafree);
		tvbptr->inodefree = ld_compx (vbd->cnodefree);
		tvbptr->idatacount = ld_compx (vbd->cdatacount);
		tvbptr->inodecount = ld_compx (vbd->cnodecount);
		tvbptr->itransnumber = ld_compx (vbd->ctransnumber);
		tvbptr->iuniqueid = ld_compx (vbd->cuniqueid);
		tvbptr->inodeaudit = ld_compx (vbd->cnodeaudit);
		for (k = 0; k < 9; k++)
			tvbptr->ivarleng[k] = ld_compx8 (&vbd->cvarleng[k]);
	} else if (tvbptr->pdictbuf[0] == 0xFE && tvbptr->pdictbuf[1] == 0x53) {
		cid = (struct CIDICTNODE *) tvbptr->pdictbuf;
		tvbptr->iquadsize = 4;
		tvbptr->inodesize = inl_ldint (cid->cnodesize) + 1;
		tvbptr->iformat = C_ISAM_FILE;
		tvbptr->irsvdperkey = cid->crsvdperkey;
		tvbptr->inkeys = inl_ldint (cid->cindexcount);
		tvbptr->iduplen = cid->crfu2[1];
		if (tvbptr->iduplen != 2 && tvbptr->iduplen != 4)
			tvbptr->iduplen = 2;
		tvbptr->iminrowlength = inl_ldint (cid->cminrowlength);
		tvbptr->imaxrowlength = inl_ldint (cid->cmaxrowlength);
		if (tvbptr->imaxrowlength == 0)
			tvbptr->imaxrowlength = tvbptr->iminrowlength;
		tvbptr->inodekeydesc = ld_compx (cid->cnodekeydesc);
		tvbptr->idatafree = ld_compx (cid->cdatafree);
		tvbptr->inodefree = ld_compx (cid->cnodefree);
		tvbptr->idatacount = ld_compx (cid->cdatacount);
		tvbptr->inodecount = ld_compx (cid->cnodecount);
		tvbptr->itransnumber = ld_compx (cid->ctransnumber);
		tvbptr->iuniqueid = ld_compx (cid->cuniqueid);
		tvbptr->inodeaudit = ld_compx (cid->cnodeaudit);
		for (k = 0; k < 9; k++)
			tvbptr->ivarleng[k] = ld_compx4 (cid->cvarleng[k]);
	} else {
		vb_rtd->iserrno = EBADFILE;
		return -1;
	}
	if (tvbptr->imaxrowlength < tvbptr->iminrowlength) {
		tvbptr->imaxrowlength = tvbptr->iminrowlength;
	}
	return 0;
}

/*
 * Move DICTINFO to block and write the block
 */
int
ivbwritedict (const int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	int         iidxhandle, k;
	struct DICTINFO *tvbptr;
	struct VBDICTNODE *vbd;
	struct CIDICTNODE *cid;

	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADFILE;
		return -1;
	}
	tvbptr = vb_rtd->psvbfile[ihandle];
	if (tvbptr == NULL) {
		vb_rtd->iserrno = EBADFILE;
		return -1;
	}
	iidxhandle = tvbptr->iindexhandle;
	if (tvbptr->inodesize < 512)
		tvbptr->inodesize = C_DFLT_NODE_LENGTH;
	if (tvbptr->pdictbuf == NULL) {
		tvbptr->pdictbuf = pvvbmalloc (tvbptr->inodesize);
	}

	if (tvbptr->iformat == V_ISAM_FILE) {
		vbd = (struct VBDICTNODE *) tvbptr->pdictbuf;
		memset ((void *) vbd, 0, tvbptr->inodesize);
		tvbptr->iquadsize = 8;
		vbd->cvalidation[0] = 'V';
		vbd->cvalidation[1] = 'B';
		vbd->cheaderrsvd = 2;
		vbd->cfooterrsvd = 2;
		vbd->crsvdperkey = tvbptr->irsvdperkey;
		vbd->crfu1 = 4;
		inl_stint (tvbptr->inodesize - 1, vbd->cnodesize);
		inl_stint (tvbptr->inkeys, vbd->cindexcount);
		vbd->crfu2[0] = 7;
		vbd->crfu2[1] = tvbptr->iduplen;
		vbd->cfileversion = 0;
		inl_stint (8, vbd->clockmethod);
		inl_stint (tvbptr->iminrowlength, vbd->cminrowlength);
		inl_stint (tvbptr->imaxrowlength, vbd->cmaxrowlength);
		st_compx (tvbptr->inodekeydesc, vbd->cnodekeydesc);
		st_compx (tvbptr->idatafree, vbd->cdatafree);
		st_compx (tvbptr->inodefree, vbd->cnodefree);
		st_compx (tvbptr->idatacount, vbd->cdatacount);
		st_compx (tvbptr->inodecount, vbd->cnodecount);
		st_compx (tvbptr->itransnumber, vbd->ctransnumber);
		st_compx (tvbptr->iuniqueid, vbd->cuniqueid);
		st_compx (tvbptr->inodeaudit, vbd->cnodeaudit);
		for (k = 0; k < 9; k++)
			st_compx8 (tvbptr->ivarleng[k], vbd->cvarleng[k]);
		tvbptr->pdictbuf[tvbptr->inodesize - 3] = 0xFF;
		tvbptr->pdictbuf[tvbptr->inodesize - 2] = 0x7E;
		tvbptr->pdictbuf[tvbptr->inodesize - 1] = 0x0A;
	} else if (tvbptr->iformat == C_ISAM_FILE) {
		cid = (struct CIDICTNODE *) tvbptr->pdictbuf;
		memset ((void *) cid, 0, tvbptr->inodesize);
		tvbptr->iquadsize = 4;
		cid->cvalidation[0] = 0xFE;
		cid->cvalidation[1] = 0x53;
		memcpy (&tvbptr->pdictbuf[tvbptr->inodesize - 4], "DISa", 4);
		/* D-ISAM: At inodesize-6 are some flags: ISMASKED & ISVARCMP */
		cid->cheaderrsvd = 2;
		cid->cfooterrsvd = 2;
		cid->crsvdperkey = tvbptr->irsvdperkey;
		cid->crfu1 = 4;
		inl_stint (tvbptr->inodesize - 1, cid->cnodesize);
		inl_stint (tvbptr->inkeys, cid->cindexcount);
		cid->crfu2[0] = 7;
		cid->crfu2[1] = tvbptr->iduplen;
		cid->crfu4[1] = 8;
		cid->cfileversion = 0;
		inl_stint (tvbptr->iminrowlength, cid->cminrowlength);
		if (tvbptr->iminrowlength == tvbptr->imaxrowlength)
			inl_stint (0, cid->cmaxrowlength);
		else
			inl_stint (tvbptr->imaxrowlength, cid->cmaxrowlength);
		st_compx (tvbptr->inodekeydesc, cid->cnodekeydesc);
		st_compx (tvbptr->idatafree, cid->cdatafree);
		st_compx (tvbptr->inodefree, cid->cnodefree);
		st_compx (tvbptr->idatacount, cid->cdatacount);
		st_compx (tvbptr->inodecount, cid->cnodecount);
		st_compx (tvbptr->itransnumber, cid->ctransnumber);
		st_compx (tvbptr->iuniqueid, cid->cuniqueid);
		st_compx (tvbptr->inodeaudit, cid->cnodeaudit);
		for (k = 0; k < 5; k++)
			st_compx4 (tvbptr->ivarleng[k], cid->cvarleng[k]);
	} else {
		vb_rtd->iserrno = EBADFILE;
		return -1;
	}
	/* read the dictionary block and populate DICTINFO */
	tvblseek (iidxhandle, 0, SEEK_SET);
	errno = 0;
	tvbwrite (iidxhandle, tvbptr->pdictbuf, tvbptr->inodesize);
	if (errno != 0) {
		vb_rtd->iserrno = errno;
		return -1;
	}
	return 0;
}
