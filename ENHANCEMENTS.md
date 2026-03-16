# VBISAM Enhancement Roadmap

Issues documented by the original authors (marked `/* BUG */` in source) plus
structural limits and missing features identified during the 2026 GCC 13 port.

---

## Phase 1 — Correctness Fixes (No API/Format Change)

### P1-1: `isrewrite.c` — REWRITE Error Rollback (DATA LOSS RISK)

**Files:** `libvbisam/isrewrite.c` lines 96, 129  
**Original comment:** `/* BUG - We need to do SOMETHING sane here */`  
             `/* BUG - This is WRONG, should re-establish what we had before */`

On a REWRITE failure (disk full, lock timeout, etc.) the function returns an error
but does not restore the original record. The file is left in an indeterminate state.
The fix is to save the original record before the REWRITE attempt and restore it on
any error path — the same pattern the transaction log already uses for BEGIN/ROLLBACK.

**Risk:** High — silent data corruption on REWRITE failure in multi-user environments.

---

### P1-2: `vbkeysio.c` — Unchecked B-tree Return Codes (SILENT CORRUPTION)

**Files:** `libvbisam/vbkeysio.c` lines 821, 869, 873, 892  
**Original comment:** `/* BUG didn't check iresult */`

The B-tree key I/O functions ignore the return value of node read/write operations.
A failed node write (disk full, I/O error) is silently swallowed — the B-tree index
becomes inconsistent with the data file. Next `ischeck` run will show errors.

**Fix:** Check return code after each node I/O call; propagate error to caller.

---

### P1-3: `istrans.c` — ROLLBACK Unlocks All Locks, Not Just Ours

**Files:** `libvbisam/istrans.c` line 184  
**Original comment:** `/* BUG Only ours? */`

`isrollback()` calls `VBUNLOCK` globally, releasing ALL record locks held by the
process — including locks acquired outside the current transaction. In a COBOL program
that mixes transactional and non-transactional I/O, a ROLLBACK can release locks the
program still needs, exposing those records to other processes.

**Fix:** Track which locks were acquired within the current `isbegin()` scope and
only release those on ROLLBACK. Use the existing lock ownership structure.

---

### P1-4: `istrans.c` — ROLLBACK Does Not Verify Row Before Revert

**Files:** `libvbisam/istrans.c` lines 282, 293  
**Original comment:** `/* BUG? Should we READ the row first? */`

Transaction ROLLBACK reverts a record to its pre-transaction state by writing the
saved image back unconditionally. If another process modified the record between
BEGIN and ROLLBACK (which should not happen if locking is correct, but can happen
given P1-3 above), the revert will silently overwrite the other process's changes.

**Fix:** READ the current row first; if it doesn't match the transaction log's
before-image checksum, raise `ERECDIFERR` instead of blindly overwriting.

---

### P1-5: `isaudit.c` — Audit Trail Complete Stub

**Files:** `libvbisam/isaudit.c`  
**Original comment:** `/* BUG - Write isaudit */`

The entire `isaudit()` function is a stub that returns immediately. The API is
defined and the mode constants exist (`AUDSETNAME`, `AUDSTART`, `AUDSTOP`, `AUDINFO`)
but no implementation was ever written.

**Implementation plan:**
- `AUDSETNAME`: set audit log filename (default: `filename.aud`)
- `AUDSTART`: open audit log in append mode, write session header
- `AUDSTOP`: flush and close audit log
- Each INSERT/UPDATE/DELETE: append timestamped record with before/after image
- Format: fixed-length binary (fast) with optional JSON mode (human-readable)

Essential for medical, financial, and any regulated application.

---

## Phase 2 — Structural Limits

### P2-1: 64-bit File Offsets (V2 Format)

**Current limit:** 32-bit internal node addressing → 4GB max per `.dat` file  
**Target:** 64-bit offsets → 16 exabyte theoretical max

**Approach:**
- Add format version byte to file header (`VBISAM_FMT_V1` / `VBISAM_FMT_V2`)
- V1 files: read/write as today (full backward compatibility)
- V2 files: use `vbisam_off_t` (already 64-bit after GCC 13 patch) throughout node I/O
- New files created as V2 by default (configurable)
- `vbisam-convert` utility to upgrade V1 → V2 in-place

**Note:** The GCC 13 `off_t` → `vbisam_off_t` patch already makes the C types 64-bit.
The remaining work is the on-disk format and the node I/O layer.

---

### P2-2: Lift `VB_MAX_FILES` from 128 → 1024

**Files:** `libvbisam/isinternal.h`  
Simple constant change. The per-file control structure (`struct VBFILE`) is heap-
allocated so there is no stack impact. The SysV semaphore set size may need review
on some platforms.

---

## Phase 3 — New Capability

### P3-1: NFS-Safe Locking Mode

SysV IPC semaphores are local to a single host. VBISAM cannot be used with NFS-
mounted data directories in multi-user mode.

**Option A:** Optional lock server daemon (Unix socket, same host or remote)  
**Option B:** SQLite-based lock coordinator file alongside the `.dat` file  
**Option C:** Document limitation clearly; recommend PostgreSQL for NFS environments

---

### P3-2: `vbisam-fsck` Utility

`ischeck.c` has known VARLEN handling bugs (lines 253-254). A standalone utility:
- Verify B-tree structure integrity
- Detect orphaned data records
- Detect index/data mismatches
- Repair mode: rebuild index from data file
- Dump mode: export records to CSV or JSON for data rescue

---

### P3-3: UTF-8 / Locale-Aware Key Collation

Keys are currently compared byte-by-byte. For character keys containing accented
characters, sort order is wrong for any locale other than C/POSIX. Add an optional
`ISDCHAR` key type (distinct from `ISCHAR`) that uses `strcoll()` for comparison.

---

### P3-4: GixSQL Bridge

Allow `EXEC SQL SELECT` and native COBOL `READ` against the same VBISAM file in the
same program — exposing VBISAM files as virtual SQL tables via a GixSQL driver. Pure
read-only SQL access to existing ISAM files without migration.

---

## Compatibility Guarantee

All Phase 1 and Phase 2-2 changes: **zero API change, zero V1 format change.**  
Phase 2-1 (64-bit): **new V2 format, V1 fully readable, opt-in for new files.**  
Phase 3: **all opt-in via configure flags or runtime options.**

