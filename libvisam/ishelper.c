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

/* Global functions */

int
iscluster (int ihandle, struct keydesc *pskeydesc)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	/* BUG Write iscluster() and don't forget to call ivbtranscluster */
	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}
	return 0;
}

int
iserase (VB_CHAR * pcfilename)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	int         ihandle, sts;
	VB_CHAR     cbuffer[1024];

	vbclrstatus ();
	for (ihandle = 0; ihandle <= vb_rtd->ivbmaxusedhandle; ihandle++) {
		if (vb_rtd->psvbfile[ihandle] != NULL) {
			if (!strcmp
				((char *) vb_rtd->psvbfile[ihandle]->cfilename,
				 (char *) pcfilename)) {
				isclose (ihandle);
				ivbclose3 (ihandle);
				vbclrstatus ();
				break;
			}
		}
	}
	sprintf ((char *) cbuffer, "%s.idx", pcfilename);
	unlink ((char *) cbuffer);
	vbdatfilename ((char *) cbuffer, (char *) pcfilename);
	unlink ((char *) cbuffer);
	unlink ((char *) pcfilename);	   /* May not have the .dat */
#if	(VBLOGGING == 1)
	sts = ivbtranserase (pcfilename);
#else
	sts = 0;
#endif
	vbsetstatus ();
	return sts;
}

int
isflush (int ihandle)
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
	errno = 0;
	if (psvbptr->iindexhandle >= 0) {
		fsync (vb_rtd->svbfile[psvbptr->iindexhandle].ihandle);
	}
	errno = 0;
	if (psvbptr->idatahandle >= 0) {
		fsync (vb_rtd->svbfile[psvbptr->idatahandle].ihandle);
	}
	vb_rtd->iserrno = errno;
	vbsetstatus ();
	return 0;
}

int
islock (int ihandle)
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
	return ivbdatalock (ihandle, VBWRLOCK, (off_t) 0);
}

int
isrelcurr (int ihandle)
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
	if (vb_rtd->ivbintrans != VBNOTRANS) {
		return 0;
	}
	if (!psvbptr->trownumber) {
		vb_rtd->iserrno = ENOREC;
		vbsetstatus ();
		return -1;
	}
	vb_rtd->iserrno = ivbdatalock (ihandle, VBUNLOCK, psvbptr->trownumber);
	if (vb_rtd->iserrno) {
		vbsetstatus ();
		return -1;
	}
	vbsetstatus ();
	return 0;
}

int
isrelease (int ihandle)
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
	if (vb_rtd->ivbintrans != VBNOTRANS) {
		vbsetstatus ();
		return 0;
	}
	ivbdatalock (ihandle, VBUNLOCK, (off_t) 0);	/* Ignore the return */
	vbsetstatus ();
	return 0;
}

int
isrelrec (int ihandle, vbisam_rec_n trownumber)
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
	vb_rtd->iserrno = ivbdatalock (ihandle, VBUNLOCK, trownumber);
	if (vb_rtd->iserrno) {
		vbsetstatus ();
		return -1;
	}
	return 0;
}

int
isrename (VB_CHAR * pcoldname, VB_CHAR * pcnewname)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	int         iresult;
	VB_CHAR     cbuffer[2][1024];

	sprintf ((char *) cbuffer[0], "%s.idx", pcoldname);
	sprintf ((char *) cbuffer[1], "%s.idx", pcnewname);
	iresult = rename ((char *) cbuffer[0], (char *) cbuffer[1]);
	if (iresult == -1) {
		goto renameexit;
	}
	sprintf ((char *) cbuffer[0], "%s.dat", pcoldname);
	sprintf ((char *) cbuffer[1], "%s.dat", pcnewname);
	iresult = rename ((char *) cbuffer[0], (char *) cbuffer[1]);
	if (iresult == -1) {
		sprintf ((char *) cbuffer[0], "%s", pcoldname);
		sprintf ((char *) cbuffer[1], "%s", pcnewname);
		iresult = rename ((char *) cbuffer[0], (char *) cbuffer[1]);
	}
	if (iresult == -1) {
		sprintf ((char *) cbuffer[0], "%s.idx", pcoldname);
		sprintf ((char *) cbuffer[1], "%s.idx", pcnewname);
		rename ((char *) cbuffer[1], (char *) cbuffer[0]);
		goto renameexit;
	}
	return ivbtransrename (pcoldname, pcnewname);
  renameexit:
	vb_rtd->iserrno = errno;
	return -1;
}

int
issetunique (int ihandle, vbisam_rec_n tuniqueid)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       tvalue;
	int         iresult, iresult2;

	if (ivbenter (ihandle, 1)) {
		return -1;
	}
	tvbptr = vb_rtd->psvbfile[ihandle];
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		ivbexit (ihandle);
		return -1;
	}

	vb_rtd->iserrno = 0;
	tvalue = tvbptr->iuniqueid;
	if (tuniqueid > tvalue) {
		tvbptr->iuniqueid = tuniqueid;
		tvbptr->iisdictlocked |= 0x02;
	}

	iresult = ivbtranssetunique (ihandle, tuniqueid);
	tvbptr->iisdictlocked |= 0x02;
	iresult2 = ivbexit (ihandle);
	if (iresult) {
		return -1;
	}
	return iresult2;
}

int
isuniqueid (int ihandle, vbisam_rec_n * ptuniqueid)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *tvbptr;
	off_t       tvalue;
	int         iresult = 0;
	int         iresult2;

	if (ivbenter (ihandle, 1)) {
		return -1;
	}

	tvbptr = vb_rtd->psvbfile[ihandle];
	vb_rtd->iserrno = EBADARG;
	if (!tvbptr->iisdictlocked) {
		ivbexit (ihandle);
		return -1;
	}
	vb_rtd->iserrno = 0;

	tvalue = tvbptr->iuniqueid;
	tvbptr->iuniqueid++;
	tvbptr->iisdictlocked |= 0x02;
	iresult = ivbtransuniqueid (ihandle, tvalue);
	iresult2 = ivbexit (ihandle);
	if (iresult) {
		return -1;
	}
	*ptuniqueid = tvalue;
	return iresult2;
}

int
isunlock (int ihandle)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbptr;

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
	return ivbdatalock (ihandle, VBUNLOCK, (off_t) 0);
}

void
ldchar (VB_CHAR * pcsource, int ilength, VB_CHAR * pcdestination)
{
	VB_CHAR    *pcdst;

	memcpy ((void *) pcdestination, (void *) pcsource, (size_t) ilength);
	for (pcdst = pcdestination + ilength - 1; pcdst >= (VB_CHAR *) pcdestination;
		 pcdst--) {
		if (*pcdst != ' ') {
			pcdst++;
			*pcdst = 0;
			return;
		}
	}
	*(++pcdst) = 0;
}

void
stchar (VB_CHAR * pcsource, VB_CHAR * pcdestination, int ilength)
{
	VB_CHAR    *pcsrc, *pcdst;
	int         icount;

	pcsrc = pcsource;
	pcdst = pcdestination;
	for (icount = ilength; icount && *pcsrc; icount--, pcsrc++, pcdst++) {
		*pcdst = *pcsrc;
	}
	for (; icount; icount--, pcdst++) {
		*pcdst = ' ';
	}
}

double
ldfloat (void *pclocation)
{
	float       ffloat;
	double      ddouble;

	memcpy (&ffloat, pclocation, FLOATSIZE);
	ddouble = ffloat;
	return (double) ddouble;
}

void
stfloat (double dsource, void *pcdestination)
{
	float       ffloat;

	ffloat = (float) dsource;
	memcpy (pcdestination, &ffloat, FLOATSIZE);
}

double
ldfltnull (void *pclocation, short *pinullflag)
{
	double      dvalue;

	*pinullflag = 0;
	dvalue = ldfloat (pclocation);
	return dvalue;
}

void
stfltnull (double dsource, void *pcdestination, int inullflag)
{
	if (inullflag) {
		dsource = 0;
	}
	stfloat (dsource, pcdestination);
}

double
lddbl (void *pclocation)
{
	double      ddouble;

	memcpy (&ddouble, pclocation, DOUBLESIZE);
	return ddouble;
}

void
stdbl (double dsource, void *pcdestination)
{
	memcpy (pcdestination, &dsource, DOUBLESIZE);
}

double
lddblnull (void *pclocation, short *pinullflag)
{
	*pinullflag = 0;
	return (lddbl (pclocation));
}

void
stdblnull (double dsource, void *pcdestination, int inullflag)
{
	if (inullflag) {
		dsource = 0;
	}
	stdbl (dsource, pcdestination);
}
