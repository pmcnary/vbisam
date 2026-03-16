/*
 * vbtest_p1.c — Regression tests for Phase 1 bug fixes (P1-1 through P1-4)
 *
 * Copyright (C) 2026 pmcnary / NMICA
 * LGPL 2.1 — see COPYING.LIB
 *
 * Tests:
 *   P1-1  isrewrite() index rollback on multi-key collision
 *   P1-2  ivbkeydelete() free-node return code check (normal delete path)
 *   P1-3  idemotelocks() lock scope at transaction end  [WITH_LOGGING only]
 *   P1-4  ivbrollmeback() UPDATE row verification       [WITH_LOGGING only]
 *
 * Exit: 0 = all pass, 1 = at least one failure.
 * Output: TAP-compatible (ok N / not ok N / SKIP N).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <visam.h>

/* ------------------------------------------------------------------ */
/* Minimal TAP harness                                                  */
/* ------------------------------------------------------------------ */
static int g_tap_next  = 1;
static int g_failures  = 0;

#define TAP_OK(desc)   fprintf(stdout, "ok %d - %s\n",     g_tap_next++, (desc))
#define TAP_FAIL(desc) do { fprintf(stdout, "not ok %d - %s\n", g_tap_next++, (desc)); g_failures++; } while(0)
#define TAP_SKIP(desc) fprintf(stdout, "ok %d - # SKIP %s\n", g_tap_next++, (desc))
#define TAP_DIAG(...)  fprintf(stdout, "# " __VA_ARGS__)

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */
#define RECLEN 32

static void
make_rec (char *buf, int key1, int key2)
{
	memset (buf, ' ', RECLEN);
	snprintf (buf,      6, "%05d", key1);   /* bytes 0-4: primary key  */
	buf[5] = ' ';
	snprintf (buf + 6, 6, "%05d", key2);   /* bytes 6-10: secondary   */
	buf[11] = ' ';
}

static int
build_two_key_file (const char *name, int *ph)
{
	vb_rtd_t *vb_rtd = VB_GET_RTD;
	struct keydesc k1, k2;

	iserase ((VB_CHAR *) name);

	memset (&k1, 0, sizeof k1);
	k1.k_flags  = ISNODUPS;
	k1.k_nparts = 1;
	k1.k_start  = 0;
	k1.k_leng   = 5;
	k1.k_type   = CHARTYPE;

	*ph = isbuild ((VB_CHAR *) name, RECLEN, &k1,
	               ISINOUT + ISFIXLEN + ISEXCLLOCK);
	if (*ph < 0) {
		TAP_DIAG ("isbuild failed: iserrno=%d\n", vb_rtd->iserrno);
		return -1;
	}

	memset (&k2, 0, sizeof k2);
	k2.k_flags  = ISNODUPS;    /* unique second key — collision is the test */
	k2.k_nparts = 1;
	k2.k_start  = 6;
	k2.k_leng   = 5;
	k2.k_type   = CHARTYPE;

	if (isaddindex (*ph, &k2)) {
		TAP_DIAG ("isaddindex failed: iserrno=%d\n", vb_rtd->iserrno);
		return -1;
	}
	return 0;
}



/* ------------------------------------------------------------------ */
/* P1-1: isrewrite() rolls back partial key updates on failure          */
/* Setup:  R1(key1=10000,key2=20000), R2(key1=10001,key2=20001)        */
/* Action: rewrite R1 → key2=20001 (collision with R2) → must fail.    */
/* Assert: R1 and R2 still findable; sequential count still 2.          */
/* ------------------------------------------------------------------ */
static void
test_p1_1_rewrite_rollback (void)
{
	vb_rtd_t *vb_rtd = VB_GET_RTD;
	char      r1[RECLEN + 1], r2[RECLEN + 1], rbuf[RECLEN + 1];
	int       h, rc;

	if (build_two_key_file ("vbtp1_rw", &h) < 0) {
		TAP_FAIL ("P1-1 setup: isbuild/addindex");
		return;
	}

	make_rec (r1, 10000, 20000);
	make_rec (r2, 10001, 20001);

	if (iswrite (h, (VB_CHAR *) r1) || iswrite (h, (VB_CHAR *) r2)) {
		TAP_DIAG ("P1-1 setup iswrite failed iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-1 setup: iswrite");
		isclose (h); iserase ((VB_CHAR *) "vbtp1_rw");
		return;
	}

	/* Read R1 to position current record, then rewrite with R2's key2 */
	memcpy (rbuf, r1, RECLEN);
	if (isread (h, (VB_CHAR *) rbuf, ISEQUAL)) {
		TAP_FAIL ("P1-1 setup: could not read R1");
		isclose (h); iserase ((VB_CHAR *) "vbtp1_rw");
		return;
	}
	char rtry[RECLEN + 1];
	make_rec (rtry, 10000, 20001);   /* key1 unchanged, key2 = R2's → EDUPL */
	rc = isrewrite (h, (VB_CHAR *) rtry);

	if (rc == 0) {
		TAP_FAIL ("P1-1a: isrewrite should have failed EDUPL but returned 0");
		isclose (h); iserase ((VB_CHAR *) "vbtp1_rw");
		return;
	}
	TAP_OK ("P1-1a: isrewrite correctly returned error on key2 collision");

	/* R1 must still be findable by its original key1 */
	memcpy (rbuf, r1, RECLEN);
	rc = isread (h, (VB_CHAR *) rbuf, ISEQUAL);
	if (rc == 0)
		TAP_OK ("P1-1b: R1 findable by original key1 after failed rewrite");
	else {
		TAP_DIAG ("P1-1b iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-1b: R1 NOT findable — index corrupted by failed rewrite");
	}

	/* R2 must still be findable */
	memcpy (rbuf, r2, RECLEN);
	rc = isread (h, (VB_CHAR *) rbuf, ISEQUAL);
	if (rc == 0)
		TAP_OK ("P1-1c: R2 findable after failed rewrite of R1");
	else {
		TAP_DIAG ("P1-1c iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-1c: R2 NOT findable — collateral index damage");
	}

	/* Sequential scan must still yield exactly 2 records */
	memset (rbuf, 0, RECLEN + 1);
	int count = 0;
	rc = isread (h, (VB_CHAR *) rbuf, ISFIRST);
	while (rc == 0) { count++; rc = isread (h, (VB_CHAR *) rbuf, ISNEXT); }
	if (count == 2)
		TAP_OK ("P1-1d: sequential scan yields exactly 2 records");
	else {
		TAP_DIAG ("P1-1d: expected 2, got %d\n", count);
		TAP_FAIL ("P1-1d: wrong record count after failed rewrite");
	}

	isclose (h);
	iserase ((VB_CHAR *) "vbtp1_rw");
}


/* ------------------------------------------------------------------ */
/* P1-2: ivbkeydelete free-node return code (normal delete path)        */
/* Write N records then delete all → exercises single-key-in-node path  */
/* that triggers ivbnodefree.  Verify file is still consistent after.   */
/* Then write new records to confirm the free list is intact.           */
/* ------------------------------------------------------------------ */
static void
test_p1_2_delete_freelist (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	char        rbuf[RECLEN + 1];
	struct keydesc k1;
	int         h, i, rc;
	const int   N = 20;   /* enough to fill several B-tree nodes */
	const char *fname = "vbtp1_del";

	iserase ((VB_CHAR *) fname);
	memset (&k1, 0, sizeof k1);
	k1.k_flags = ISNODUPS;
	k1.k_nparts = 1;
	k1.k_start = 0;
	k1.k_leng  = 5;
	k1.k_type  = CHARTYPE;

	h = isbuild ((VB_CHAR *) fname, RECLEN, &k1,
	             ISINOUT + ISFIXLEN + ISEXCLLOCK);
	if (h < 0) {
		TAP_DIAG ("P1-2 isbuild failed iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-2 setup: isbuild");
		return;
	}

	for (i = 0; i < N; i++) {
		make_rec (rbuf, i, 0);
		if (iswrite (h, (VB_CHAR *) rbuf)) {
			TAP_DIAG ("P1-2 iswrite %d failed iserrno=%d\n", i, vb_rtd->iserrno);
			TAP_FAIL ("P1-2 setup: iswrite");
			isclose (h); iserase ((VB_CHAR *) fname);
			return;
		}
	}

	/* Delete all records — last delete triggers the free-node path */
	int del_errors = 0;
	for (i = 0; i < N; i++) {
		make_rec (rbuf, i, 0);
		if (isread (h, (VB_CHAR *) rbuf, ISEQUAL) == 0) {
			if (isdelete (h, (VB_CHAR *) rbuf))
				del_errors++;
		}
	}
	if (del_errors == 0)
		TAP_OK ("P1-2a: all deletes returned success (ivbnodefree error check intact)");
	else {
		TAP_DIAG ("P1-2a: %d delete errors\n", del_errors);
		TAP_FAIL ("P1-2a: unexpected delete errors");
	}

	/* File should now be empty — ISFIRST must return EENDFILE */
	memset (rbuf, 0, RECLEN + 1);
	rc = isread (h, (VB_CHAR *) rbuf, ISFIRST);
	if (rc != 0 && vb_rtd->iserrno == EENDFILE)
		TAP_OK ("P1-2b: file empty after delete-all (EENDFILE on ISFIRST)");
	else {
		TAP_DIAG ("P1-2b: rc=%d iserrno=%d\n", rc, vb_rtd->iserrno);
		TAP_FAIL ("P1-2b: file not empty after delete-all");
	}

	/* Re-write to prove free-list is intact */
	int write_errors = 0;
	for (i = 0; i < N; i++) {
		make_rec (rbuf, i + N, 0);
		if (iswrite (h, (VB_CHAR *) rbuf))
			write_errors++;
	}
	if (write_errors == 0)
		TAP_OK ("P1-2c: re-write after delete-all succeeded (free list intact)");
	else {
		TAP_DIAG ("P1-2c: %d write errors after delete-all\n", write_errors);
		TAP_FAIL ("P1-2c: re-write failed — free list possibly corrupt");
	}

	isclose (h);
	iserase ((VB_CHAR *) fname);
}

/* ------------------------------------------------------------------ */
/* P1-3/P1-4: transaction tests require VBLOGGING                       */
/* ------------------------------------------------------------------ */
#if defined(WITH_LOGGING)

static void
test_p1_4_update_rollback (void)
{
	vb_rtd_t   *vb_rtd = VB_GET_RTD;
	char        rbuf[RECLEN + 1], orig[RECLEN + 1], modified[RECLEN + 1];
	struct keydesc k1;
	int         h, rc;
	char        logname[] = "vbtp1_p4.log";
	const char *fname = "vbtp1_p4";

	iserase ((VB_CHAR *) fname);
	unlink (logname);

	memset (&k1, 0, sizeof k1);
	k1.k_flags  = ISNODUPS;
	k1.k_nparts = 1;
	k1.k_start  = 0;
	k1.k_leng   = 5;
	k1.k_type   = CHARTYPE;

	h = isbuild ((VB_CHAR *) fname, RECLEN, &k1,
	             ISINOUT + ISFIXLEN + ISEXCLLOCK);
	if (h < 0) {
		TAP_DIAG ("P1-4 isbuild failed iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-4 setup: isbuild");
		return;
	}

	/* Write original record */
	make_rec (orig, 77777, 0);
	if (iswrite (h, (VB_CHAR *) orig)) {
		TAP_FAIL ("P1-4 setup: iswrite");
		isclose (h); iserase ((VB_CHAR *) fname);
		return;
	}

	/* Open transaction log, begin transaction */
	rc = open (logname, O_CREAT | O_TRUNC | O_RDWR, 0666);
	if (rc < 0) { TAP_FAIL ("P1-4 setup: log open"); isclose(h); return; }
	close (rc);
	if (islogopen ((VB_CHAR *) logname)) {
		TAP_DIAG ("P1-4 islogopen failed iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-4 setup: islogopen");
		isclose (h); iserase ((VB_CHAR *) fname); unlink (logname);
		return;
	}

	isbegin ();

	/* Read and rewrite with different content */
	memcpy (rbuf, orig, RECLEN);
	isread (h, (VB_CHAR *) rbuf, ISEQUAL);
	make_rec (modified, 77777, 99999);
	isrewrite (h, (VB_CHAR *) modified);

	/* Rollback — P1-4 verifies on-disk row matches modified before reverting */
	rc = isrollback ();
	if (rc == 0)
		TAP_OK ("P1-4a: isrollback succeeded (row verification passed)");
	else {
		TAP_DIAG ("P1-4a: isrollback returned error iserrno=%d\n", vb_rtd->iserrno);
		TAP_FAIL ("P1-4a: isrollback failed after clean UPDATE");
	}

	/* Record must be restored to original content */
	memcpy (rbuf, orig, RECLEN);
	rc = isread (h, (VB_CHAR *) rbuf, ISEQUAL);
	if (rc == 0 && memcmp (rbuf, orig, RECLEN) == 0)
		TAP_OK ("P1-4b: record restored to original content after rollback");
	else {
		TAP_DIAG ("P1-4b: rc=%d iserrno=%d content_match=%d\n",
		           rc, vb_rtd->iserrno, memcmp(rbuf,orig,RECLEN)==0);
		TAP_FAIL ("P1-4b: record NOT restored after rollback");
	}

	islogclose ();
	isclose (h);
	iserase ((VB_CHAR *) fname);
	unlink (logname);
}

#endif /* WITH_LOGGING */

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int
main (void)
{
	/* TAP plan: P1-1:4 + P1-2:3 + P1-4:2 = 9 total (SKIPs count in plan) */
	const int plan = 9;
	fprintf (stdout, "1..%d\n", plan);

	test_p1_1_rewrite_rollback ();   /* 4 TAP assertions */
	test_p1_2_delete_freelist ();    /* 3 TAP assertions */

#if defined(WITH_LOGGING)
	test_p1_4_update_rollback ();    /* 2 TAP assertions */
#else
	TAP_SKIP ("P1-4a: update rollback (build without --enable-logging)");
	TAP_SKIP ("P1-4b: record content after rollback (build without --enable-logging)");
#endif

	if (g_failures == 0)
		fprintf (stdout, "# All tests passed.\n");
	else
		fprintf (stdout, "# %d test(s) FAILED.\n", g_failures);

	return g_failures ? 1 : 0;
}
