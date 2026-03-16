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

static VB_CHAR *gpsdatarow;		/*  Buffer to hold rows read */
static VB_CHAR *gpsdatamap[2];	/*  Bitmap of 'used' data rows */
static VB_CHAR *gpsindexmap[2];	/*  Bitmap of 'used' index nodes */
static VB_CHAR *cvbnodetmp;
static char rbldfilename[256] = "";
static off_t gtlastuseddata;	/*  Last row USED in data file */
static off_t gtdatasize;		/*  # Rows in data file */
static off_t gtindexsize;		/*  # Nodes in index file */
static int  girebuilddatafree;	/*  If set, we need to rebuild data free list */
static int  girebuildindexfree;	/*  If set, we need to rebuild index free list */
static int  girebuildkey[MAXSUBS];	/*  For any are SET, we need to rebuild that key */

static int
ibittestandset (VB_CHAR * psmap, off_t tbit)
{
	tbit--;
	switch (tbit % 8) {
	case 0:
		if (psmap[tbit / 8] & 0x80) {
			return 1;
		}
		psmap[tbit / 8] |= 0x80;
		break;
	case 1:
		if (psmap[tbit / 8] & 0x40) {
			return 1;
		}
		psmap[tbit / 8] |= 0x40;
		break;
	case 2:
		if (psmap[tbit / 8] & 0x20) {
			return 1;
		}
		psmap[tbit / 8] |= 0x20;
		break;
	case 3:
		if (psmap[tbit / 8] & 0x10) {
			return 1;
		}
		psmap[tbit / 8] |= 0x10;
		break;
	case 4:
		if (psmap[tbit / 8] & 0x08) {
			return 1;
		}
		psmap[tbit / 8] |= 0x08;
		break;
	case 5:
		if (psmap[tbit / 8] & 0x04) {
			return 1;
		}
		psmap[tbit / 8] |= 0x04;
		break;
	case 6:
		if (psmap[tbit / 8] & 0x02) {
			return 1;
		}
		psmap[tbit / 8] |= 0x02;
		break;
	case 7:
		if (psmap[tbit / 8] & 0x01) {
			return 1;
		}
		psmap[tbit / 8] |= 0x01;
		break;
	}
	return 0;
}

static int
ibittestandreset (VB_CHAR * psmap, off_t tbit)
{
	tbit--;
	switch (tbit % 8) {
	case 0:
		if (!(psmap[tbit / 8] & 0x80)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x80;
		break;
	case 1:
		if (!(psmap[tbit / 8] & 0x40)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x40;
		break;
	case 2:
		if (!(psmap[tbit / 8] & 0x20)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x20;
		break;
	case 3:
		if (!(psmap[tbit / 8] & 0x10)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x10;
		break;
	case 4:
		if (!(psmap[tbit / 8] & 0x08)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x08;
		break;
	case 5:
		if (!(psmap[tbit / 8] & 0x04)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x04;
		break;
	case 6:
		if (!(psmap[tbit / 8] & 0x02)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x02;
		break;
	case 7:
		if (!(psmap[tbit / 8] & 0x01)) {
			return 1;
		}
		psmap[tbit / 8] ^= 0x01;
		break;
	}
	return 0;
}

static int
ipreamble (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	struct stat sstat;

	psvbptr = vb_rtd->psvbfile[ihandle];
	printf ("index block size: %d bytes\n", psvbptr->inodesize);
	if (psvbptr->iminrowlength == psvbptr->imaxrowlength) {
		printf ("data record size: %d bytes\n", psvbptr->iminrowlength);
	} else {
		printf (" min record size: %d bytes\n", psvbptr->iminrowlength);
		printf (" max record size: %d bytes\n", psvbptr->imaxrowlength);
	}
	printf (" index dup width: %d \n", psvbptr->iduplen);
	girebuilddatafree = 0;
	girebuildindexfree = 0;
	gtlastuseddata = 0;
	gpsdatarow = pvvbmalloc ((size_t) psvbptr->imaxrowlength + 1);
	if (!gpsdatarow) {
		printf ("Unable to allocate data row buffer!\n");
		return -1;
	}
	if (fstat (vb_rtd->svbfile[psvbptr->idatahandle].ihandle, &sstat)) {
		printf ("Unable to get data status!\n");
		return -1;
	}
	if (psvbptr->iopenmode & ISVARLEN) {
		gtdatasize =
			(off_t) (sstat.st_size + psvbptr->iminrowlength + INTSIZE +
					 psvbptr->iquadsize) / (psvbptr->iminrowlength + INTSIZE +
											psvbptr->iquadsize + 1);
	} else {
		gtdatasize = (off_t) (sstat.st_size +
							  psvbptr->iminrowlength) / (psvbptr->iminrowlength + 1);
	}
	gpsdatamap[0] = pvvbmalloc ((size_t) ((gtdatasize + 7) / 8));
	if (gpsdatamap[0] == NULL) {
		printf ("Unable to allocate node map!\n");
		return -1;
	}
	gpsdatamap[1] = pvvbmalloc ((size_t) ((gtdatasize + 7) / 8));
	if (gpsdatamap[1] == NULL) {
		printf ("Unable to allocate node map!\n");
		return -1;
	}

	if (fstat (vb_rtd->svbfile[psvbptr->iindexhandle].ihandle, &sstat)) {
		printf ("Unable to get index status!\n");
		return -1;
	}
	gtindexsize = (off_t) (sstat.st_size + psvbptr->inodesize -
						   1) / psvbptr->inodesize;
	gpsindexmap[0] = pvvbmalloc ((size_t) ((gtindexsize + 7) / 8));
	if (gpsindexmap[0] == NULL) {
		printf ("Unable to allocate node map!\n");
		return -1;
	}
	gpsindexmap[1] = pvvbmalloc ((size_t) ((gtindexsize + 7) / 8));
	if (gpsindexmap[1] == NULL) {
		printf ("Unable to allocate node map!\n");
		return -1;
	}
	cvbnodetmp = pvvbmalloc (MAX_NODE_LENGTH);
	if (cvbnodetmp == NULL) {
		printf ("Unable to allocate buffer!\n");
		return -1;
	}
	switch (gtindexsize % 8) {
	case 1:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x7f;
		break;
	case 2:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x3f;
		break;
	case 3:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x1f;
		break;
	case 4:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x0f;
		break;
	case 5:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x07;
		break;
	case 6:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x03;
		break;
	case 7:
		*(gpsindexmap[0] + ((gtindexsize - 1) / 8)) = 0x01;
		break;
	}
	ibittestandset (gpsindexmap[0], 1);	/*  Dictionary node! */

	return 0;
}

static int
idatacheck (int ihandle)
{
	off_t       tloop;
	int         ideleted;
	long        delrow = 0;

	printf ("Checking data records\n");
	/*  Mark the entries used by *LIVE* data rows */
	for (tloop = 1; tloop <= gtdatasize; tloop++) {
		if (ivbdataread (ihandle, gpsdatarow, &ideleted, tloop)) {
			continue;				   /*  A data file read error! Leave it as free! */
		}
		if ((tloop % 10) == 0) {
			printf ("Current row : %ld\r", (long) tloop);
		}
		if (!ideleted) {
			gtlastuseddata = tloop;
			/*  MAYBE we could add index verification here */
			/*  That'd be in SUPER THOROUGH mode only! */
			ibittestandset (gpsdatamap[0], tloop);
		} else {
			delrow++;
		}
		/*  BUG - We also need to set gpsindexmap [1] for those node(s) */
		/*  BUG - that were at least partially consumed by VARLEN data! */
	}
	if (delrow > 0) {
		printf (" Total records  : %ld    Deleted records: %ld\n", (long) gtdatasize,
				(long) delrow);
		printf (" Active records : %ld \n", (long) (gtdatasize - delrow));
	} else {
		printf (" Total records  : %ld\n", (long) gtdatasize);
	}
	printf ("\n");
	return 0;
}

static int
idatafreecheck (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tfreehead, tfreerow, tholdhead, tloop;
	int         iloop, iresult;

	psvbptr = vb_rtd->psvbfile[ihandle];
	girebuilddatafree = 1;
	/*  Mark the entries used by the free data list */
	tholdhead = psvbptr->idatafree;
	psvbptr->idatafree = 0;
	tfreehead = tholdhead;
	memcpy (gpsdatamap[1], gpsdatamap[0], (int) ((gtdatasize + 7) / 8));
	memcpy (gpsindexmap[1], gpsindexmap[0], (int) ((gtindexsize + 7) / 8));
	while (tfreehead) {
		/*  If the freelist node is > index.EOF, it must be bullshit! */
		if (tfreehead > gtindexsize) {
			return 0;
		}
		iresult = ivbblockread (ihandle, 1, tfreehead, cvbnodetmp);
		if (iresult) {
			return 0;
		}
		/*
		 * If the node has the WRONG signature, then we've got
		 * a corrupt data free list.  We'll rebuild it later!
		 */
		if (psvbptr->iformat == V_ISAM_FILE
			&& cvbnodetmp[psvbptr->inodesize - 2] != 0x7f) {
			return 0;
		}
		if (cvbnodetmp[psvbptr->inodesize - 3] != -1) {
			return 0;
		}
		if (inl_ldint (cvbnodetmp) > (psvbptr->inodesize - 3)) {
			return 0;
		}
		/*
		 * If the node is already 'used' then we have a corrupt
		 * data free list (circular reference).
		 * We'll rebuild the free list later
		 */
		if (ibittestandset (gpsindexmap[1], tfreehead)) {
			return 0;
		}
		for (iloop = INTSIZE + psvbptr->iquadsize; iloop < inl_ldint (cvbnodetmp);
			 iloop += psvbptr->iquadsize) {
			tfreerow = inl_ldcompx (cvbnodetmp + iloop, psvbptr->iquadsize);
			/*
			 * If the row is NOT deleted, then the free
			 * list is screwed so we ignore it and rebuild it
			 * later.
			 */
			if (ibittestandset (gpsdatamap[1], tfreerow)) {
				return 0;
			}
		}
		tfreehead = inl_ldcompx (cvbnodetmp + INTSIZE, psvbptr->iquadsize);
	}
	/*  Set the few bits between the last row used and EOF to 'used' */
	for (tloop = gtlastuseddata + 1; tloop <= ((gtdatasize + 7) / 8) * 8; tloop++) {
		ibittestandset (gpsdatamap[1], tloop);
	}
	for (tloop = 0; tloop < (gtdatasize + 7) / 8; tloop++) {
		if (gpsdatamap[1][tloop] != -1) {
			return 0;
		}
	}
	/*  Seems the data file is 'intact' so we'll keep the allocation lists! */
	memcpy (gpsdatamap[0], gpsdatamap[1], (int) ((gtdatasize + 7) / 8));
	memcpy (gpsindexmap[0], gpsindexmap[1], (int) ((gtindexsize + 7) / 8));
	psvbptr->idatafree = tholdhead;
	girebuilddatafree = 0;

	return 0;
}

static int
iindexfreecheck (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tfreehead, tfreenode, tholdhead;
	int         iloop, iresult;

	psvbptr = vb_rtd->psvbfile[ihandle];
	/*  Mark the entries used by the free data list */
	girebuildindexfree = 1;
	tfreehead = psvbptr->inodefree;
	tholdhead = tfreehead;
	psvbptr->inodefree = 0;
	memcpy (gpsindexmap[1], gpsindexmap[0], (int) ((gtindexsize + 7) / 8));
	while (tfreehead) {
		/*  If the freelist node is > index.EOF, it must be bullshit! */
		if (tfreehead > psvbptr->inodecount) {
			return 0;
		}
		if (tfreehead > gtindexsize) {
			return 0;
		}
		iresult = ivbblockread (ihandle, 1, tfreehead, cvbnodetmp);
		if (iresult) {
			return 0;
		}
		/*
		 * If the node has the WRONG signature, then we've got
		 * a corrupt data free list.  We'll rebuild it later!
		 */
		if (psvbptr->iformat == V_ISAM_FILE
			&& cvbnodetmp[psvbptr->inodesize - 2] != 0x7f) {
			return 0;
		}
		if (cvbnodetmp[psvbptr->inodesize - 3] != -2) {
			return 0;
		}
		if (inl_ldint (cvbnodetmp) > (psvbptr->inodesize - 3)) {
			return 0;
		}
		/*
		 * If the node is already 'used' then we have a corrupt
		 * index free list (circular reference).
		 * We'll rebuild the free list later
		 */
		if (ibittestandset (gpsindexmap[1], tfreehead)) {
			return 0;
		}
		for (iloop = INTSIZE + psvbptr->iquadsize; iloop < inl_ldint (cvbnodetmp);
			 iloop += psvbptr->iquadsize) {
			tfreenode = inl_ldcompx (cvbnodetmp + iloop, psvbptr->iquadsize);
			/*
			 * If the row is NOT deleted, then the free
			 * list is screwed so we ignore it and rebuild it
			 * later.
			 */
			if (ibittestandset (gpsindexmap[1], tfreenode)) {
				return 0;
			}
		}
		tfreehead = inl_ldcompx (cvbnodetmp + INTSIZE, psvbptr->iquadsize);
	}
	/*  Seems the index free list is 'intact' so we'll keep the allocation lists! */
	memcpy (gpsindexmap[0], gpsindexmap[1], (int) ((gtindexsize + 7) / 8));
	psvbptr->inodefree = tholdhead;
	girebuildindexfree = 0;

	return 0;
}

static int
icheckkeydesc (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tnode;

	psvbptr = vb_rtd->psvbfile[ihandle];
	tnode = psvbptr->inodekeydesc;
	while (tnode) {
		if (tnode > gtindexsize) {
			return 1;
		}
		if (ibittestandset (gpsindexmap[0], tnode)) {
			return 1;
		}
		if (ivbblockread (ihandle, 1, tnode, cvbnodetmp)) {
			return 1;
		}
		if (cvbnodetmp[psvbptr->inodesize - 3] != -1) {
			return 1;
		}
		if (cvbnodetmp[psvbptr->inodesize - 2] != 0x7e) {
			return 1;
		}
		if (cvbnodetmp[psvbptr->inodesize - 1] != '\n'	/* Either LF or NULL is valid */
			&& cvbnodetmp[psvbptr->inodesize - 1] != 0) {
			return 1;
		}
		tnode = inl_ldcompx (cvbnodetmp + INTSIZE, psvbptr->iquadsize);
	}
	return 0;
}

static int
ichecktree (int ihandle, int ikey, off_t tnode, int ilevel)
{
	int         iloop;
	struct VBTREE stree;
	struct DICTINFO *psvbptr;
	vb_rtd_t   *vb_rtd = VB_GET_RTD;

	psvbptr = vb_rtd->psvbfile[ihandle];

	memset (&stree, 0, sizeof (stree));
	if (tnode > gtindexsize) {
		return 1;
	}
	if (ibittestandset (gpsindexmap[1], tnode)) {
		return 1;
	}
	stree.ttransnumber = -1;
	if (ivbnodeload (ihandle, ikey, &stree, tnode, ilevel)) {
		return 1;
	}
	for (iloop = 0; iloop < stree.ikeysinnode; iloop++) {
		if (stree.pskeylist[iloop]->iisdummy) {
			continue;
		}
		if (iloop > 0
			&& ivbkeycompare (ihandle, ikey, 0, stree.pskeylist[iloop - 1]->ckey,
							  stree.pskeylist[iloop]->ckey,
							  psvbptr->iquadsize) > 0) {
			printf ("Index %d is out of order!\n", ikey + 1);
			vvbkeyallfree (ihandle, ikey, &stree);
			return 1;
		}
		if (iloop > 0
			&& ivbkeycompare (ihandle, ikey, 0, stree.pskeylist[iloop - 1]->ckey,
							  stree.pskeylist[iloop]->ckey, psvbptr->iquadsize) == 0
			&& stree.pskeylist[iloop - 1]->tdupnumber >=
			stree.pskeylist[iloop]->tdupnumber) {
			printf ("Index %d is out of order at dup %ld!!\n", ikey + 1,
					(long) stree.pskeylist[iloop]->tdupnumber);
			vvbkeyallfree (ihandle, ikey, &stree);
			return 1;
		}
		if (stree.ilevel) {
			if (ichecktree (ihandle, ikey, stree.pskeylist[iloop]->trownode,
							stree.ilevel)) {
				vvbkeyallfree (ihandle, ikey, &stree);
				return 1;
			}
		} else {
			if (ibittestandreset (gpsdatamap[1], stree.pskeylist[iloop]->trownode)) {
				vvbkeyallfree (ihandle, ikey, &stree);
#if (defined (WITH_LFS64) || defined (VISAM_64BIT))
				printf ("Bad data row pointer! Row: %lld\n",
								(long long)(stree.pskeylist[iloop]->trownode));
#else
				printf ("Bad data row pointer! Row: %ld\n",
								(long)(stree.pskeylist[iloop]->trownode));
#endif
				return 1;
			}
		}
	}
	vvbkeyallfree (ihandle, ikey, &stree);
	return 0;
}

static int
icheckkey (int ihandle, int ikey)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	off_t       tloop;

	psvbptr = vb_rtd->psvbfile[ihandle];
	if (isstart (ihandle, psvbptr->pskeydesc[ikey], 0, NULL, ISFIRST) < 0) {
		return 1;
	}

	memcpy (gpsdatamap[1], gpsdatamap[0], (int) ((gtdatasize + 7) / 8));
	memcpy (gpsindexmap[1], gpsindexmap[0], (int) ((gtindexsize + 7) / 8));
	if (ivbblockread (ihandle, 1, psvbptr->pskeydesc[ikey]->k_rootnode, cvbnodetmp)) {
		return 1;
	}
	if (ichecktree (ihandle, ikey, psvbptr->pskeydesc[ikey]->k_rootnode,
					cvbnodetmp[psvbptr->inodesize - 2] + 1)) {
		return 1;
	}
	for (tloop = 0; tloop < (gtdatasize + 7) / 8; tloop++) {
		if (gpsdatamap[1][tloop]) {
			return 1;
		}
	}
	memcpy (gpsindexmap[0], gpsindexmap[1], (int) ((gtindexsize + 7) / 8));
	return 0;
}

static int
iindexcheck (int ihandle, int docheck)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	unsigned char ch;
	int         ikey, iloop, ipart;

	if (docheck) {
		printf ("Checking INDEX\n");
		for (iloop = 0; iloop < MAXSUBS; iloop++) {
			girebuildkey[iloop] = 0;
		}
		/*  If the keydesc node(s) are bad, we QUIT this table altogether */
		if (icheckkeydesc (ihandle)) {
			printf ("Corrupted Key Descriptor node(s)! Can't continue!\n");
			return -1;
		}
	}

	psvbptr = vb_rtd->psvbfile[ihandle];
	for (ikey = 0; ikey < psvbptr->inkeys; ikey++) {
		printf ("Index %d: ", ikey + 1);
		printf ("%s ",
				psvbptr->pskeydesc[ikey]->k_flags & ISDUPS ? "ISDUPS" : "ISNODUPS");
		if (psvbptr->pskeydesc[ikey]->k_flags & NULLKEY) {
			printf (" NULLKEY");
		}
		printf ("%s",
				psvbptr->pskeydesc[ikey]->k_flags & DCOMPRESS ? "DCOMPRESS " : "");
		printf ("%s",
				psvbptr->pskeydesc[ikey]->k_flags & LCOMPRESS ? "LCOMPRESS " : "");
		printf ("%s\n",
				psvbptr->pskeydesc[ikey]->k_flags & TCOMPRESS ? "TCOMPRESS" : "");
		for (ipart = 0; ipart < psvbptr->pskeydesc[ikey]->k_nparts; ipart++) {
			printf (" Part %d: ", ipart + 1);
			printf ("%d,", psvbptr->pskeydesc[ikey]->k_part[ipart].kp_start);
			printf ("%d,", psvbptr->pskeydesc[ikey]->k_part[ipart].kp_leng);
			switch ((psvbptr->pskeydesc[ikey]->k_part[ipart].kp_type & BYTEMASK) & ~ISDESC) {
			case CHARTYPE:
				printf ("CHARTYPE");
				break;
			case INTTYPE:
				printf ("INTTYPE");
				break;
			case LONGTYPE:
				printf ("LONGTYPE");
				break;
			case MINTTYPE:
				printf ("MINTTYPE");
				break;
			case MLONGTYPE:
				printf ("MLONGTYPE");
				break;
			case DOUBLETYPE:
				printf ("DOUBLETYPE");
				break;
			case FLOATTYPE:
				printf ("FLOATTYPE");
				break;
			case QUADTYPE:
				printf ("QUADTYPE");
				break;
			default:
				printf ("UNKNOWN TYPE %X",
						(psvbptr->pskeydesc[ikey]->k_part[ipart].kp_type & BYTEMASK) & ~ISDESC);
				break;
			}
			if (psvbptr->pskeydesc[ikey]->k_flags & NULLKEY) {
				ch = (psvbptr->pskeydesc[ikey]->k_part[ipart].kp_type >> BYTESHFT) & BYTEMASK;
				if (isprint (ch))
					printf ("  NULL Char '%c'", ch);
				else
					printf ("  NULL Char 0x%02X", ch);
			}
			if (psvbptr->pskeydesc[ikey]->k_part[ipart].kp_type & ISDESC) {
				printf (" ISDESC\n");
			} else {
				printf ("\n");
			}
		}
		/*  If the index is screwed, write out an EMPTY root node and */
		/*  flag the index for a complete reconstruction later on */
		if (docheck && icheckkey (ihandle, ikey)) {
			/*
			   memset (cvbnodetmp, 0, MAX_NODE_LENGTH);
			   stint (2, cvbnodetmp);
			   ivbblockwrite (ihandle, 1,
			   psvbptr->pskeydesc[ikey]->k_rootnode,
			   cvbnodetmp);
			   ibittestandset (gpsindexmap[0],
			   psvbptr->pskeydesc[ikey]->k_rootnode);
			   vvbtreeallfree (ihandle, ikey, psvbptr->pstree[ikey]);
			   psvbptr->pstree[ikey] = NULL;
			 */
			girebuildkey[ikey] = 1;

		}
	}
	return 0;
}


int         VBiaddkeydescriptor (const int ihandle, struct keydesc *pskeydesc);


static void
vrebuildkeys (int ihandle, const int format)
{
	off_t       trownumber;
	int         ideleted, ikey;
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	int         iNewhandel;
	int         ires;
	int         irowcnt = 0;

	psvbptr = vb_rtd->psvbfile[ihandle];

	sprintf (rbldfilename, "VBISAM_REBUILD_%d", getpid ());
	iserase ((VB_CHAR *) rbldfilename);
	vb_rtd->isreclen = psvbptr->iminrowlength;
	iNewhandel = isbuild ((void *) rbldfilename, (int) psvbptr->imaxrowlength,
						  psvbptr->pskeydesc[0],
						  (int) (ISINOUT | ISEXCLLOCK | format |
								 ((psvbptr->iminrowlength) !=
								  (psvbptr->imaxrowlength)
								  ? ISVARLEN : 0)));
	for (ikey = 1; ikey < MAXSUBS; ikey++) {
		if (psvbptr->pskeydesc[ikey]) {
			vb_rtd->iserrno = 0;
			isaddindex (iNewhandel, psvbptr->pskeydesc[ikey]);
		}
	}
	printf ("\nCreating temporary : %s\n", rbldfilename);

	/*  Mark the entries used by *LIVE* data rows */
	for (trownumber = 1; trownumber <= gtdatasize; trownumber++) {
		if (ivbdataread (ihandle, gpsdatarow, &ideleted, trownumber)) {
			printf ("Error %d reading data row %ld!\n", vb_rtd->iserrno,
					(long) trownumber);
			continue;				   /*  A data file read error! Leave it as free! */
		}
		if (!ideleted) {
			irowcnt++;
			ires = iswrite (iNewhandel, gpsdatarow);
			if (ires < 0) {
				printf ("Error %d writing data row %ld!\n", vb_rtd->iserrno,
						(long) trownumber);
				/*
				   for ( ikey = 0; ikey < MAXSUBS; ikey++ ) {
				   if ( psvbptr->pskeydesc[ikey] ) {
				   vaddkeyforrow (ihandle, ikey, trownumber);
				   }
				   }
				 */
			}
		}
		if ((trownumber % 100) == 0) {
			printf ("Current row : %ld\r", (long) trownumber);
		}
	}
	printf ("  Total read row  : %ld    \n", (long) gtdatasize);
	printf ("  Total added row : %d    \n", irowcnt);
	isclose (iNewhandel);
	return;
}

static void
ipostamble (int ihandle, const int format)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	/*off_t   tloop; */
	int         ikeycounttorebuild = 0, iloop;

	/*
	   for ( tloop = 0; tloop < (gtindexsize + 7) / 8; tloop++ ) {
	   if ( gpsindexmap[0][tloop] != -1 ) {
	   girebuildindexfree = 1;
	   }
	   }
	 */

	/*
	   if ( girebuildindexfree ) {
	   vrebuildindexfree (ihandle);
	   }
	   if ( girebuilddatafree ) {
	   vrebuilddatafree (ihandle);
	   }
	 */
	for (iloop = 0; iloop < MAXSUBS; iloop++)
		if (girebuildkey[iloop]) {
			if (!ikeycounttorebuild) {
			}
			ikeycounttorebuild++;
		}
	if (ikeycounttorebuild || girebuildindexfree || girebuilddatafree) {
		printf ("Rebuilding file : ");
		if (format & ISMVBISAM)
			printf (" VB-ISAM format");
		if (format & ISMCISAM)
			printf (" C-ISAM format");
		vrebuildkeys (ihandle, format);
		printf ("\n");
	}
	vb_rtd->psvbfile[ihandle]->idatacount = gtlastuseddata;
	/*  Other stuff here */
	vvbfree (gpsdatarow, vb_rtd->psvbfile[ihandle]->imaxrowlength);
	vvbfree (cvbnodetmp, MAX_NODE_LENGTH);

	vvbfree (gpsdatamap[0], ((int) ((gtdatasize + 7) / 8)));
	vvbfree (gpsdatamap[1], ((int) ((gtdatasize + 7) / 8)));
	vvbfree (gpsindexmap[0], ((int) ((gtindexsize + 7) / 8)));
	vvbfree (gpsindexmap[1], ((int) ((gtindexsize + 7) / 8)));
}

static void
vprocess (int ihandle, const int rebuild, const int format)
{
	if (ipreamble (ihandle)) {
		return;
	}
	if (idatacheck (ihandle)) {
		return;
	}
	if (iindexcheck (ihandle, 1)) {
		return;
	}
	if (idatafreecheck (ihandle)) {
		return;
	}
	if (iindexfreecheck (ihandle)) {
		return;
	}
	if (rebuild) {
		ipostamble (ihandle, format);
	}
}

int
ischeck (const VB_CHAR * pcfile, const int rebuild, const int format)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;
	int         ihandle;
	char        tmpfile[260], newfile[260];

	rbldfilename[0] = 0;
	ihandle = isopen (pcfile, ISINPUT);
	if (ihandle < 0) {
		printf ("Error %d opening %s\n", vb_rtd->iserrno, pcfile);
		return -1;
	}
	psvbptr = vb_rtd->psvbfile[ihandle];
	if (psvbptr->iopenmode & ISVARLEN) {
		printf ("Re%s of ISVARLEN file %s not possible at this time\n",
				rebuild ? "build" : "covery", pcfile);
		iindexcheck (ihandle, 0);
		return -1;
	}
	if (psvbptr->iminrowlength != psvbptr->imaxrowlength) {
		printf ("Re%s of var len file %s not possible at this time\n",
				rebuild ? "build" : "covery", pcfile);
		iindexcheck (ihandle, 0);
		return -1;
	}
	isclose (ihandle);

	ihandle = isopen (pcfile, ISINOUT | ISEXCLLOCK | ISREBUILD | format);
	if (ihandle < 0) {
		printf ("Error %d opening %s\n", vb_rtd->iserrno, pcfile);
		return -1;
	}
	psvbptr = vb_rtd->psvbfile[ihandle];

	printf ("Processing: %s ", pcfile);
	if ((format & ISMCISAM))
		printf ("into C-ISAM format");
	else if ((format & ISMVBISAM))
		printf ("into VB-ISAM format");
	else if (vb_rtd->psvbfile[ihandle]->iformat == V_ISAM_FILE)
		printf ("is VB-ISAM format");
	else
		printf ("is C-ISAM format");
	printf ("\n");

	ivbenter (ihandle, 1);
	vprocess (ihandle, rebuild, format);
	vb_rtd->psvbfile[ihandle]->iisdictlocked |= 0x06;
	ivbexit (ihandle);

	isfullclose (ihandle);
	if (rebuild && rbldfilename[0] > ' ') {
		sprintf (tmpfile, "%s.idx", pcfile);
		unlink (tmpfile);
		sprintf (newfile, "%s.idx", rbldfilename);
		rename ((const char *) newfile, (const char *) tmpfile);

		vbdatfilename (tmpfile, (char *) pcfile);
		unlink ((char *)pcfile);
		unlink (tmpfile);
		if ((format & ISMNODAT))
			strcpy (newfile, rbldfilename);
		else
			vbdatfilename (newfile, (char *) rbldfilename);
		rename ((const char *) newfile, (const char *) tmpfile);
		printf ("Renamed back to: %s\n", pcfile);
	}
	return 0;
}
