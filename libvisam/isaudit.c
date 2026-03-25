/*
 * Copyright (C) 2003 Trevor van Bremen
 * Copyright (C) 2026 pmcnary / NMICA
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

/*
 * isaudit.c — C-ISAM compatible audit trail implementation
 *
 * The audit trail is a per-file, append-only log of all INSERT, UPDATE,
 * and DELETE operations.  Each record consists of an audhead header
 * (type, timestamp, pid, uid, recnum, reclen) followed by the row data.
 *
 * Audit types (stored in au_type):
 *   "aa" — row added (INSERT)
 *   "dd" — row deleted (DELETE)
 *   "rr" — row before rewrite (UPDATE old image)
 *   "ww" — row after rewrite (UPDATE new image)
 *
 * Usage:
 *   isaudit(handle, "/path/to/audit.log", AUDSETNAME);
 *   isaudit(handle, NULL, AUDSTART);
 *   ... INSERT/UPDATE/DELETE operations ...
 *   isaudit(handle, NULL, AUDSTOP);
 */

#include	"isinternal.h"

#include	<fcntl.h>
#include	<time.h>
#include	<unistd.h>
#include	<string.h>
#include	<stdlib.h>

/*
 * ivbauditrecord — write one audit trail record (called from iswrite/isdelete/isrewrite)
 *
 * pctype: "aa" (insert), "dd" (delete), "rr" (old image), "ww" (new image)
 *
 * Returns 0 on success, -1 on error (sets iserrno but does NOT fail the
 * calling DML operation — audit errors are non-fatal per C-ISAM convention).
 */
VB_HIDDEN int
ivbauditrecord (const int ihandle, const VB_CHAR *pctype,
				off_t trownumber, const VB_CHAR *pcrow, int irowlength)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;
	struct audhead  saudhead;
	time_t      tnow;
	ssize_t     nwritten;

	if (ihandle < 0 || ihandle > vb_rtd->ivbmaxusedhandle) {
		return -1;
	}
	psvbfptr = vb_rtd->psvbfile[ihandle];
	if (!psvbfptr || !psvbfptr->iauditactive || psvbfptr->iaudithandle < 0) {
		return 0;	/* No audit active — silently succeed */
	}

	/*
	 * P1-6 FIX: tvbpid/tvbuid are initialised by vinitpiduid() in istrans.c,
	 * which is only called on the first logging operation (islogopen path).
	 * Audit and logging are independent C-ISAM features: an application may
	 * call isaudit(AUDSTART) without ever calling islogopen.  In that case
	 * tvbpid remains 0 (calloc default) and every audit record shows PID=0.
	 * PID 0 is never assigned to a user process, so it is a safe sentinel.
	 * Lazily populate both fields here so ivbauditrecord is self-sufficient.
	 */
	if (!vb_rtd->tvbpid) {
		vb_rtd->tvbpid = (long) getpid ();
		vb_rtd->tvbuid = (long) getuid ();
	}

	/* Build the audit header */
	memset (&saudhead, 0, sizeof (saudhead));
	memcpy (saudhead.au_type, pctype, 2);

	tnow = time (NULL);
	inl_stint ((int)(tnow >> 16), saudhead.au_time);
	inl_stint ((int)(tnow & 0xFFFF), saudhead.au_time + 2);

	inl_stint ((int)(vb_rtd->tvbpid & 0xFFFF), saudhead.au_procid);
	inl_stint ((int)(vb_rtd->tvbuid & 0xFFFF), saudhead.au_userid);
	inl_stint ((int)(trownumber >> 16), saudhead.au_recnum);
	inl_stint ((int)(trownumber & 0xFFFF), saudhead.au_recnum + 2);
	inl_stint (irowlength, saudhead.au_reclen);

	/* Write header + row data atomically (best effort — no partial writes) */
	nwritten = write (psvbfptr->iaudithandle, &saudhead, AUDHEADSIZE);
	if (nwritten != AUDHEADSIZE) {
		return -1;
	}
	if (irowlength > 0 && pcrow) {
		nwritten = write (psvbfptr->iaudithandle, pcrow, (size_t) irowlength);
		if (nwritten != (ssize_t) irowlength) {
			return -1;
		}
	}
	return 0;
}

/*
 * isaudit — C-ISAM audit trail control
 *
 * Modes:
 *   AUDSETNAME (0) — set the audit trail filename for this handle
 *   AUDGETNAME (1) — copy the current audit trail filename into pcfilename
 *   AUDSTART   (2) — open the audit file and begin recording
 *   AUDSTOP    (3) — stop recording and close the audit file
 *   AUDINFO    (4) — return 1 if audit is active, 0 if not
 */
int
isaudit (int ihandle, VB_CHAR *pcfilename, int imode)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	struct DICTINFO *psvbfptr;

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

	switch (imode) {

	case AUDSETNAME:
		if (!pcfilename || !pcfilename[0]) {
			vb_rtd->iserrno = EBADARG;
			vbsetstatus ();
			return -1;
		}
		/* If audit is currently active, stop it first */
		if (psvbfptr->iauditactive && psvbfptr->iaudithandle >= 0) {
			close (psvbfptr->iaudithandle);
			psvbfptr->iaudithandle = -1;
			psvbfptr->iauditactive = 0;
		}
		if (psvbfptr->pcauditfilename) {
			free (psvbfptr->pcauditfilename);
		}
		psvbfptr->pcauditfilename = (VB_CHAR *) strdup ((char *) pcfilename);
		if (!psvbfptr->pcauditfilename) {
			vb_rtd->iserrno = EBADMEM;
			vbsetstatus ();
			return -1;
		}
		break;

	case AUDGETNAME:
		if (!pcfilename) {
			vb_rtd->iserrno = EBADARG;
			vbsetstatus ();
			return -1;
		}
		if (psvbfptr->pcauditfilename) {
			strcpy ((char *) pcfilename, (char *) psvbfptr->pcauditfilename);
		} else {
			pcfilename[0] = '\0';
		}
		break;

	case AUDSTART:
		if (!psvbfptr->pcauditfilename || !psvbfptr->pcauditfilename[0]) {
			vb_rtd->iserrno = EBADARG;
			vbsetstatus ();
			return -1;
		}
		if (psvbfptr->iauditactive) {
			/* Already running — C-ISAM returns EAUDIT */
			vb_rtd->iserrno = EAUDIT;
			vbsetstatus ();
			return -1;
		}
		/* Open audit file append-only, create if needed, mode 0644 */
		psvbfptr->iaudithandle = open (
			(char *) psvbfptr->pcauditfilename,
			O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (psvbfptr->iaudithandle < 0) {
			vb_rtd->iserrno = ELOGOPEN;
			vbsetstatus ();
			return -1;
		}
		psvbfptr->iauditactive = 1;
		break;

	case AUDSTOP:
		if (!psvbfptr->iauditactive) {
			/* Not running — not an error, just a no-op */
			break;
		}
		if (psvbfptr->iaudithandle >= 0) {
			close (psvbfptr->iaudithandle);
			psvbfptr->iaudithandle = -1;
		}
		psvbfptr->iauditactive = 0;
		break;

	case AUDINFO:
		return (int) psvbfptr->iauditactive;

	default:
		vb_rtd->iserrno = EBADARG;
		vbsetstatus ();
		return -1;
	}

	return 0;
}
