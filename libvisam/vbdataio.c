/*
 * Copyright (C) 2003 Trevor van Bremen
 * Copyright (C) 2008-2010 Cobol-IT
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

struct CIVARLEN {
	VB_CHAR     crfu[INTSIZE];	/* Always 0x0000 */
	VB_CHAR     cconst[INTSIZE];	/* Always 0x7e26 */
	VB_CHAR     cfreenext[4];	/* Pointer to next in group with space */
	VB_CHAR     cfreeprev[4];	/* Pointer to prev in group with space */
	VB_CHAR     cfreethis[2];	/* Free space in THIS node */
	VB_CHAR     cfreeoffset[2];	/* Position in node of free space */
	VB_CHAR     cfreecont[4];	/* Continuation node (Only on FULL node) */
	VB_CHAR     cflag;			/* Unknown, set to 0x00 */
	VB_CHAR     cusedcount;		/* Number of slots in use */
	VB_CHAR     cgroup;			/* Used as a reference in dictionary */
};

struct VBVARLEN {
	VB_CHAR     crfu[INTSIZE];	/* Always 0x0000 */
	VB_CHAR     cconst[INTSIZE];	/* Always 0x7e26 */
	VB_CHAR     cfreenext[8];	/* Pointer to next in group with space */
	VB_CHAR     cfreeprev[8];	/* Pointer to prev in group with space */
	VB_CHAR     cfreethis[2];	/* Free space in THIS node */
	VB_CHAR     cfreeoffset[2];	/* Position in node of free space */
	VB_CHAR     cfreecont[8];	/* Continuation node (Only on FULL node) */
	VB_CHAR     cflag;			/* Unknown, set to 0x00 */
	VB_CHAR     cusedcount[2];	/* Number of slots in use */
	VB_CHAR     cgroup;			/* Used as a reference in dictionary */
};

/* For VB-ISAM: cvarleng0 thru cvarleng8  */
static const int vbgroupsize[] = {
	8, 16, 32, 64, 128, 256, 512, 1024, MAX_NODE_LENGTH
};

#define vbgroupcount (sizeof(vbgroupsize) / sizeof(int))

/* For C-ISAM: cvarleng0 thru cvarleng4 */
static const int cigroupsize[] = {
	8, 32, 128, 512, MAX_NODE_LENGTH
};

#define cigroupcount (sizeof(cigroupsize) / sizeof(int))

/*******************************/
/* Local functions for VB-ISAM */
/*******************************/
static int
vbrelocatenode (vb_rtd_t * vb_rtd,
				struct DICTINFO *psvbptr,
				struct VBVARLEN *volatile pshdr,
				off_t tnodenumber, const int ihandle)
{
	int         ifreethis, igroup;
	off_t       tnodenext, tnodeprev;
	VB_CHAR     cnextprev[MAX_NODE_LENGTH];
	struct VBVARLEN *volatile psnphdr;
	psnphdr = (struct VBVARLEN *) cnextprev;

	ifreethis = inl_ldint (pshdr->cfreethis);
	/* Determine which 'group' the node belongs in now */
	for (igroup = 0; igroup < vbgroupcount && vbgroupsize[igroup] < ifreethis; igroup++);	/* Do nothing! */
	igroup--;
	if (igroup != pshdr->cgroup) {
		tnodenext = ld_compx (pshdr->cfreenext);
		tnodeprev = ld_compx (pshdr->cfreeprev);
		if (pshdr->cgroup >= 0) {
			if (psvbptr->ivarleng[pshdr->cgroup] == tnodenumber) {
				psvbptr->ivarleng[pshdr->cgroup] = tnodenext;
			}
		}
		if (tnodeprev) {
			if (ivbblockread (ihandle, 1, tnodeprev, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
			st_compx (tnodenext, psnphdr->cfreenext);
			if (ivbblockwrite (ihandle, 1, tnodeprev, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
		}
		if (tnodenext) {
			if (ivbblockread (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
			st_compx (tnodeprev, psnphdr->cfreeprev);
			if (ivbblockwrite (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
		}
		st_compx ((off_t) 0, pshdr->cfreeprev);
		if (igroup >= 0) {
			tnodenext = psvbptr->ivarleng[igroup];
		} else {
			tnodenext = 0;
		}
		st_compx (tnodenext, pshdr->cfreenext);
		if (tnodenext) {
			if (ivbblockread (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
			st_compx (tnodenumber, psnphdr->cfreeprev);
			if (ivbblockwrite (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
		}
		if (igroup >= 0) {
			psvbptr->ivarleng[igroup] = tnodenumber;
		}
		psvbptr->iisdictlocked |= 0x02;
		pshdr->cgroup = igroup;
		if (ivbblockwrite (ihandle, 1, tnodenumber, (VB_CHAR *) pshdr)) {
			return (-1);
		}
	}
	return tnodenumber;
}

/* Locate space for, and fill in the tail content. */
/* Write out the tail node, and return the tail node number. */
/* Fill in the slot number used in the tail node */

static      off_t
vbtailnode (const int ihandle, VB_CHAR * pcbuffer, const int ilength,
			int *pislotnumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcnodeptr;
	struct VBVARLEN *volatile pshdr;
	off_t       tnodenumber = 0;
	int         ifreethis, ifreeoffset, igroup, islotnumber;
	int         inodesize;
	VB_CHAR     clclnode[MAX_NODE_LENGTH];

	psvbptr = vb_rtd->psvbfile[ihandle];
	pshdr = (struct VBVARLEN *) clclnode;
	inodesize = psvbptr->inodesize;
	/* Determine which group to START with */
	for (igroup = 0; igroup < vbgroupcount
		 && vbgroupsize[igroup] < (ilength + INTSIZE + INTSIZE); igroup++);

	if (igroup >= vbgroupcount) {
		igroup--;
	}
	/*ACO igroup--; */
	while (!tnodenumber) {
		tnodenumber = psvbptr->ivarleng[igroup];
		while (tnodenumber) {
			if (ivbblockread (ihandle, 1, tnodenumber, (VB_CHAR *) clclnode)) {
				return (-1);
			}
			ifreethis = inl_ldint (pshdr->cfreethis);
			/* sanity check */
			if ( /*(igroup > 0) && */ (ifreethis < vbgroupsize[igroup])) {
				pshdr->cgroup = igroup;
				vbrelocatenode (vb_rtd, psvbptr, pshdr, tnodenumber, ihandle);
				tnodenumber = psvbptr->ivarleng[igroup];
			} else {
				/* check space */
				if (ifreethis < (ilength + INTSIZE + INTSIZE)) {
					tnodenumber = ld_compx (pshdr->cfreenext);
					/*Hum ... the should see next group */
					/*tnodenumber = 0; */
				} else {
					break;
				}
			}
		}
		if (tnodenumber) {
			break;
		}
		if (vbgroupsize[igroup] >= MAX_NODE_LENGTH) {
			break;
		}
		igroup++;
	}
	if (!tnodenumber) {
		tnodenumber = tvbnodecountgetnext (ihandle);
		if (tnodenumber == -1) {
			return (tnodenumber);
		}
		memset (clclnode, 0, MAX_NODE_LENGTH);
		inl_stint (0x7e26, pshdr->cconst);
		inl_stint (inodesize -
				   (sizeof (struct VBVARLEN) + 3 + INTSIZE + INTSIZE),
				   pshdr->cfreethis);
		inl_stint (sizeof (struct VBVARLEN), pshdr->cfreeoffset);
		pshdr->cgroup = -1;
		clclnode[inodesize - 3] = 0x7c;
		*pislotnumber = 0;
		inl_stint (1, pshdr->cusedcount);
	} else {
		*pislotnumber = -1;
		pcnodeptr = clclnode + inodesize - (3 + INTSIZE + INTSIZE);
		for (islotnumber = 0; islotnumber < inl_ldint (pshdr->cusedcount);
			 islotnumber++) {
			if (inl_ldint (pcnodeptr)) {
				pcnodeptr -= (INTSIZE * 2);
				continue;
			} else {
				*pislotnumber = islotnumber;
				break;
			}
		}
		if (*pislotnumber == -1) {
			*pislotnumber = inl_ldint (pshdr->cusedcount);
			inl_stint (*(pislotnumber) + 1, pshdr->cusedcount);
			inl_stint (inl_ldint (pshdr->cfreethis) - (INTSIZE * 2),
					   pshdr->cfreethis);
		}
	}
	ifreethis = inl_ldint (pshdr->cfreethis);
	ifreeoffset = inl_ldint (pshdr->cfreeoffset);
	pcnodeptr =
		clclnode + inodesize - (3 + INTSIZE + INTSIZE +
								(*pislotnumber * INTSIZE * 2));
	inl_stint (ilength, pcnodeptr);
	inl_stint (ifreeoffset, pcnodeptr + INTSIZE);
	memcpy (((VB_CHAR *) pshdr) + ifreeoffset, pcbuffer, (size_t) ilength);
	ifreethis -= ilength;
	inl_stint (ifreethis, pshdr->cfreethis);
	ifreeoffset += ilength;
	inl_stint (ifreeoffset, pshdr->cfreeoffset);

	if (ivbblockwrite (ihandle, 1, tnodenumber, (VB_CHAR *) clclnode)) {
		return (-1);
	}
	return vbrelocatenode (vb_rtd, psvbptr, pshdr, tnodenumber, ihandle);
}

static int
ivbvarlenread (const int ihandle, VB_CHAR * pcbuffer, off_t tnodenumber,
			   int islotnumber, int ilength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	VB_CHAR    *pcnodeptr;
	int         iresult, islotlength, islotoffset;
	int         inodesize;
	VB_CHAR     cnode[MAX_NODE_LENGTH];
	struct VBVARLEN *volatile psvarlenheader = (struct VBVARLEN *) cnode;

	inodesize = vb_rtd->psvbfile[ihandle]->inodesize;
	while (1) {
		iresult = ivbblockread (ihandle, 1, tnodenumber, cnode);
		if (iresult) {
			return (-1);
		}
		if (inl_ldint (psvarlenheader->cconst) != 0x7e26) {
			vb_rtd->iserrno = EBADFILE;
			return (-1);
		}
		pcnodeptr = cnode + inodesize - 3;
		if (*pcnodeptr != 0x7c) {
			vb_rtd->iserrno = EBADFILE;
			return (-1);
		}
		pcnodeptr -= ((islotnumber + 1) * INTSIZE * 2);
		islotlength = inl_ldint (pcnodeptr);
		islotoffset = inl_ldint (pcnodeptr + INTSIZE);
		/*COBOL-IT if (islotlength <= ilength) { */
		if (islotlength >= ilength) {
			if (ld_compx (psvarlenheader->cfreecont) != 0) {
				vb_rtd->iserrno = EBADFILE;
				return (-1);
			}
			memcpy (pcbuffer, cnode + islotoffset, (size_t) ilength);
			return (0);
		}
		if (ld_compx (psvarlenheader->cfreecont) == 0) {
			vb_rtd->iserrno = EBADFILE;
			return (-1);
		}

		memcpy (pcbuffer, cnode + islotoffset, (size_t) islotlength);
		pcbuffer += islotlength;
		ilength -= islotlength;
		islotnumber = inl_ldint (psvarlenheader->cfreecont) >> 6;
		*(psvarlenheader->cfreecont + 1) &= 0x3f;
		*(psvarlenheader->cfreecont) = 0;
		tnodenumber = ld_compx (psvarlenheader->cfreecont);
	}
	return (0);
}

/* MUST populate vb_rtd->psvbfile [ihandle]->tvarlennode */
/* MUST populate vb_rtd->psvbfile [ihandle]->ivarlenslot */
/* MUST populate vb_rtd->psvbfile [ihandle]->ivarlenlength */
static int
ivbvarlenwrite (const int ihandle, VB_CHAR * pcbuffer, int ilength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tnewnode, tnodenumber = 0;
	int         islotnumber;
	int         inodesize;
	VB_CHAR     cnode[MAX_NODE_LENGTH];
	struct VBVARLEN *volatile psvarlenheader = (struct VBVARLEN *) cnode;

	psvbptr = vb_rtd->psvbfile[ihandle];
	psvbptr->ivarlenlength = ilength;
	psvbptr->tvarlennode = 0;
	inodesize = psvbptr->inodesize;
	/* Write out 'FULL' nodes first */
	while (ilength > 0) {
		if (ilength >
			(inodesize - (int) (3 + INTSIZE + INTSIZE + sizeof (struct VBVARLEN)))) {
			tnewnode = tvbnodecountgetnext (ihandle);
			if (tnewnode == -1) {
				return (-1);
			}
			if (tnodenumber) {
				st_compx (tnewnode, psvarlenheader->cfreecont);
				if (ivbblockwrite (ihandle, 1, tnodenumber, cnode)) {
					return (-1);
				}
			} else {
				psvbptr->tvarlennode = tnewnode;
				psvbptr->ivarlenslot = 0;
			}
			inl_stint (0, psvarlenheader->crfu);
			inl_stint (0x7e26, psvarlenheader->cconst);
			st_compx ((off_t) 0, psvarlenheader->cfreenext);
			st_compx ((off_t) 0, psvarlenheader->cfreeprev);
			inl_stint (0, psvarlenheader->cfreethis);
			inl_stint (inodesize - (3 + INTSIZE + INTSIZE),
					   psvarlenheader->cfreeoffset);
			psvarlenheader->cflag = 0x01;
			inl_stint (1, psvarlenheader->cusedcount);
			psvarlenheader->cgroup = 0x00;
			memcpy (&(psvarlenheader->cgroup) + 1, pcbuffer,
					inodesize - (3 + INTSIZE + INTSIZE + sizeof (struct VBVARLEN)));
			pcbuffer +=
				inodesize - (3 + INTSIZE + INTSIZE + sizeof (struct VBVARLEN));
			ilength -=
				inodesize - (3 + INTSIZE + INTSIZE + sizeof (struct VBVARLEN));
			/* Length */
			inl_stint (inodesize -
					   (3 + INTSIZE + INTSIZE + sizeof (struct VBVARLEN)),
					   cnode + inodesize - (3 + INTSIZE + INTSIZE));
			/* Offset */
			inl_stint (&(psvarlenheader->cgroup) + 1 - cnode,
					   cnode + inodesize - (3 + INTSIZE));
			*(cnode + inodesize - 3) = 0x7c;
			*(cnode + inodesize - 2) = 0x0;
			*(cnode + inodesize - 1) = 0x0;
			tnodenumber = tnewnode;
			continue;
		}
		if (!psvbptr->tvarlennode) {
			psvbptr->tvarlennode = tnodenumber;
			psvbptr->ivarlenslot = 0;
		}
		/* If tnodenumber is != 0, we still need to write it out! */
		if (tnodenumber && !ilength) {
			return (ivbblockwrite (ihandle, 1, tnodenumber, cnode));
		}
		/* Now, to deal with the 'tail' */
		tnewnode = vbtailnode (ihandle, pcbuffer, ilength, &islotnumber);
		if (tnewnode == -1) {
			return (-1);
		}
		if (tnodenumber) {
			st_compx (tnewnode, psvarlenheader->cfreecont);
			inl_stint ((islotnumber << 6) + inl_ldint (psvarlenheader->cfreecont),
					   psvarlenheader->cfreecont);
			if (ivbblockwrite (ihandle, 1, tnodenumber, cnode)) {
				return (-1);
			}
			if (!psvbptr->tvarlennode) {
				psvbptr->tvarlennode = tnodenumber;
				psvbptr->ivarlenslot = 0;
			}
		}
		if (!psvbptr->tvarlennode) {
			psvbptr->tvarlennode = tnewnode;
			psvbptr->ivarlenslot = islotnumber;
		}
		return (0);
	}
	return (-1);
}

/* MUST update the group number (if applicable) */
/* MUST update the dictionary node (if applicable) */
static int
ivbvarlendelete (const int ihandle, off_t tnodenumber, int islotnumber, int ilength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tnodenext, tnodeprev;
	int         ifreethis, ifreeoffset, igroup,
		iisanyused, iloop, imovelength,
		ioffset, ithislength, ithisoffset, iusedcount;
	int         inodesize;
	VB_CHAR     cnode[MAX_NODE_LENGTH];
	struct VBVARLEN *volatile psvarlenheader = (struct VBVARLEN *) cnode;

	psvbptr = vb_rtd->psvbfile[ihandle];
	inodesize = psvbptr->inodesize;
	while (ilength > 0) {
		if (ivbblockread (ihandle, 1, tnodenumber, ((VB_CHAR *) cnode))) {
			return (-1);
		}
		ithislength = inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
								 (3 + INTSIZE + INTSIZE +
								  (islotnumber * 2 * INTSIZE)));
		ilength -= ithislength;
		inl_stint (0, ((VB_CHAR *) psvarlenheader) + inodesize -
				   (3 + INTSIZE + INTSIZE + (islotnumber * 2 * INTSIZE)));
		ithisoffset = inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
								 (3 + INTSIZE + (islotnumber * 2 * INTSIZE)));
		inl_stint (0, ((VB_CHAR *) psvarlenheader) + inodesize -
				   (3 + INTSIZE + (islotnumber * 2 * INTSIZE)));
		iusedcount = inl_ldint (psvarlenheader->cusedcount);
		iisanyused = 0;
		for (iloop = 0; iloop < iusedcount; iloop++) {
			if (inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
						   (3 + INTSIZE + INTSIZE + (iloop * 2 * INTSIZE)))) {
				iisanyused = 1;
				break;
			}
		}
		if (!iisanyused) {
			tnodenext = ld_compx (psvarlenheader->cfreenext);
			tnodeprev = ld_compx (psvarlenheader->cfreeprev);
			igroup = psvarlenheader->cgroup;
			if (igroup >= 0 && psvbptr->ivarleng[igroup] == tnodenumber) {
				psvbptr->ivarleng[igroup] = tnodenext;
			}
			ivbnodefree (ihandle, tnodenumber);
			tnodenumber = ld_compx (psvarlenheader->cfreecont);
			if (tnodenext) {
				if (ivbblockread
					(ihandle, 1, tnodenext, ((VB_CHAR *) psvarlenheader))) {
					return (-1);
				}
				st_compx (tnodeprev, psvarlenheader->cfreeprev);
				if (ivbblockwrite (ihandle, 1, tnodenext, ((VB_CHAR *) cnode))) {
					return (-1);
				}
			}
			if (tnodeprev) {
				if (ivbblockread (ihandle, 1, tnodeprev, ((VB_CHAR *) cnode))) {
					return (-1);
				}
				st_compx (tnodenext, psvarlenheader->cfreenext);
				if (ivbblockwrite (ihandle, 1, tnodeprev, ((VB_CHAR *) cnode))) {
					return (-1);
				}
			}
			continue;
		}
		ifreethis = inl_ldint (psvarlenheader->cfreethis);
		ifreeoffset = inl_ldint (psvarlenheader->cfreeoffset);
		if (islotnumber == iusedcount) {
			iusedcount--;
			inl_stint (iusedcount - 1, psvarlenheader->cusedcount);
			ifreethis += (INTSIZE * 2);
		}
		imovelength = inodesize - (ithisoffset + ithislength);
		imovelength -= (3 + (iusedcount * INTSIZE * 2));
		memmove (((VB_CHAR *) psvarlenheader) + ithisoffset,
				 ((VB_CHAR *) psvarlenheader) + ithisoffset + ithislength,
				 (size_t) imovelength);
		ifreeoffset -= ithislength;
		ifreethis += ithislength;
		inl_stint (ifreethis, psvarlenheader->cfreethis);
		inl_stint (ifreeoffset, psvarlenheader->cfreeoffset);
		memset (((VB_CHAR *) psvarlenheader) + ifreeoffset, 0, (size_t) ifreethis);
		for (iloop = 0; iloop < iusedcount; iloop++) {
			ioffset =
				inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
						   (3 + INTSIZE + (iloop * 2 * INTSIZE)));
			if (ioffset > ithisoffset) {
				inl_stint (ioffset - ithislength,
						   ((VB_CHAR *) psvarlenheader) + inodesize -
						   (3 + INTSIZE + (iloop * 2 * INTSIZE)));
			}
		}
		if (ivbblockwrite (ihandle, 1, tnodenumber, ((VB_CHAR *) cnode))) {
			return (-1);
		}
		vbrelocatenode (vb_rtd, psvbptr, psvarlenheader, tnodenumber, ihandle);

		tnodenumber = ld_compx (psvarlenheader->cfreecont);
	}
	psvbptr->tvarlennode = 0;
	return (0);
}

/******************************/
/* Local functions for C-ISAM */
/******************************/
static int
cirelocatenode (vb_rtd_t * vb_rtd,
				struct DICTINFO *psvbptr,
				struct CIVARLEN *volatile pshdr,
				off_t tnodenumber, const int ihandle)
{
	int         ifreethis, igroup;
	off_t       tnodenext, tnodeprev;
	VB_CHAR     cnextprev[MAX_NODE_LENGTH];
	struct CIVARLEN *volatile psnphdr;
	psnphdr = (struct CIVARLEN *) cnextprev;

	ifreethis = inl_ldint (pshdr->cfreethis);
	/* Determine which 'group' the node belongs in now */
	for (igroup = 0; igroup < cigroupcount
		 && cigroupsize[igroup] < ifreethis; igroup++);
	igroup--;
	if (igroup != pshdr->cgroup) {
		tnodenext = ld_compx (pshdr->cfreenext);
		tnodeprev = ld_compx (pshdr->cfreeprev);
		if (pshdr->cgroup >= 0) {
			if (psvbptr->ivarleng[pshdr->cgroup] == tnodenumber) {
				psvbptr->ivarleng[pshdr->cgroup] = tnodenext;
			}
		}
		if (tnodeprev) {
			if (ivbblockread (ihandle, 1, tnodeprev, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
			st_compx (tnodenext, psnphdr->cfreenext);
			if (ivbblockwrite (ihandle, 1, tnodeprev, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
		}
		if (tnodenext) {
			if (ivbblockread (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
			st_compx (tnodeprev, psnphdr->cfreeprev);
			if (ivbblockwrite (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
		}
		st_compx ((off_t) 0, pshdr->cfreeprev);
		if (igroup >= 0) {
			tnodenext = psvbptr->ivarleng[igroup];
		} else {
			tnodenext = 0;
		}
		st_compx (tnodenext, pshdr->cfreenext);
		if (tnodenext) {
			if (ivbblockread (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
			st_compx (tnodenumber, psnphdr->cfreeprev);
			if (ivbblockwrite (ihandle, 1, tnodenext, ((VB_CHAR *) cnextprev))) {
				return (-1);
			}
		}
		if (igroup >= 0) {
			psvbptr->ivarleng[igroup] = tnodenumber;
		}
		psvbptr->iisdictlocked |= 0x02;
		pshdr->cgroup = igroup;
		if (ivbblockwrite (ihandle, 1, tnodenumber, (VB_CHAR *) pshdr)) {
			return (-1);
		}
	}
	return tnodenumber;
}

/* Locate space for, and fill in the tail content. */
/* Write out the tail node, and return the tail node number. */
/* Fill in the slot number used in the tail node */

static      off_t
citailnode (const int ihandle, VB_CHAR * pcbuffer, const int ilength,
			int *pislotnumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	VB_CHAR    *pcnodeptr;
	struct CIVARLEN *volatile pshdr;
	off_t       tnodenumber = 0;
	int         ifreethis, ifreeoffset, igroup, islotnumber;
	int         inodesize;
	VB_CHAR     clclnode[MAX_NODE_LENGTH];

	psvbptr = vb_rtd->psvbfile[ihandle];
	pshdr = (struct CIVARLEN *) clclnode;
	inodesize = psvbptr->inodesize;
	/* Determine which group to START with */
	for (igroup = 0; igroup < cigroupcount && cigroupsize[igroup] < ilength + INTSIZE + INTSIZE; igroup++);	/* Do nothing! */


	if (igroup >= cigroupcount) {
		igroup--;
	}
	/*ACO igroup--; */
	while (!tnodenumber) {
		tnodenumber = psvbptr->ivarleng[igroup];
		while (tnodenumber) {
			if (ivbblockread (ihandle, 1, tnodenumber, (VB_CHAR *) clclnode)) {
				return (-1);
			}
			ifreethis = inl_ldint (pshdr->cfreethis);
			/* sanity check */
			if ( /*(igroup > 0) && */ (ifreethis < cigroupsize[igroup])) {
				pshdr->cgroup = igroup;
				cirelocatenode (vb_rtd, psvbptr, pshdr, tnodenumber, ihandle);
				tnodenumber = psvbptr->ivarleng[igroup];
			} else {
				/* check space */
				if (ifreethis < (ilength + INTSIZE + INTSIZE)) {
					tnodenumber = ld_compx (pshdr->cfreenext);
					/*Hum ... the should see next group */
					/*tnodenumber = 0; */
				} else {
					break;
				}
			}
		}
		if (tnodenumber) {
			break;
		}
		if (cigroupsize[igroup] >= MAX_NODE_LENGTH) {
			break;
		}
		igroup++;
	}
	if (!tnodenumber) {
		tnodenumber = tvbnodecountgetnext (ihandle);
		if (tnodenumber == -1) {
			return (tnodenumber);
		}
		memset (clclnode, 0, MAX_NODE_LENGTH);
		inl_stint (0x7e26, pshdr->cconst);
		inl_stint (inodesize -
				   (sizeof (struct CIVARLEN) + 3 + INTSIZE + INTSIZE),
				   pshdr->cfreethis);
		inl_stint (sizeof (struct CIVARLEN), pshdr->cfreeoffset);
		pshdr->cgroup = -1;
		clclnode[inodesize - 3] = 0x7c;
		*pislotnumber = 0;
		pshdr->cusedcount = 1;
	} else {
		*pislotnumber = -1;
		pcnodeptr = clclnode + inodesize - (3 + INTSIZE + INTSIZE);
		for (islotnumber = 0; islotnumber < pshdr->cusedcount; islotnumber++) {
			if (inl_ldint (pcnodeptr)) {
				pcnodeptr -= (INTSIZE * 2);
				continue;
			} else {
				*pislotnumber = islotnumber;
				break;
			}
		}
		if (*pislotnumber == -1) {
			*pislotnumber = pshdr->cusedcount;
			pshdr->cusedcount++;
			inl_stint (inl_ldint (pshdr->cfreethis) - (INTSIZE * 2),
					   pshdr->cfreethis);
		}
	}
	ifreethis = inl_ldint (pshdr->cfreethis);
	ifreeoffset = inl_ldint (pshdr->cfreeoffset);
	pcnodeptr =
		clclnode + inodesize - (3 + INTSIZE + INTSIZE +
								(*pislotnumber * INTSIZE * 2));
	inl_stint (ilength, pcnodeptr);
	inl_stint (ifreeoffset, pcnodeptr + INTSIZE);
	memcpy (((VB_CHAR *) pshdr) + ifreeoffset, pcbuffer, (size_t) ilength);
	ifreethis -= ilength;
	inl_stint (ifreethis, pshdr->cfreethis);
	ifreeoffset += ilength;
	inl_stint (ifreeoffset, pshdr->cfreeoffset);

	if (ivbblockwrite (ihandle, 1, tnodenumber, (VB_CHAR *) clclnode)) {
		return (-1);
	}
	return cirelocatenode (vb_rtd, psvbptr, pshdr, tnodenumber, ihandle);
}

static int
icivarlenread (const int ihandle, VB_CHAR * pcbuffer, off_t tnodenumber,
			   int islotnumber, int ilength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	VB_CHAR    *pcnodeptr;
	int         iresult, islotlength, islotoffset;
	int         inodesize;
	VB_CHAR     cnode[MAX_NODE_LENGTH];
	struct CIVARLEN *volatile psvarlenheader = (struct CIVARLEN *) cnode;

	inodesize = vb_rtd->psvbfile[ihandle]->inodesize;
	while (1) {
		iresult = ivbblockread (ihandle, 1, tnodenumber, cnode);
		if (iresult) {
			return (-1);
		}
		if (inl_ldint (psvarlenheader->cconst) != 0x7e26) {
			vb_rtd->iserrno = EBADFILE;
			return (-1);
		}
		pcnodeptr = cnode + inodesize - 3;
		if (*pcnodeptr != 0x7c) {
			vb_rtd->iserrno = EBADFILE;
			return (-1);
		}
		pcnodeptr -= ((islotnumber + 1) * INTSIZE * 2);
		islotlength = inl_ldint (pcnodeptr);
		islotoffset = inl_ldint (pcnodeptr + INTSIZE);
		/*COBOL-IT if (islotlength <= ilength) { */
		if (islotlength >= ilength) {
			if (ld_compx (psvarlenheader->cfreecont) != 0) {
				vb_rtd->iserrno = EBADFILE;
				return (-1);
			}
			memcpy (pcbuffer, cnode + islotoffset, (size_t) ilength);
			return (0);
		}
		if (ld_compx (psvarlenheader->cfreecont) == 0) {
			vb_rtd->iserrno = EBADFILE;
			return (-1);
		}

		memcpy (pcbuffer, cnode + islotoffset, (size_t) islotlength);
		pcbuffer += islotlength;
		ilength -= islotlength;
		islotnumber = *(psvarlenheader->cfreecont);
		*(psvarlenheader->cfreecont) = 0;
		tnodenumber = ld_compx (psvarlenheader->cfreecont);
	}
	return (0);
}

/* MUST populate vb_rtd->psvbfile [ihandle]->tvarlennode */
/* MUST populate vb_rtd->psvbfile [ihandle]->ivarlenslot */
/* MUST populate vb_rtd->psvbfile [ihandle]->ivarlenlength */
static int
icivarlenwrite (const int ihandle, VB_CHAR * pcbuffer, int ilength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tnewnode, tnodenumber = 0;
	int         islotnumber;
	int         inodesize;
	VB_CHAR     cnode[MAX_NODE_LENGTH];
	struct CIVARLEN *volatile psvarlenheader = (struct CIVARLEN *) cnode;

	psvbptr = vb_rtd->psvbfile[ihandle];
	psvbptr->ivarlenlength = ilength;
	psvbptr->tvarlennode = 0;
	inodesize = psvbptr->inodesize;
	/* Write out 'FULL' nodes first */
	while (ilength > 0) {
		if (ilength >
			(inodesize - (int) (3 + INTSIZE + INTSIZE + sizeof (struct CIVARLEN)))) {
			tnewnode = tvbnodecountgetnext (ihandle);
			if (tnewnode == -1) {
				return (-1);
			}
			if (tnodenumber) {
				st_compx (tnewnode, psvarlenheader->cfreecont);
				if (ivbblockwrite (ihandle, 1, tnodenumber, cnode)) {
					return (-1);
				}
			} else {
				psvbptr->tvarlennode = tnewnode;
				psvbptr->ivarlenslot = 0;
			}
			inl_stint (0, psvarlenheader->crfu);
			inl_stint (0x7e26, psvarlenheader->cconst);
			st_compx ((off_t) 0, psvarlenheader->cfreenext);
			st_compx ((off_t) 0, psvarlenheader->cfreeprev);
			inl_stint (0, psvarlenheader->cfreethis);
			inl_stint (inodesize - (3 + INTSIZE + INTSIZE),
					   psvarlenheader->cfreeoffset);
			psvarlenheader->cflag = 0x01;
			psvarlenheader->cusedcount = 1;
			psvarlenheader->cgroup = 0x00;
			memcpy (&(psvarlenheader->cgroup) + 1, pcbuffer,
					inodesize - (3 + INTSIZE + INTSIZE + sizeof (struct CIVARLEN)));
			pcbuffer +=
				inodesize - (3 + INTSIZE + INTSIZE + sizeof (struct CIVARLEN));
			ilength -=
				inodesize - (3 + INTSIZE + INTSIZE + sizeof (struct CIVARLEN));
			/* Length */
			inl_stint (inodesize -
					   (3 + INTSIZE + INTSIZE + sizeof (struct CIVARLEN)),
					   cnode + inodesize - (3 + INTSIZE + INTSIZE));
			/* Offset */
			inl_stint (&(psvarlenheader->cgroup) + 1 - cnode,
					   cnode + inodesize - (3 + INTSIZE));
			*(cnode + inodesize - 3) = 0x7c;
			*(cnode + inodesize - 2) = 0x0;
			*(cnode + inodesize - 1) = 0x0;
			tnodenumber = tnewnode;
			continue;
		}
		if (!psvbptr->tvarlennode) {
			psvbptr->tvarlennode = tnodenumber;
			psvbptr->ivarlenslot = 0;
		}
		/* If tnodenumber is != 0, we still need to write it out! */
		if (tnodenumber && !ilength) {
			return (ivbblockwrite (ihandle, 1, tnodenumber, cnode));
		}
		/* Now, to deal with the 'tail' */
		tnewnode = citailnode (ihandle, pcbuffer, ilength, &islotnumber);
		if (tnewnode == -1) {
			return (-1);
		}
		if (tnodenumber) {
			st_compx (tnewnode, psvarlenheader->cfreecont);
			*psvarlenheader->cfreecont = islotnumber;
			if (ivbblockwrite (ihandle, 1, tnodenumber, cnode)) {
				return (-1);
			}
			if (!psvbptr->tvarlennode) {
				psvbptr->tvarlennode = tnodenumber;
				psvbptr->ivarlenslot = 0;
			}
		}
		if (!psvbptr->tvarlennode) {
			psvbptr->tvarlennode = tnewnode;
			psvbptr->ivarlenslot = islotnumber;
		}
		return (0);
	}
	return (-1);
}

/* MUST update the group number (if applicable) */
/* MUST update the dictionary node (if applicable) */
static int
icivarlendelete (const int ihandle, off_t tnodenumber, int islotnumber, int ilength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tnodenext, tnodeprev;
	int         ifreethis, ifreeoffset, igroup,
		iisanyused, iloop, imovelength,
		ioffset, ithislength, ithisoffset, iusedcount;
	int         inodesize;
	VB_CHAR     cnode[MAX_NODE_LENGTH];
	struct CIVARLEN *volatile psvarlenheader = (struct CIVARLEN *) cnode;

	psvbptr = vb_rtd->psvbfile[ihandle];
	inodesize = psvbptr->inodesize;
	while (ilength > 0) {
		if (ivbblockread (ihandle, 1, tnodenumber, ((VB_CHAR *) cnode))) {
			return (-1);
		}
		ithislength = inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
								 (3 + INTSIZE + INTSIZE +
								  (islotnumber * 2 * INTSIZE)));
		ilength -= ithislength;
		inl_stint (0, ((VB_CHAR *) psvarlenheader) + inodesize -
				   (3 + INTSIZE + INTSIZE + (islotnumber * 2 * INTSIZE)));
		ithisoffset = inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
								 (3 + INTSIZE + (islotnumber * 2 * INTSIZE)));
		inl_stint (0, ((VB_CHAR *) psvarlenheader) + inodesize -
				   (3 + INTSIZE + (islotnumber * 2 * INTSIZE)));
		iusedcount = psvarlenheader->cusedcount;
		iisanyused = 0;
		for (iloop = 0; iloop < iusedcount; iloop++) {
			if (inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
						   (3 + INTSIZE + INTSIZE + (iloop * 2 * INTSIZE)))) {
				iisanyused = 1;
				break;
			}
		}
		if (!iisanyused) {
			tnodenext = ld_compx (psvarlenheader->cfreenext);
			tnodeprev = ld_compx (psvarlenheader->cfreeprev);
			igroup = psvarlenheader->cgroup;
			if (igroup >= 0 && psvbptr->ivarleng[igroup] == tnodenumber) {
				psvbptr->ivarleng[igroup] = tnodenext;
			}
			ivbnodefree (ihandle, tnodenumber);
			tnodenumber = ld_compx (psvarlenheader->cfreecont);
			if (tnodenext) {
				if (ivbblockread
					(ihandle, 1, tnodenext, ((VB_CHAR *) psvarlenheader))) {
					return (-1);
				}
				st_compx (tnodeprev, psvarlenheader->cfreeprev);
				if (ivbblockwrite (ihandle, 1, tnodenext, ((VB_CHAR *) cnode))) {
					return (-1);
				}
			}
			if (tnodeprev) {
				if (ivbblockread (ihandle, 1, tnodeprev, ((VB_CHAR *) cnode))) {
					return (-1);
				}
				st_compx (tnodenext, psvarlenheader->cfreenext);
				if (ivbblockwrite (ihandle, 1, tnodeprev, ((VB_CHAR *) cnode))) {
					return (-1);
				}
			}
			continue;
		}
		ifreethis = inl_ldint (psvarlenheader->cfreethis);
		ifreeoffset = inl_ldint (psvarlenheader->cfreeoffset);
		if (islotnumber == iusedcount) {
			iusedcount--;
			psvarlenheader->cusedcount = iusedcount;
			ifreethis += (INTSIZE * 2);
		}
		imovelength = inodesize - (ithisoffset + ithislength);
		imovelength -= (3 + (iusedcount * INTSIZE * 2));
		memmove (((VB_CHAR *) psvarlenheader) + ithisoffset,
				 ((VB_CHAR *) psvarlenheader) + ithisoffset + ithislength,
				 (size_t) imovelength);
		ifreeoffset -= ithislength;
		ifreethis += ithislength;
		inl_stint (ifreethis, psvarlenheader->cfreethis);
		inl_stint (ifreeoffset, psvarlenheader->cfreeoffset);
		memset (((VB_CHAR *) psvarlenheader) + ifreeoffset, 0, (size_t) ifreethis);
		for (iloop = 0; iloop < iusedcount; iloop++) {
			ioffset =
				inl_ldint (((VB_CHAR *) psvarlenheader) + inodesize -
						   (3 + INTSIZE + (iloop * 2 * INTSIZE)));
			if (ioffset > ithisoffset) {
				inl_stint (ioffset - ithislength,
						   ((VB_CHAR *) psvarlenheader) + inodesize -
						   (3 + INTSIZE + (iloop * 2 * INTSIZE)));
			}
		}
		if (ivbblockwrite (ihandle, 1, tnodenumber, ((VB_CHAR *) cnode))) {
			return (-1);
		}
		cirelocatenode (vb_rtd, psvbptr, psvarlenheader, tnodenumber, ihandle);

		tnodenumber = ld_compx (psvarlenheader->cfreecont);
	}
	psvbptr->tvarlennode = 0;
	return (0);
}

/* Global functions */

/* Comments:
*      This function is *NOT* concerned with whether the row is deleted or not
*      However, it *DOES* set *(pideletedrow) accordingly.
*      The receiving buffer (pcbuffer) is only guaranteed to be long enough to
*      hold the MINIMUM row length (exclusive of the 1 byte deleted flag) and
*      thus we need to jump through hoops to avoid overwriting stuff beyond it.
*/
int
ivbdataread (const int ihandle, VB_CHAR * pcbuffer, int *pideletedrow,
			 off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       tblocknumber;
	off_t       toffset;
	off_t       tsofar = 0;
	int         irowlength;
	VB_CHAR     cfooter[16];
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];

	/* Sanity check - Is ihandle a currently open table? */
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		return (ENOTOPEN);
	}
	if (!vb_rtd->psvbfile[ihandle]) {
		return (ENOTOPEN);
	}
	tvbptr = vb_rtd->psvbfile[ihandle];

	irowlength = tvbptr->iminrowlength;
	irowlength++;
	if (tvbptr->iopenmode & ISVARLEN) {
		irowlength += INTSIZE + tvbptr->iquadsize;
	}
	toffset = irowlength * (trownumber - 1);
	tblocknumber = (toffset / tvbptr->inodesize);
	toffset -= (tblocknumber * tvbptr->inodesize);
	if (ivbblockread (ihandle, 0, tblocknumber + 1, cvbnodetmp)) {
		return (EBADFILE);
	}
	/* Read in the *MINIMUM* rowlength and store it into pcbuffer */
	while (tsofar < tvbptr->iminrowlength) {
		if ((tvbptr->iminrowlength - tsofar) < (tvbptr->inodesize - toffset)) {
			memcpy (pcbuffer + tsofar, cvbnodetmp + toffset,
					(size_t) (tvbptr->iminrowlength - tsofar));
			toffset += tvbptr->iminrowlength - tsofar;
			tsofar = tvbptr->iminrowlength;
			break;
		}
		memcpy (pcbuffer + tsofar, cvbnodetmp + toffset,
				(size_t) (tvbptr->inodesize - toffset));
		tblocknumber++;
		tsofar += tvbptr->inodesize - toffset;
		toffset = 0;
		if (ivbblockread (ihandle, 0, tblocknumber + 1, cvbnodetmp)) {
			return (EBADFILE);
		}
	}
	pcbuffer += tsofar;
	/* OK, now for the footer.  Either 1 byte or 1 + INTSIZE + tvbptr->iquadsize. */
	while (tsofar < irowlength) {
		if ((irowlength - tsofar) <= (tvbptr->inodesize - toffset)) {
			memcpy (cfooter + tsofar - tvbptr->iminrowlength, cvbnodetmp + toffset,
					(size_t) (irowlength - tsofar));
			break;
		}
		memcpy (cfooter + tsofar - tvbptr->iminrowlength, cvbnodetmp + toffset,
				(size_t) (tvbptr->inodesize - toffset));
		tblocknumber++;
		tsofar += tvbptr->inodesize - toffset;
		toffset = 0;
		if (ivbblockread (ihandle, 0, tblocknumber + 1, cvbnodetmp)) {
			return (EBADFILE);
		}
	}
	vb_rtd->isreclen = tvbptr->iminrowlength;
	*pideletedrow = 0;
	if (cfooter[0] == 0x00) {
		*pideletedrow = 1;
	} else {
		if (tvbptr->iopenmode & ISVARLEN) {
			tvbptr->ivarlenlength = inl_ldint (cfooter + 1);
/* VBISAM in 64 bit mode (4k Nodes) uses a ten bit slot number */
/* VBISAM in 32 bit mode (1k Nodes) uses an eight bit slot number */
			if (tvbptr->iformat == V_ISAM_FILE) {
				tvbptr->ivarlenslot = inl_ldint (cfooter + 1 + INTSIZE) >> 6;
				*(cfooter + 1 + INTSIZE + 1) &= 0x3f;
			} else {
				tvbptr->ivarlenslot = *(cfooter + 1 + INTSIZE);
			}
			*(cfooter + 1 + INTSIZE) = 0;
			tvbptr->tvarlennode =
				inl_ldcompx (cfooter + 1 + INTSIZE, tvbptr->iquadsize);
			if (tvbptr->ivarlenlength) {
				if (tvbptr->iformat == V_ISAM_FILE) {
					if (ivbvarlenread (ihandle, (VB_CHAR *) pcbuffer,
									   tvbptr->tvarlennode,
									   tvbptr->ivarlenslot, tvbptr->ivarlenlength)) {
						return (vb_rtd->iserrno);
					}
				} else {
					if (icivarlenread (ihandle, (VB_CHAR *) pcbuffer,
									   tvbptr->tvarlennode,
									   tvbptr->ivarlenslot, tvbptr->ivarlenlength)) {
						return (vb_rtd->iserrno);
					}
				}
			}
			vb_rtd->isreclen += tvbptr->ivarlenlength;
		}
	}
	return (0);
}

int
ivbdatawrite (const int ihandle, VB_CHAR * pcbuffer, int ideletedrow,
			  off_t trownumber)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	VB_CHAR    *pctemp;
	off_t       tblocknumber, toffset, tsofar = 0;
	int         irowlength;
	VB_CHAR     cvbnodetmp[MAX_NODE_LENGTH];
	VB_CHAR     pcwritebuffer[MAX_RESERVED_LENGTH];

	/* Sanity check - Is ihandle a currently open table? */
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		return (ENOTOPEN);
	}
	if (!vb_rtd->psvbfile[ihandle]) {
		return (ENOTOPEN);
	}

	tvbptr = vb_rtd->psvbfile[ihandle];
	irowlength = tvbptr->iminrowlength;
	toffset = irowlength + 1;
	if (tvbptr->iopenmode & ISVARLEN) {
		toffset += INTSIZE + tvbptr->iquadsize;
	}
	toffset *= (trownumber - 1);
	if (tvbptr->iopenmode & ISVARLEN) {
		if (tvbptr->tvarlennode) {
			if (tvbptr->iformat == C_ISAM_FILE) {
				if (icivarlendelete
					(ihandle, tvbptr->tvarlennode, tvbptr->ivarlenslot,
					 tvbptr->ivarlenlength)) {
					return (-69);
				}
			} else
				if (ivbvarlendelete
					(ihandle, tvbptr->tvarlennode, tvbptr->ivarlenslot,
					 tvbptr->ivarlenlength)) {
				return (-69);
			}
		}
		if (vb_rtd->isreclen == tvbptr->iminrowlength || ideletedrow) {
			tvbptr->tvarlennode = 0;
			tvbptr->ivarlenlength = 0;
			tvbptr->ivarlenslot = 0;
		} else if (tvbptr->iformat == C_ISAM_FILE) {
			if (icivarlenwrite (ihandle, pcbuffer + tvbptr->iminrowlength,
								vb_rtd->isreclen - tvbptr->iminrowlength)) {
				return (vb_rtd->iserrno);
			}
		} else {
			if (ivbvarlenwrite (ihandle, pcbuffer + tvbptr->iminrowlength,
								vb_rtd->isreclen - tvbptr->iminrowlength)) {
				return (vb_rtd->iserrno);
			}
		}
	}
	memcpy (pcwritebuffer, pcbuffer, (size_t) irowlength);
	*(pcwritebuffer + irowlength) = ideletedrow ? 0x00 : 0x0a;
	irowlength++;
	if (tvbptr->iopenmode & ISVARLEN) {
		inl_stint (tvbptr->ivarlenlength, pcwritebuffer + irowlength);
		inl_stcompx (tvbptr->tvarlennode, pcwritebuffer + irowlength + INTSIZE,
					 tvbptr->iquadsize);
		pctemp = pcwritebuffer + irowlength + INTSIZE;
		if (tvbptr->iformat == V_ISAM_FILE) {
			inl_stint ((tvbptr->ivarlenslot << 6) + inl_ldint (pctemp), pctemp);
		} else {
			*pctemp = tvbptr->ivarlenslot;
		}
		irowlength += INTSIZE + tvbptr->iquadsize;
	}

	tblocknumber = (toffset / tvbptr->inodesize);
	toffset -= (tblocknumber * tvbptr->inodesize);
	while (tsofar < irowlength) {
		memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
		ivbblockread (ihandle, 0, tblocknumber + 1, cvbnodetmp);	/* Can fail!! */
		if ((irowlength - tsofar) <= (tvbptr->inodesize - toffset)) {
			memcpy (cvbnodetmp + toffset, pcwritebuffer + tsofar,
					(size_t) (irowlength - tsofar));
			if (ivbblockwrite (ihandle, 0, tblocknumber + 1, cvbnodetmp)) {
				return (EBADFILE);
			}
			break;
		}
		memcpy (cvbnodetmp + toffset, pcwritebuffer + tsofar,
				(size_t) (tvbptr->inodesize - toffset));
		if (ivbblockwrite (ihandle, 0, tblocknumber + 1, cvbnodetmp)) {
			return (EBADFILE);
		}
		tblocknumber++;
		tsofar += tvbptr->inodesize - toffset;
		toffset = 0;
	}
	return (0);
}
