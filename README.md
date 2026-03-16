# VBISAM — Maintained Fork

> C-ISAM compatible ISAM library for GnuCOBOL on modern Linux.  
> Upstream (SourceForge) abandoned 2013. This fork is actively maintained.

## What This Is

VBISAM is a C-ISAM API-compatible indexed sequential access method (ISAM) library.
It provides the same record-level locking semantics that Micro Focus COBOL used on
SCO OpenServer and other System V platforms — using **System V IPC semaphores** for
`LOCK MODE AUTOMATIC`, `LOCK MODE MANUAL`, and `SHARING` clause enforcement.

GnuCOBOL applications built with `--with-vbisam` use native COBOL file I/O verbs
(`READ`, `WRITE`, `REWRITE`, `DELETE`, `START`) against VBISAM-managed `.dat`/`.idx`
files — the same file format as C-ISAM. Code that ran on SCO in 1982 runs today.

## Why This Fork Exists

The SourceForge upstream (`https://sourceforge.net/projects/vbisam/`) has not been
updated since **2013-03-06**. The codebase is architecturally sound but accumulates
compiler compatibility issues as GCC evolves.

This fork:
- Fixes GCC 13+ compatibility (Debian Trixie, Ubuntu 24.04, RHEL 9)
- Documents and fixes known bugs left by the original authors
- Maintains C-ISAM file format compatibility (no format breaks)
- Maintains C-ISAM API compatibility (drop-in replacement)

## Status

| Branch | Purpose |
|---|---|
| `main` | Stable — GCC 13 compat patches only, API/format unchanged |
| `gcc13-compat` | GCC 13 patch history (merged to main) |
| `enhancements` | Active development — bug fixes and new features |

## Building

```bash
sudo apt install build-essential autoconf automake libtool

# Standard build — works on GCC 13+ without any extra flags
autoreconf -fi          # only needed on a fresh clone
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig
```

## GnuCOBOL Integration

```bash
# Configure GnuCOBOL to use VBISAM instead of Berkeley DB:
./configure --prefix=/opt/gnucobol --with-vbisam [other options]

# Verify:
cobcrun --info | grep indexed
# indexed file handler : VBISAM
```

## GCC 13 Patches Applied (in `main`)

**1. `off_t` → `vbisam_off_t` throughout `libvbisam/`**  
GCC 13 treats `off_t` (long) and `vbisam_off_t` (long long) as incompatible pointer
types and makes this a hard error. On 64-bit Linux both are 64-bit — the change is
type-alias only, no behavior difference.

**2. `AM_CFLAGS` in `Makefile.am`**  
C89-era implicit declarations are hard errors in GCC 13. Added:
```
-Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-pointer-types
```

## Known Issues / Enhancement Roadmap

See [`ENHANCEMENTS.md`](ENHANCEMENTS.md) for the full list. Priority items:

- [ ] **`isrewrite.c`** — REWRITE error path leaves file in indeterminate state (data loss risk)
- [ ] **`vbkeysio.c`** — B-tree key I/O ignores return codes (silent corruption possible)
- [ ] **`istrans.c`** — ROLLBACK unlocks all locks, not just ours (can unlock other processes)
- [ ] **`isaudit.c`** — Audit trail is a complete stub (`/* BUG - Write isaudit */`)
- [ ] **64-bit file offsets** — current 32-bit addressing caps files at 4GB
- [ ] **`VB_MAX_FILES`** — lift from 128 to 1024
- [ ] **NFS locking** — SysV semaphores don't cross NFS; optional lock server mode

## Compatibility

| Platform | GCC | Status |
|---|---|---|
| Debian Trixie 13 | GCC 13.3 | ✅ Tested |
| Ubuntu 24.04 LTS | GCC 13.2 | ✅ Expected (same patches) |
| Debian Bookworm 12 | GCC 12 | ✅ Upstream code worked |
| RHEL 9 / Rocky 9 | GCC 11 | ✅ Expected |

## File Format

VBISAM uses the original C-ISAM file format:
- `filename.dat` — data records
- `filename.idx` — B-tree index

Files created by C-ISAM, VBISAM 2.0, and this fork are interchangeable.
Format version 2 (64-bit offsets) is planned for `enhancements` branch — will
be opt-in with full backward read compatibility.

## License

GPL v2 (`COPYING`) / LGPL v2 (`COPYING.LIB`)  
Original authors: Trevor van Bremen and contributors  
This fork maintained by: pmcnary / mcnary-dev

