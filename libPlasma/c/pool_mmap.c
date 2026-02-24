
/* (c)  oblong industries */

///
/// Mmap-type pool implementation
///

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-atomic.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include "libLoam/c/ob-time.h"
#include "libLoam/c/ob-hash.h"

#include "libPlasma/c/pool.h"
#include "libPlasma/c/pool_options.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/pool_mmap.h"
#include "libPlasma/c/private/pool_multi.h"
#include "libPlasma/c/private/pool-toc.h"

#include "libPlasma/c/slaw-coerce.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/private/plasma-private.h"

// XXX: harmonize terminology: "offset" versus "entry".

/// The mmap pool type uses an mmap()d file as the backing store for
/// proteins.  All access to the pool is through simple memory reads
/// and writes.  When the pool is full, proteins wrap around to the
/// beginning of the file and overwrite older proteins.
///
/// Each protein, as deposited in the pool, is part of an entry.  An
/// entry includes the protein and various ancillary data - timestamp,
/// etc.  It is larger than the protein, and when navigating about the
/// pool, you want to think in terms of entries and not proteins.
///
/// The index indicates where the protein falls in the series of
/// protein deposits - that is, the 27th protein deposited into the
/// pool has an index of 26 (programmers count starting from 0).  The
/// index does not give you any information about the exact location of
/// a protein in the pool, only its location relative to other
/// proteins.
///
/// In regard to proteins, entries, and indices, "first" and "last"
/// are different from "oldest" and "newest".  Oldest and newest refer
/// to the order in which proteins are deposited into the pool.  First
/// and last refer to the order of the proteins as laid out in the
/// backing file.

/// What is the format of the backing file?
///
/// -----------------------------------
/// | pool header                     | <= beginning of file
/// ...                               |
/// |                                 |
/// -----------------------------------
/// | unused space (in rare cases)    |
/// -----------------------------------
/// | protein entries                 |
/// ...                               |
/// |                                 |
/// -----------------------------------
/// | oldest protein entry            | <= oldest offset points here
/// -----------------------------------
/// | protein entries                 |
/// ...                               |
/// |                                 |
/// -----------------------------------
/// | newest protein entry            |
/// -----------------------------------
/// |                                 | <= write offset points here
/// ...                               |
/// | end of protein entry N          |
/// -----------------------------------
/// | unused space (fragmented)       |
/// ----------------------------------- <= end of file
///
/// Normally there is not any unused space between the header and
/// the protein entries.  The only case in which this can occur
/// is when pool_advance_oldest() is used to erase old proteins
/// at the beginning of the file.  (And when this happens, the
/// pool is not wrapped.  If the pool is wrapped, there is always
/// a protein right after the header.)
///
/// What does the format of a protein entry in a pool look like?
///
/// -----------------------
/// | timestamp - 8 bytes | time the protein was deposited
/// -----------------------
/// | index - 8 bytes     | index of this protein
/// -----------------------
/// | protein - n bytes   | actual protein - first 8 bytes are length
/// ...
/// |                     |
/// -----------------------
/// | jumpback - 8 bytes  | how far back the beginning of this entry is
/// -----------------------
///
/// The jumpback is useful for navigating the pool backwards.  When
/// going forward, we use the length of the protein (plus the length
/// of the other fields in the entry) to figure out where the next
/// protein starts.
///
/// The proteins themselves are in machine independent byte order,
/// but everything else (such as offsets, timestamps, jumpback, etc.)
/// is in native byte order.
///
/// The whole pool backing file is, of course, mmap'd into the
/// process's address space, and the address it has been mapped to is
/// kept in the d->mem member of the mmap private data.  The d->header
/// member - the pool header struct - is simply a pointer to the
/// beginning of the map; when you write to its fields, you are
/// writing to the mmap file.
///
/// Entry offsets are kept in the form of 64-bit offsets from the
/// "beginning" of the pool, as if the pool was infinite in size (when
/// really it wraps around and obliterates older entries).  The
/// offsets are converted to actual usable memory addresses in the
/// entry read/write functions.  You can't use them raw to access
/// memory.  Pool entry offsets are basically a virtual address space,
/// and the entry read/write functions do the virtual -> "physical"
/// address translation.  The base address for the virt -> phys
/// translation is the d->mem address - not the beginning of the
/// proteins in the pool.

/// See file versions.txt for more about versioning.
///
/// Version 0 pools have a header like this:
///
/// ----------------------------------- <= beginning of file
/// | oldest protein offset - 8 bytes |
/// -----------------------------------
/// | newest protein offset - 8 bytes |
/// -----------------------------------
/// | magic number and vers - 8 bytes | <= absent in really old (pre-0) pools
/// -----------------------------------
/// | table of contents               | <= optional
/// ...                               |
/// |                                 |
/// -----------------------------------
///
/// pre-0 pools are a variant of version 0 pools, which probably don't
/// exist anywhere anymore except on Mignon's computer.  pre-0 pools
/// have exactly 16 bytes of header.  (In version 0 pools, the header
/// length is stored in the mmap.conf file; if it is missing, it is
/// assumed to be 16 and therefore a pre-0 pool.)  In normal version 0
/// pools, the header length is at least 24 bytes, and is more if there
/// is an index.
///
/// Version 1 pools have a header like this:
///
/// ----------------------------------- <= beginning of file
/// | magic number and vers - 8 bytes |
/// -----------------------------------
/// | conf chunk                      |
/// -----------------------------------
/// | other chunks                    | <= optional
/// ...                               |
/// |                                 |
/// -----------------------------------
///
/// The first oct of the version 1 backing file uses the same format
/// as slaw binary files (see comment at top of slaw-io.c), but with
/// a different value for "type".  Although not currently exploited,
/// this paves the way for a future capability that can accept an
/// arbitrary "plasma file" and auto-detect whether it is an mmap pool
/// or a binary slaw file.  (The main differences between the two are
/// that mmap pools are random-access, haves indexes and timestamps,
/// and only contain proteins, while binary slaw files are sequential
/// access, may contain any type of slaw, and don't have indexes or
/// timestamps.  But otherwise they have a lot in common.)
///
/// After the first oct are a sequence of "chunks".  Each chunk
/// has the following format:
///
/// -----------------------------------
/// | chunk signature       - 8 bytes |
/// -----------------------------------
/// | chunk length          - 8 bytes |
/// -----------------------------------
/// | chunk payload                   |
/// ...                               |
/// |                                 |
/// -----------------------------------
///
/// The signature contains 32 invariant bits (same for all chunks),
/// and 32 bits indicating the type of chunk.  See private/pool_mmap.h
/// for more details.
///
/// Most chunks are optional, and can come in any order.  Currently the
/// "conf" chunk and "ptrs" chunk are required for all pools, and the
/// "conf" chunk must be the first chunk.  (But all other chunks, including
/// the "ptrs" chunk, may be in any order after that.)  Optional chunks
/// are the "perm" chunk and the "indx" chunk, and others may be added
/// later.
///
/// The "conf" chunk contains information that used to (in version 0)
/// be contained in the mmap.conf file.  The "ptrs" chunk contains the
/// oldest and newest protein offsets, which used to just be right at
/// the beginning of the header in version 0 pools.

/// Locking strategy
///
/// Our goal is that readers don't interfere with depositors, and that
/// many reads can progress in parallel.  Thus, we have one depositor
/// lock, and lock-free reads.  As you might imagine, this makes reads
/// an enormous pain, since the data they are reading might change at
/// any moment.
///
/// The theory behind reads is that we check to see if the data we
/// want to read is valid before we start the read, do the read, then
/// check if the data is still valid.  We have to do before we use the
/// data we read.  We call data that has been overwritten "stompled."
///
/// We determine whether data is still good by comparing the entry
/// address of the desired data with the current recorded oldest and
/// newest entries.  If it is between the oldest and newest entries,
/// then it is valid, otherwise it has been stompled.  Likewise with
/// indexes.
///
/// The indexes of the oldest, newest, and first entries must be
/// derived from entries to avoid a race condition between setting the
/// entry and the index in which readers get the wrong entry or index.
/// Also, in the read path, indexes derived from entries may have been
/// overwritten, so the caller must check to see if the entry is still
/// valid after calculating an index.

/* If an index is larger than this number, we're going to assume something
 * very bad has happened, and the pool is corrupt.  At the very least,
 * we must make sure the index is less than 9223372036854775808, because
 * otherwise the unsigned index will become negative when we cast it to
 * a signed value (which, perhaps unfortunately, we do) which basically
 * causes a horrible infinite loop, as I have discovered tonight, while
 * looking at some corrupt pools that were submitted to me.
 *
 * However, since I need to enforce a maximum anyway, I've decided to
 * set it to the (slightly) lower value of 3153600000000000000.
 * If my math is correct (and goodness knows, it often isn't), this
 * is the value the index would be if it was incremented once per
 * nanosecond for a hundred years.  So I think that's a pretty safe
 * upper bound.
 */

#define MAXIMUM_REASONABLE_INDEX OB_CONST_U64 (3153600000000000000)

/// The offset from the beginning of the pool where proteins begin to
/// be written.

#define POOL_MMAP_PROTEINS_START_OFFSET(x) (get_header_size (x))

/// The offset of an entry's timestamp from the beginning of the
/// entry.

#define POOL_MMAP_TIMESTAMP_OFFSET 0

/// The offset of an entry's index from the beginning of the entry.

#define POOL_MMAP_INDEX_OFFSET                                                 \
  POOL_MMAP_TIMESTAMP_OFFSET + sizeof (pool_timestamp)

/* The offset of the checksum (if present) from the beginning of the entry. */

#define POOL_MMAP_CHECKSUM_OFFSET POOL_MMAP_INDEX_OFFSET + sizeof (unt64)

/// The offset of the protein itself from the beginning of a protein entry.

static inline size_t POOL_MMAP_PROTEIN_OFFSET (const pool_mmap_data *d)
{
  const unt64 flags = get_flags (d);
  size_t off = POOL_MMAP_CHECKSUM_OFFSET;
  if (0 != (flags & POOL_FLAG_CHECKSUM))
    off += sizeof (unt64);
  return off;
}

/// The size of the field containing the jumpback length.

#define POOL_MMAP_JUMPBACK_LEN sizeof (unt64)

// The follow line is parsed by the script libPlasma/c/tests/avoid-fallback.rb,
// so deleting it or moving it to another file will cause issues with that
// script.
#define POOL_MMAP_CURRENT_VERSION OB_CONST_U64 (1)

/// The number of proteins below which we don't care if we're being
/// inefficient.  This is a very touchy-feely constant, but 10 seems
/// about right to me, although 100 wouldn't be wrong either.
#define TOO_SMALL_TO_MATTER 10

static inline unt64 primitive_get_oldest_entry (const pool_mmap_data *d)
{
  return (unt64) ob_atomic_int64_ref (&d->oldnew->oldest_entry);
}

static inline void primitive_set_oldest_entry (const pool_mmap_data *d,
                                               unt64 entry)
{
  ob_atomic_int64_set (&d->oldnew->oldest_entry, (int64) entry);
}

static inline unt64 primitive_get_newest_entry (const pool_mmap_data *d)
{
  return (unt64) ob_atomic_int64_ref (&d->oldnew->newest_entry);
}

static inline void primitive_set_newest_entry (const pool_mmap_data *d,
                                               unt64 entry)
{
  ob_atomic_int64_set (&d->oldnew->newest_entry, (int64) entry);
}

/// Collects pool index statistics, and reports on their current status
/// if advisable.

static void collect_index_stats (pool_mmap_data *d, unt64 succ, unt64 fail,
                                 unt64 tries)
{
  if (pool_toc_count (d->ptoc) == 0 || (0 == succ && 0 == fail))
    return;

  d->index_success_count += succ;
  d->index_failure_count += fail;

  unt64 step = pool_toc_step (d->ptoc);
  if (fail > 0 && tries > TOO_SMALL_TO_MATTER && tries > step)
    {
      unt64 failed = 1 + ((tries - TOO_SMALL_TO_MATTER) / step);
      ob_log (OBLV_INFO, 0x20104000,
              "hose '%s' pool '%s': deeply regrettable... "
              "search through %" OB_FMT_64 "u proteins "
              "in a pool index with a step of %" OB_FMT_64 "u "
              "(%" OB_FMT_64 "u failed index lookups)\n",
              hname (d), pname (d), tries, step, failed);
    }
}

static void report_index_stats (pool_mmap_data *d)
{
  static const double ALARM_THRESHOLD = 0.001;

  if (d->index_failure_count == 0)
    return;
  unt64 total = d->index_success_count + d->index_failure_count;
  double rate = (double) d->index_failure_count / total;
  if (rate - d->index_failure_rate > ALARM_THRESHOLD)
    {
      ob_log (OBLV_INFO, 0x20104001,
              "hose '%s' pool '%s' (step: %" OB_FMT_64 "u): "
              "high fail rate: %" OB_FMT_64 "u failed / %" OB_FMT_64
              "u succeeded (%lf rate)\n",
              hname (d), pname (d), pool_toc_step (d->ptoc),
              d->index_failure_count, d->index_success_count, rate);
      d->index_failure_rate = rate;
    }
}

static ob_retort pool_mmap_create_file1 (const char *path, va_list vargs);

static ob_retort pool_mmap_destroy_file1 (const char *path, va_list ignore);

static ob_retort pool_mmap_open_file1 (const char *path, va_list ignore);

/// Create the backing file

static ob_retort pool_mmap_create_file (pool_hose ph, int mode, int uid,
                                        int gid)
{
  return pool_mmap_call_with_backing_file (ph->name, ph->pool_directory_version,
                                           false, pool_mmap_create_file1, ph,
                                           mode, uid, gid);
}

static ob_retort pool_mmap_create_file1 (const char *path, va_list vargs)
{
  pool_hose ph = va_arg (vargs, pool_hose);
  int mode = va_arg (vargs, int);
  int uid = va_arg (vargs, int);
  int gid = va_arg (vargs, int);

  int fd = 0;
  pool_mmap_data *d = pool_mmap_get_data (ph);

#ifdef _MSC_VER

  // Create file
  const errno_t err1 = _sopen_s (&fd, path, O_RDWR | O_CREAT | O_EXCL,
                                 _SH_DENYNO, _S_IREAD | _S_IWRITE);
  if (fd < 0)
    return ob_errno_to_retort (err1);

  // Now extend it to the proper size
  int chsize_result = _chsize (fd, get_file_size (d));
  const errno_t err2 = errno;

  // now close it
  _close (fd);

  //if we couldnt change size, error out
  if (chsize_result != 0)
    {
      _unlink (path);
      return ob_errno_to_retort (err2);
    }

  //ok now we have to open it with createfile
  HANDLE fh =
    CreateFile (path, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
  if (fh == 0)
    {
      const DWORD last = GetLastError ();
      _unlink (path);
      return ob_win32err_to_retort (last);
    }
#else
  // Create file
  int i;
  int erryes = 0;
  /* Ugh, this is ugly code (and yes, I wrote it).  The for loop is
   * because we want to delete the file and retry if we failed to open
   * it.  But, we only do the loop a maxiumum of two times, because we
   * don't want to retry indefinitely if for some reason, deleting
   * doesn't help.  But admittedly, this control path is rather obtuse,
   * and should probably be rewritten.  (Though I'm not quite sure how,
   * which is why for now I'm just commenting it rather than rewriting it.)
   *
   * Furthermore, is there a reason why we open the file with O_EXCL, and
   * then try deleting it, rather than just omitting the O_EXCL?  I have
   * a vague memory that there was some reason for doing it this way,
   * but unfortunately my self from six months ago is not around to ask.
   * So, I'll leave it as-is for now.
   */
  for (i = 0; i < 2; i++)
    {
      fd = ob_open_cloexec (path, O_RDWR | O_CREAT | O_EXCL, 0666);
      erryes = errno;
      if (fd < 0 && erryes == EEXIST)
        // this might be a case like bug 2566, so let's try
        // deleting the old backing file to get unwedged
        unlink (path);
      else
        break;
    }
  if (fd < 0)
    return ob_errno_to_retort (erryes);
  // Advise the user that doing this over NFS really isn't a good idea...
  if (ob_is_network_path (path))
    {
      OB_LOG_ERROR_CODE (0x20104002,
                         "***** %s: CREATING AN MMAP POOL ON AN NFS "
                         "FILESYSTEM IS NOT A GOOD IDEA *****\n",
                         path);
      OB_CHECK_POSIX_CODE (0x2010402c, close (fd));
      OB_CHECK_POSIX_CODE (0x2010402d, unlink (path));
      return POOL_INAPPROPRIATE_FILESYSTEM;
    }
  // permissions, permissions, permissions...
  ob_retort tort = ob_permissionize (path, mode, uid, gid);
  if (tort < OB_OK)
    {
      OB_CHECK_POSIX_CODE (0x2010402e, close (fd));
      OB_CHECK_POSIX_CODE (0x2010402f, unlink (path));
      return tort;
    }
  // Now extend it to the proper size
  if (ftruncate (fd, get_file_size (d)) != 0)
    {
      erryes = errno;
      OB_CHECK_POSIX_CODE (0x20104030, close (fd));
      OB_CHECK_POSIX_CODE (0x20104031, unlink (path));
      return ob_errno_to_retort (erryes);
    }
  // Convert fd to a file handle
  FILE *fh = fdopen (fd, "w+");
  if (!fh)
    {
      erryes = errno;
      OB_CHECK_POSIX_CODE (0x20104032, close (fd));
      OB_CHECK_POSIX_CODE (0x20104033, unlink (path));
      return ob_errno_to_retort (erryes);
    }

#endif

  d->file = fh;
  return OB_OK;
}

/// Destroy the backing file.

static ob_retort pool_mmap_destroy_file (pool_hose ph, bool safely)
{
  return pool_mmap_call_with_backing_file (ph->name, ph->pool_directory_version,
                                           safely, pool_mmap_destroy_file1, ph);
}

static ob_retort pool_mmap_destroy_file1 (const char *path, va_list vargs)
{
  pool_hose ph = va_arg (vargs, pool_hose);
  ob_retort pret = pool_destroy_multi_await (ph);
  ob_err_accum (&pret, pool_destroy_semaphores (ph, true));
  if (unlink (path) != 0)
    {
      int erryes = errno;
      ob_log (OBLV_DBUG, 0x20104003, "unlink failed for pool mmap file %s\n",
              path);
#ifdef _MSC_VER
      if (EACCES == erryes)
        return POOL_IN_USE;
#endif
      return ob_errno_to_retort (erryes);
    }
  return pret;
}

/// Mmap the backing file into our memory.

static ob_retort pool_mmapify (pool_hose ph)
{
  byte *mmem;
  pool_mmap_data *d = pool_mmap_get_data (ph);

#ifdef _MSC_VER
  HANDLE fileMapping = CreateFileMapping (d->file, 0, PAGE_READWRITE, 0, 0, 0);

  if (!fileMapping)
    return POOL_MMAP_BADTH;

  mmem = (byte *) MapViewOfFile (fileMapping, FILE_MAP_ALL_ACCESS, 0, 0,
                                 d->mapped_size);

  if (!mmem)
    return POOL_MMAP_BADTH;

  d->file_mapping_object = fileMapping;

#else
  mmem = (byte *) mmap ((caddr_t) 0, d->mapped_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fileno (d->file), 0);
  if (mmem == MAP_FAILED)
    {
      const int erryes = errno;
      OB_LOG_ERROR_CODE (0x2010402b,
                         "mmap of size %" OB_FMT_64 "u failed because '%s'\n"
                         "%s",
                         d->mapped_size, strerror (erryes),
                         ((sizeof (void *) < 8 && ENOMEM == erryes)
                            ? "(Since you are on a 32-bit machine, virtual\n"
                              "address space is going to be scarce)\n"
                            : ""));
      errno = erryes;
      return POOL_MMAP_BADTH;
    }
#endif

  d->mem = mmem;

  return OB_OK;
}

static ob_retort pool_munmapify (pool_hose ph)
{
  pool_mmap_data *d = pool_mmap_get_data (ph);
  ob_retort pret = OB_OK;
  if (d->mem)
    {
      // save config chunk in legacy chunk, so we still have it after
      // we unmap the backing file
      memmove (&(d->legacy_conf), d->conf_chunk, sizeof (d->legacy_conf));
      d->conf_chunk = &(d->legacy_conf);
    }
#ifdef _MSC_VER
  if (d->mem)
    {
      if (!UnmapViewOfFile (d->mem))
        ob_err_accum (&pret, ob_win32err_to_retort (GetLastError ()));
      d->mem = 0;
    }
  if (d->file_mapping_object)
    {
      if (!CloseHandle (d->file_mapping_object))
        ob_err_accum (&pret, ob_win32err_to_retort (GetLastError ()));
      d->file_mapping_object = 0;
    }
#else
  if (d->mem)
    {
      if (munmap (d->mem, d->mapped_size) < 0)
        ob_err_accum (&pret, ob_errno_to_retort (errno));
      d->mem = NULL;
    }
#endif
  return pret;
}

/// Open the backing file and stick its file handle into our pool hose.

static ob_retort pool_mmap_open_file (pool_hose ph)
{
  return pool_mmap_call_with_backing_file (ph->name, ph->pool_directory_version,
                                           false, pool_mmap_open_file1, ph);
}

static ob_retort pool_mmap_open_file1 (const char *path, va_list vargs)
{
  pool_hose ph = va_arg (vargs, pool_hose);
#ifdef _MSC_VER
  HANDLE fh =
    CreateFile (path, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
  if (fh == INVALID_HANDLE_VALUE)
    {
      const DWORD err = GetLastError ();

      ob_log (OBLV_DBUG, 0x20104005, "Couldn't open '%s'\n", path);
      return ob_win32err_to_retort (err);
    }
#else
  // Advise the user that doing this over NFS really isn't a good idea...
  if (ob_is_network_path (path))
    {
      OB_LOG_ERROR_CODE (0x20104006,
                         "***** %s: USING AN MMAP POOL ON AN NFS FILESYSTEM IS "
                         "NOT A GOOD IDEA *****\n",
                         path);
      return POOL_INAPPROPRIATE_FILESYSTEM;
    }

  FILE *fh = ob_fopen_cloexec (path, "r+");

  if (!fh)
    {
      const int erryes = errno;
      ob_log (OBLV_DBUG, 0x20104007, "Couldn't open '%s' because '%s'\n", path,
              strerror (erryes));
      return ob_errno_to_retort (erryes);
    }

  /* So here's the idea: as long as we have the backing file open
   * (which is more or less synonymous with having the pool open),
   * we keep a non-exclusive lock on the backing file.  This has
   * no effect *except* that it prevents someone from taking an
   * exclusive lock on the file.  So when we want to delete or rename
   * the pool, we try to take an exclusive lock.  If that fails, we
   * know somebody is still using the pool!  The beauty of this is
   * that we never have to explicitly drop the lock-- it will be dropped
   * for us when the file is closed, either because we withdraw from
   * the pool or because our process crashes or is rudely killed.
   */
  while (flock (fileno (fh), LOCK_SH) < 0)
    {
      const int erryes = errno;
      if (EINTR != erryes)
        {
          OB_CHECK_POSIX_CODE (0x2010403a, fclose (fh));
          return ob_errno_to_retort (erryes);
        }
    }
#endif

  pool_mmap_data *d = pool_mmap_get_data (ph);
  d->file = fh;
  return OB_OK;
}

static void redo_mmap (pool_mmap_data *d, unt64 new_size, ob_retort *errp)
{
  if (already_failed (errp))
    return;
  const unt8 mmv = get_mmap_version (d);
  ob_err_accum (errp, pool_munmapify (d->ph));
  if (already_failed (errp))
    return;
  OB_LOG_DEBUG_CODE (0x20104049,
                     "setting mapped_size to 0x%016" OB_FMT_64 "x\n", new_size);
  d->mapped_size = new_size;
  ob_err_accum (errp, pool_mmapify (d->ph));
  if (already_failed (errp))
    return;
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);
  ob_err_accum (errp, f.read_header (d));
  // cached index and entry will be invalid
  d->cached_index = -1;
  OB_INVALIDATE (d->cached_entry);
}

static void check_for_size_change (pool_mmap_data *d, ob_retort *errp)
{
  if (already_failed (errp))
    return;
  unt64 size_now = get_file_size (d);
  if (size_now != d->mapped_size)
    redo_mmap (d, size_now, errp);
}

static void change_file_size (pool_mmap_data *d, unt64 new_size,
                              ob_retort *errp)
{
  if (already_failed (errp))
    return;
#ifdef _MSC_VER
  // because having an API that just takes a plain old 64-bit integer
  // would be too easy...
  LARGE_INTEGER li;
  li.QuadPart = new_size;
  if (!SetFilePointerEx (d->file, li, NULL, FILE_BEGIN))
    {
      *errp = ob_win32err_to_retort (GetLastError ());
      return;
    }
  if (!SetEndOfFile (d->file))
    {
      *errp = ob_win32err_to_retort (GetLastError ());
      return;
    }
#else
  if (ftruncate (fileno (d->file), new_size) < 0)
    {
      *errp = ob_errno_to_retort (errno);
      return;
    }
#endif
  OB_LOG_INFO_CODE (0x2010404a,
                    "resized '%s' backing file to %" OB_FMT_64 "u\n", pname (d),
                    new_size);
}

static unt64 get_real_file_size (pool_mmap_data *d, ob_retort *errp)
{
  if (already_failed (errp))
    return 0;
#ifdef _MSC_VER
  LARGE_INTEGER thanks_bill;
  if (!GetFileSizeEx (d->file, &thanks_bill))
    *errp = ob_win32err_to_retort (GetLastError ());
  return thanks_bill.QuadPart;
#else
  struct stat ss;
  if (fstat (fileno (d->file), &ss) < 0)
    *errp = ob_errno_to_retort (errno);
  return ss.st_size;
#endif
}

static void physically_resize (pool_mmap_data *d, unt64 new_size,
                               ob_retort *errp)
{
  unt64 real_file_size = get_real_file_size (d, errp);
  if (already_failed (errp))
    return;
  if (new_size > real_file_size)
    // If the file is getting larger, do it now.  If it's getting smaller,
    // wait until last_withdraw_housekeeping() to resize it.  This avoids
    // SIGBUS problem when the mapping of the readers is larger than the file.
    change_file_size (d, new_size, errp);
  redo_mmap (d, new_size, errp);
  OB_LOG_DEBUG_CODE (0x20104048, "new_size = 0x%016" OB_FMT_64 "x\n", new_size);
  ob_atomic_int64_set (&d->conf_chunk->file_size, (int64) new_size);
}

static void pool_mmap_sync (const pool_mmap_data *d, ob_retort *errp)
{
  if (already_failed (errp))
    return;

  const unt64 flags = get_flags (d);

  if (0 != (flags & POOL_FLAG_SYNC))
    {
// http://www.humboldt.co.uk/2009/03/fsync-across-platforms.html
#if defined(_MSC_VER)
      if (!FlushFileBuffers (d->file))
        *errp = ob_win32err_to_retort (GetLastError ());
#elif defined(__APPLE__)
      if (fcntl (fileno (d->file), F_FULLFSYNC) == -1)
        *errp = ob_errno_to_retort (errno);
#else
      if (fsync (fileno (d->file)) < 0)
        *errp = ob_errno_to_retort (errno);
#endif
    }
}

/// Is this pool empty now and for the forseeable future?  This is
/// different from when the pool is temporarily empty during a deposit
/// that is so large that it overwrites every valid protein in the
/// pool, in which case the pool will very shortly be non-empty.

static bool is_pool_empty (const pool_mmap_data *d)
{
  return (0 == primitive_get_newest_entry (d));
}

/// Is the pool temporarily empty?  This happens when a protein is
/// added to the pool that is big enough to overwrite all existing
/// proteins.  When this happens, the newest entry is less than the
/// oldest entry.  Can't be called on a truly empty pool (one in which
/// no protein deposit is ongoing).
///
/// Since this gets the oldest and newest entry, it optionally can
/// store them so the caller doesn't have to read them again.

static bool is_pool_temporarily_empty (const pool_mmap_data *d, unt64 *poldest,
                                       unt64 *pnewest)
{
  unt64 oldest_entry = primitive_get_oldest_entry (d);
  unt64 newest_entry = primitive_get_newest_entry (d);
  if (poldest)
    *poldest = oldest_entry;
  if (pnewest)
    *pnewest = newest_entry;
  return (newest_entry < oldest_entry);
}

/// How much total space is there for writing entries?

static unt64 pool_entry_space (const pool_mmap_data *d)
{
  return d->mapped_size - POOL_MMAP_PROTEINS_START_OFFSET (d);
}

/// Useful debugging and sanity checks.

/// Sanity check the difference between the oldest and newest entry
/// address.  No matter which entry is greater, newest and oldest
/// should be no more than pool size apart - as long as we hold the
/// lock.  If we're the reader, the depositors can change them
/// underneath us by arbitrary amounts.  So don't call this in the
/// read path.

static void check_oldest_newest_diff (const pool_mmap_data *d, ob_retort *errp)
{
  if (already_failed (errp))
    return;
  unt64 oldest_entry = primitive_get_oldest_entry (d);
  unt64 newest_entry = primitive_get_newest_entry (d);
  int64 diff = newest_entry - oldest_entry;
  diff = (diff >= 0) ? diff : -diff;
  if (diff > d->mapped_size)
    {
      OB_LOG_ERROR_CODE (
        0x20104008,
        "hose '%s' pool '%s': "
        "Difference between newest (%" OB_FMT_64 "u) and oldest (%" OB_FMT_64
        "u) greater than pool size (%" OB_FMT_64 "u > %" OB_FMT_64 "u)\n",
        hname (d), pname (d), newest_entry, oldest_entry, diff, d->mapped_size);
      *errp = POOL_CORRUPT;
    }
}

/// The addresses of the oldest and newest entries should always
/// increase.

static void check_oldest_forward_progress (const pool_mmap_data *d,
                                           unt64 oldest_entry, ob_retort *errp)
{
  if (already_failed (errp))
    return;
  unt64 holdest_entry = primitive_get_oldest_entry (d);
  if (oldest_entry <= holdest_entry)
    {
      OB_LOG_ERROR_CODE (0x20104009,
                         "hose '%s' pool '%s': "
                         "New oldest entry (%" OB_FMT_64
                         "u) <= old oldest entry (%" OB_FMT_64 "u)\n",
                         hname (d), pname (d), oldest_entry, holdest_entry);
      *errp = POOL_CORRUPT;
    }
}

static void check_newest_forward_progress (const pool_mmap_data *d,
                                           unt64 newest_entry, ob_retort *errp)
{
  if (already_failed (errp))
    return;
  unt64 hnewest_entry = primitive_get_newest_entry (d);
  if (newest_entry <= hnewest_entry)
    {
      OB_LOG_ERROR_CODE (0x2010400a,
                         "hose '%s' pool '%s': "
                         "New newest entry (%" OB_FMT_64
                         "u) <= new newest entry (%" OB_FMT_64 "u)\n",
                         hname (d), pname (d), newest_entry, hnewest_entry);
      *errp = POOL_CORRUPT;
    }
}

/// The address of the oldest entry should be less than that of the
/// newest.

static void check_oldest_less_than_newest (const pool_mmap_data *d,
                                           ob_retort *errp)
{
  if (already_failed (errp))
    return;
  unt64 oldest_entry = primitive_get_oldest_entry (d);
  unt64 newest_entry = primitive_get_newest_entry (d);
  if (oldest_entry > newest_entry)
    {
      OB_LOG_ERROR_CODE (0x2010400b,
                         "hose '%s' pool '%s': "
                         "oldest %" OB_FMT_64 "u newest %" OB_FMT_64 "u\n",
                         hname (d), pname (d), oldest_entry, newest_entry);
      *errp = POOL_CORRUPT;
    }
}

/// The oldest/newest/first entry functions merely have to return an
/// offset that was valid at some point in the past, and the caller
/// must check if it was stompled and retry.  This lets us return
/// known-stompled entries and push the retry into the caller.
///
/// When the pool is temporarily empty, we have set the oldest entry
/// to a place ahead of the newest entry (oldest > newest).  That
/// means newest has been stompled and is safe to return.  So for this
/// case, return the following:
///
/// oldest entry: newest entry (stompled)
/// newest entry: newest entry (stompled)
/// first entry: first entry (<= newest entry and therefore also stompled)
///
/// Note that all get_*_entry() functions cannot be called on a truly
/// empty pool.

/// Get the oldest entry.  Readers must check for stompledness.

static unt64 get_oldest_entry (const pool_mmap_data *d)
{
  unt64 oldest_entry, newest_entry;
  if (is_pool_temporarily_empty (d, &oldest_entry, &newest_entry))
    return newest_entry;
  else
    return oldest_entry;
}

/// Set the oldest entry.

static void set_oldest_entry (pool_mmap_data *d, unt64 oldest_entry,
                              ob_retort *errp)
{
  check_oldest_forward_progress (d, oldest_entry, errp);
  if (already_failed (errp))
    return;
  primitive_set_oldest_entry (d, oldest_entry);
  check_oldest_newest_diff (d, errp);
}

/// Get the newest entry.  Readers must check for stompledness.

static unt64 get_newest_entry (const pool_mmap_data *d)
{
  // Newest entry is always safe to return, regardless of pool emptiness
  return primitive_get_newest_entry (d);
}

/// Set the newest entry.

static void set_newest_entry (pool_mmap_data *d, unt64 newest_entry,
                              ob_retort *errp)
{
  check_newest_forward_progress (d, newest_entry, errp);
  if (already_failed (errp))
    return;
  primitive_set_newest_entry (d, newest_entry);
}

static const int64 nothing[5] = {0, 0, 0, 0, 0};

/// Turn an entry into a usable memory location.  The entry is a
/// virtual address which must be converted into an offset into
/// mmap()ed region of memory.

static byte *entry_to_address_internal (const pool_mmap_data *d, unt64 entry,
                                        char *errmsg, size_t errmsg_len,
                                        ob_retort *errp)
{
  if (already_failed (errp))
    return (byte *) &nothing[0];
  if (d->mapped_size != get_file_size (d))
    {
      *errp = POOL_SIZE_CHANGED;
      return (byte *) &nothing[0];
    }
  // Entry is relative to d->mem, and grows without bound, so we must
  // take its value modulo the pool size to get its offset within the
  // mmap()ed region.  Then we add the start of the mmap()ed region to
  // get an address in memory.
  unt64 offset = entry % d->mapped_size;
  if (offset < POOL_MMAP_PROTEINS_START_OFFSET (d))
    {
      snprintf (errmsg, errmsg_len,
                "Entry %" OB_FMT_64 "u was unexpectedly "
                "in pool header\n"
                "(%" OB_FMT_64 "u < %" OB_FMT_64 "u)\n",
                entry, offset, POOL_MMAP_PROTEINS_START_OFFSET (d));
      *errp = POOL_CORRUPT;
      return (byte *) &nothing;
    }
  return d->mem + offset;
}

static byte *entry_to_address_ (const pool_mmap_data *d, unt64 entry,
                                const char *file, int line, ob_retort *errp)
{
  char buf[160];
  buf[0] = 0;
  byte *result = entry_to_address_internal (d, entry, buf, sizeof (buf), errp);
  if (buf[0])
    {
      ob_log_loc (file, line, OBLV_ERROR, 0,
                  "hose '%s' pool '%s': When called from here:\n", hname (d),
                  pname (d));
      OB_LOG_ERROR_CODE (0x2010400e, "%s", buf);
    }
  return result;
}

/// Wrap above function in a macro, to provide a bit more context
/// on error (e. g. for debugging bug 161)

#define entry_to_address(d, entry, errp)                                       \
  entry_to_address_ (d, entry, __FILE__, __LINE__, errp)

/// Get the offset of the first entry in the pool.  Pool must be
/// non-empty.  Called only by the reader.
///
/// This isn't as simple as it would seem - the first entry needs to
/// be in the 64-bit address space, but we don't keep that information
/// anywhere.  Instead, we use the value of the oldest and newest
/// entry to reverse engineer the first entry's virtual address,
/// because the first entry must be less than the newest entry and
/// greater than the oldest entry.  Also, there may be no first entry
/// if the pool is in the middle of overwriting, so the caller must
/// check if it's stompled before trusting it - as usual.

// It used to be that for empty pools, the first entry was always
// valid.  But now, with pool_advance_oldest(), it's now possible
// to create a situation such that the oldest and newest entry are
// both greater than the first entry.  In other words, there can now
// be unused space at the beginning of the file, not just the end.
// get_first_entry() still returns the first entry, the one right
// after the header, even if it is invalid.

static unt64 get_first_entry (const pool_mmap_data *d, ob_retort *errp)
{
  if (already_failed (errp))
    return 0;
  // Since the pool is non-empty, the first entry is always <= the
  // newest entry, so use that to get the first entry address.
  unt64 newest_entry = get_newest_entry (d);
  // Figure out how far ahead newest entry is from the beginning of the file
  unt64 offset = entry_to_address (d, newest_entry, errp) - d->mem;
  // Now we have the beginning of the file
  unt64 first_entry = newest_entry - offset;
  // Add back in the protein start offset
  first_entry += POOL_MMAP_PROTEINS_START_OFFSET (d);
  OB_LOG_DEBUG_CODE (0x20104046,
                     "newest = 0x%016" OB_FMT_64 "x\n"
                     "first  = 0x%016" OB_FMT_64 "x\n",
                     newest_entry, first_entry);
  return first_entry;
}

// If the first entry is invalid for the reasons explained above,
// returns the oldest entry instead.  In other words, returns the
// entry immediately after the empty space at the beginning of the
// file, if any.

static unt64 get_first_valid_entry (const pool_mmap_data *d, ob_retort *errp)
{
  if (already_failed (errp))
    return 0;

  // This might get stompled, but that's okay; the caller must
  // check for that.
  const unt64 oldest = get_oldest_entry (d);
  const unt64 first = get_first_entry (d, errp);
  if (already_failed (errp))
    return 0;

  OB_LOG_DEBUG_CODE (0x20104045,
                     "mapped_size = 0x%016" OB_FMT_64 "x\n"
                     "oldest      = 0x%016" OB_FMT_64 "x\n"
                     "first       = 0x%016" OB_FMT_64 "x\n",
                     d->mapped_size, oldest, first);
  if (oldest > first)
    return oldest;
  else
    return first;
}

/// Was this entry overwritten?  Pool must be non-empty.
///
/// This check tells us whether the entry in question was valid at the
/// point this function was called.  It tells us nothing whatsoever
/// about the validity of the entry after this function returns.  So
/// its proper use is:
///
/// unt64 some_entry = go_get_some_entry();
/// [read some part of the entry, such as index]
/// if (is_entry_stompled (d, some_entry)
///   goto retry;
/// [act on the data read prior to the stompled check]
///
/// This feels like a strange way to operate - read something first,
/// knowing it may be wrong, and then test for correctness afterwards
/// - but it's how the lock-free read works.

static bool is_entry_stompled (const pool_mmap_data *d, unt64 entry,
                               ob_retort *errp)
{
  if (already_failed (errp))
    return true;
  unt64 newest_entry;
  // In the case of the pool being temporarily empty, every single
  // entry is stompled.
  if (is_pool_temporarily_empty (d, NULL, &newest_entry))
    return true;
  // Any entry we are checking should always be <= newest
  if (entry > newest_entry)
    {
      OB_LOG_ERROR_CODE (0x20104010,
                         "hose '%s' pool '%s': Checking entry %" OB_FMT_64
                         "u > newest entry %" OB_FMT_64 "u\n",
                         hname (d), pname (d), entry, newest_entry);
      *errp = POOL_CORRUPT;
      return true;
    }
  // Note: below means that the oldest entry must be updated in
  // deposit() before any proteins are overwritten.
  if (entry < get_oldest_entry (d))
    // This entry was stompled on by big bad mean new proteins
    return true;
  else
    // Anything we copied out of the entry before this call is safe -
    // but the entry itself may have already been overwritten by now!
    return false;
}

/// Given an entry, extract its index.  If it was stompled, return -1.

static int64 entry_to_index_ (const pool_mmap_data *d, unt64 entry,
                              const char *file, int line, ob_retort *errp)
{
  if (already_failed (errp))
    return 0;

  ob_retort tort = OB_OK;
  char buf[160];
  buf[0] = 0;
  byte *addr = entry_to_address_internal (d, entry, buf, sizeof (buf), &tort);
  unt64 idx = *(unt64 *) (addr + POOL_MMAP_INDEX_OFFSET);
  // Can we trust the index?  If not, caller must retry
  if (is_entry_stompled (d, entry, errp))
    return -1;
  if (already_failed (errp))
    return 0;
  if (tort < OB_OK)
    {
      *errp = tort;
      if (buf[0])
        {
          ob_log_loc (file, line, OBLV_ERROR, 0,
                      "hose '%s' pool '%s': When called from here:\n",
                      hname (d), pname (d));
          OB_LOG_ERROR_CODE (0x20104052, "%s", buf);
        }
      return 0;
    }

  if (idx > MAXIMUM_REASONABLE_INDEX)
    {
      OB_LOG_ERROR_CODE (0x20104050,
                         "hose '%s' pool '%s':\n"
                         "index %" OB_FMT_64 "u is probably\n"
                         "bogus, because it is greater than\n"
                         "%" OB_FMT_64 "u, which is what the\n"
                         "index would be if it was incremented\n"
                         "once per nanosecond for a hundred years.\n",
                         hname (d), pname (d), idx, MAXIMUM_REASONABLE_INDEX);
      *errp = POOL_CORRUPT;
    }
  return idx;
}

#define entry_to_index(d, entry, errp)                                         \
  entry_to_index_ (d, entry, __FILE__, __LINE__, errp)

/// The oldest and newest indexes are set by setting the oldest and
/// newest entries - entries contain their indexes.  The oldest and
/// newest indexes should not be stored separately from the pointers
/// to their entries since this introduces nasty race conditions.
/// Instead, extract the oldest and newest indexes from the oldest and
/// newest entries, retrying if they have been stompled.

// XXX We should avoid busy waiting if the pool is empty and a deposit
// is going on.  However, given that this is an unlikely case and is a
// performance problem only, go ahead and defer it for now.

static int64 get_newest_index_ (const pool_mmap_data *d, const char *file,
                                int line, ob_retort *errp)
{
  // Do we have any hope of a protein existing in the pool?
  if (already_failed (errp) || is_pool_empty (d))
    return -1;
  // XXX busy-waiting when protein size ~ pool size
  int64 idx;
  unt64 loop_count = 0;
  // -1 means the newest entry was stompled before we could get the
  // index out.
  while ((idx = entry_to_index_ (d, get_newest_entry (d), file, line, errp))
           == -1
         && !already_failed (errp))
    {
      if (is_pool_empty (d))
        return -1;
      loop_count++;
      if (loop_count == 100000)
        {
          ob_log (OBLV_INFO, 0x20104029, "busy waiting on newest\n");
          loop_count = 0;
        }
    }
  return idx;
}

#define get_newest_index(d, errp)                                              \
  get_newest_index_ (d, __FILE__, __LINE__, errp)

static int64 get_oldest_index (const pool_mmap_data *d, ob_retort *errp)
{
  // Do we have any hope of a protein existing in the pool?
  if (already_failed (errp) || is_pool_empty (d))
    return -1;
  // XXX busy-waiting when protein size ~ pool size
  int64 idx;
  unt64 loop_count = 0;
  // -1 means the oldest entry was stompled before we could get the
  // index out.
  while ((idx = entry_to_index (d, get_oldest_entry (d), errp)) == -1
         && !already_failed (errp))
    {
      if (is_pool_empty (d))
        return -1;
      loop_count++;
      if (loop_count == 100000)
        {
          ob_log (OBLV_INFO, 0x2010402a, "busy waiting on oldest\n");
          loop_count = 0;
        }
    }
  return idx;
}

// Note to implementors: Don't create a routine for getting the first
// index out of a pool - you're only doing that to get the first entry
// out of the pool, and you can't get the index and then get the
// entry, because the entry may have been overwritten and no longer
// match the index.  You have to get the entry, pull out its index,
// compare it with the index you want, and then start the search from
// the entry, checking for stompled-ness as you go.

/// mmap implementation of newest_index() function.

ob_retort pool_mmap_newest_index (pool_hose ph, int64 *idx)
{
  pool_mmap_data *d = pool_mmap_get_data (ph);
  ob_retort tort;
  do
    {
      tort = OB_OK;
      check_for_size_change (d, &tort);
      if (already_failed (&tort))
        break;
      *idx = get_newest_index (d, &tort);
      if (*idx == -1 && !already_failed (&tort))
        return POOL_NO_SUCH_PROTEIN;
    }
  while (tort == POOL_SIZE_CHANGED);
  return tort;
}

/// mmap implementation of oldest_index() function.

ob_retort pool_mmap_oldest_index (pool_hose ph, int64 *idx)
{
  pool_mmap_data *d = pool_mmap_get_data (ph);
  ob_retort tort;
  do
    {
      tort = OB_OK;
      check_for_size_change (d, &tort);
      if (already_failed (&tort))
        break;
      *idx = get_oldest_index (d, &tort);
      if (*idx == -1 && !already_failed (&tort))
        return POOL_NO_SUCH_PROTEIN;
    }
  while (tort == POOL_SIZE_CHANGED);
  return tort;
}

/// Given an entry, return the protein.  Caller must check for
/// stompled-ness.

static protein get_protein_from_entry (const pool_mmap_data *d, unt64 entry,
                                       ob_retort *errp)
{
  byte *addr = entry_to_address (d, entry, errp);
  return (protein) (addr + POOL_MMAP_PROTEIN_OFFSET (d));
}

/// Get the size of an entry that would contain this protein.  Can only
/// be used in deposit path.

static int64 entry_size_from_protein (const pool_mmap_data *d, bprotein p)
{
  return d->vprotein_len (p) + POOL_MMAP_PROTEIN_OFFSET (d)
         + POOL_MMAP_JUMPBACK_LEN;
}

/// Get the size of an entry from the actual entry.  Can only be used
/// in the deposit path.  Can't be called with entry >= pool size.

static int64 entry_size_from_entry (const pool_mmap_data *d, unt64 entry,
                                    ob_retort *errp)
{
  if (already_failed (errp))
    return 0;
  int64 size =
    entry_size_from_protein (d, get_protein_from_entry (d, entry, errp));
  if (size > pool_entry_space (d) && !already_failed (errp))
    {
      OB_LOG_ERROR_CODE (0x20104011,
                         "hose '%s' pool '%s': Entry size %" OB_FMT_64
                         "u > than pool size %" OB_FMT_64 "u\n",
                         hname (d), pname (d), size, pool_entry_space (d));
      *errp = POOL_CORRUPT;
      return 0;
    }
  return size;
}

/// Safe version of protein_len (), for use in read path.
/// We cannot call protein_len () on a protein in the pool in the
/// read path, since the protein may be overwritten underneath
/// us. (Well, we can, but it's basically the same as calling
/// protein_len on complete garbage - not safe.)  Instead, we use
/// protein_len_safe (), which makes a copy of the protein header and
/// checks for overwriting before returning.  This bubbles up into
/// entry_size_from_entry_safe ().

static int64 protein_len_safe (const pool_mmap_data *d, protein p, unt64 entry,
                               ob_retort *errp)
{
  if (already_failed (errp))
    return -1;
  // For slaw version 2, we only need 8 bytes (1 oct), but
  // for slaw version 1, we might need up to 12 bytes (3 quads).
  byte protein_header[12];
  memcpy (protein_header, p, sizeof (protein_header));
  // Did the header get stompled?
  if (is_entry_stompled (d, entry, errp) || already_failed (errp))
    return -1;
  return d->vprotein_len ((protein) protein_header);
}

/// Safe version of entry size routine, for use in read path.

static int64 entry_size_from_entry_safe (const pool_mmap_data *d, unt64 entry,
                                         ob_retort *errp)
{
  protein p = get_protein_from_entry (d, entry, errp);
  int64 size = protein_len_safe (d, p, entry, errp);
  // Check for stompled-ness
  if (size == -1 || already_failed (errp))
    return size;
  size += POOL_MMAP_PROTEIN_OFFSET (d) + POOL_MMAP_JUMPBACK_LEN;
  if (size > pool_entry_space (d))
    {
      OB_LOG_ERROR_CODE (0x20104012,
                         "hose '%s' pool '%s': Entry size %" OB_FMT_64
                         "u > than pool size %" OB_FMT_64 "u\n",
                         hname (d), pname (d), size, pool_entry_space (d));
      *errp = POOL_CORRUPT;
      return 0;
    }
  return size;
}

/// Returns the size of the *previous* entry, by using the "jumpback"
/// field.  Returns -1 if stompled.  Illegal to call if "entry" is the
/// first entry.

static int64 jumpback_size_from_entry_safe (const pool_mmap_data *d,
                                            unt64 entry, ob_retort *errp)
{
  if (already_failed (errp))
    return 0;
  int64 jumpback = *(volatile int64 *) entry_to_address (
    d, entry - POOL_MMAP_JUMPBACK_LEN, errp);
  if (is_entry_stompled (d, entry, errp) || already_failed (errp))
    return -1;
  if (jumpback <= (POOL_MMAP_JUMPBACK_LEN + POOL_MMAP_PROTEIN_OFFSET (d)))
    {
      OB_LOG_ERROR_CODE (0x20104013,
                         "hose '%s' pool '%s': "
                         "Didn't expect jumpback to be %" OB_FMT_64 "d\n",
                         hname (d), pname (d), jumpback);
      *errp = POOL_CORRUPT;
      return 0;
    }
  return jumpback;
}


/// Pull out the timestamp from an entry.  Caller must check for
/// stompled-ness.

static inline pool_timestamp timestamp_from_entry (const pool_mmap_data *d,
                                                   unt64 entry, ob_retort *errp)
{
  return *(pool_timestamp *) (entry_to_address (d, entry, errp)
                              + POOL_MMAP_TIMESTAMP_OFFSET);
}

/* Pull out the checksum from an entry.  Caller must check for
 * stompled-ness. */

static inline unt64 checksum_from_entry (const pool_mmap_data *d, unt64 entry,
                                         ob_retort *errp)
{
  return *(unt64 *) (entry_to_address (d, entry, errp)
                     + POOL_MMAP_CHECKSUM_OFFSET);
}

static ob_retort find_entry (pool_mmap_data *d, int64 idx, unt64 *ret_entry);

/// Routines for performing lookups by time

static inline pool_timestamp timestamp_from_index (pool_mmap_data *d, int64 idx,
                                                   ob_retort *errp)
{
  if (already_failed (errp))
    return OB_NAN;
  unt64 entry = 0;
  if (find_entry (d, idx, &entry) != OB_OK)
    entry = get_oldest_entry (d);
  return timestamp_from_entry (d, entry, errp);
}

static inline void fill_with_oldest (const pool_mmap_data *d, pool_toc_entry *e,
                                     ob_retort *errp)
{
  if (already_failed (errp))
    return;
  e->idx = get_oldest_index (d, errp);
  e->offset = get_oldest_entry (d);
  e->stamp = timestamp_from_entry (d, e->offset, errp);
}

static inline void fill_with_newest (const pool_mmap_data *d, pool_toc_entry *e,
                                     ob_retort *errp)
{
  if (already_failed (errp))
    return;
  e->idx = get_newest_index (d, errp);
  e->offset = get_newest_entry (d);
  e->stamp = timestamp_from_entry (d, e->offset, errp);
}

static bool fill_with_offset (const pool_mmap_data *d, unt64 offset,
                              pool_toc_entry *e, ob_retort *errp)
{
  if (already_failed (errp))
    return false;
  int64 idx = entry_to_index (d, offset, errp);
  if (idx < 0)
    return false;
  e->stamp = timestamp_from_entry (d, offset, errp);
  e->offset = offset;
  e->idx = idx;
  return true;
}

static ob_retort find_index_lookup_ends (const pool_mmap_data *d,
                                         pool_timestamp ts,
                                         time_comparison bound,
                                         pool_toc_entry *start,
                                         pool_toc_entry *end)
{
  pool_toc_t *pi = d->ptoc;
  ob_retort tort = OB_OK;
  if (!pi || !pool_toc_find_timestamp (pi, ts, start, end))
    {
      fill_with_oldest (d, start, &tort);
      if (ts < start->stamp && bound == OB_CLOSEST_LOWER)
        return POOL_NO_SUCH_PROTEIN;
      fill_with_newest (d, end, &tort);
      if (ts > end->stamp && bound == OB_CLOSEST_HIGHER)
        return POOL_NO_SUCH_PROTEIN;
    }
  else if (POOL_TOC_ENTRY_NULL_P (*start))
    fill_with_oldest (d, start, &tort);

  if (already_failed (&tort))
    return tort;

  return POOL_TOC_ENTRY_NULL_P (*start) ? POOL_NO_SUCH_PROTEIN : OB_OK;
}

static void find_next_protein_data (const pool_mmap_data *d, pool_toc_entry *e,
                                    ob_retort *errp)
{
  if (already_failed (errp))
    return;
  if (!POOL_TOC_ENTRY_NULL_P (*e) && e->idx < get_newest_index (d, errp))
    {
      int64 delta = entry_size_from_entry_safe (d, e->offset, errp);
      if (delta < 0 || !fill_with_offset (d, e->offset + delta, e, errp))
        fill_with_oldest (d, e, errp);
    }
  else
    *e = POOL_TOC_NULL_ENTRY;
}

static void find_previous_protein_data (const pool_mmap_data *d,
                                        pool_toc_entry *e, ob_retort *errp)
{
  if (already_failed (errp))
    return;
  if (!POOL_TOC_ENTRY_NULL_P (*e) && e->idx > get_oldest_index (d, errp))
    {
      int64 delta = jumpback_size_from_entry_safe (d, e->offset, errp);
      if (delta < 0 || !fill_with_offset (d, e->offset - delta, e, errp))
        fill_with_newest (d, e, errp);
    }
  else
    *e = POOL_TOC_NULL_ENTRY;
}

static ob_retort find_timestamp (const pool_mmap_data *d, pool_timestamp ts,
                                 const pool_toc_entry *start,
                                 const pool_toc_entry *last,
                                 time_comparison bound, int64 *idx)
{
  assert (!POOL_TOC_ENTRY_NULL_P (*start));

  if (start->stamp >= ts)
    {
      if (start->stamp > ts && OB_CLOSEST_LOWER == bound)
        return POOL_NO_SUCH_PROTEIN;
      *idx = start->idx;
      return OB_OK;
    }

  pool_toc_entry prev = POOL_TOC_NULL_ENTRY, cur = *start;
  while (!POOL_TOC_ENTRY_NULL_P (cur) && cur.stamp <= ts)
    {
      prev = cur;
      ob_retort tort = OB_OK;
      find_next_protein_data (d, &cur, &tort);
      if (already_failed (&tort))
        return tort;
    }

  const pool_toc_entry end = POOL_TOC_ENTRY_NULL_P (*last) ? cur : *last;

  if ((POOL_TOC_ENTRY_NULL_P (cur) || POOL_TOC_ENTRY_NULL_P (end))
      && OB_CLOSEST_HIGHER == bound && prev.stamp != ts)
    return POOL_NO_SUCH_PROTEIN;

  if (POOL_TOC_ENTRY_NULL_P (cur) || POOL_TOC_ENTRY_NULL_P (end)
      || prev.stamp == ts || OB_CLOSEST_LOWER == bound)
    {
      *idx = prev.idx;
      return OB_OK;
    }

  assert (bound != OB_CLOSEST_LOWER);
  pool_toc_entry post = POOL_TOC_NULL_ENTRY;
  cur = end;
  while (!POOL_TOC_ENTRY_NULL_P (cur) && cur.stamp > ts)
    {
      post = cur;
      ob_retort tort = OB_OK;
      find_previous_protein_data (d, &cur, &tort);
      if (already_failed (&tort))
        return tort;
    }

  if (OB_CLOSEST_HIGHER == bound)
    {
      *idx = post.idx;
      return OB_OK;
    }

  assert (OB_CLOSEST == bound);
  assert (post.stamp >= ts);
  assert (prev.stamp <= ts);
  *idx = (post.stamp - ts >= ts - prev.stamp) ? prev.idx : post.idx;

  return OB_OK;
}

static ob_retort pool_mmap_index_lookup (pool_hose ph, int64 *idx,
                                         pool_timestamp ts,
                                         time_comparison bound, bool relative)
{
  assert (ph && idx);
  pool_mmap_data *d = pool_mmap_get_data (ph);
  assert (d);
  if (is_pool_empty (d))
    return POOL_NO_SUCH_PROTEIN;
  ob_retort tort = OB_OK;
  if (relative)
    ts += timestamp_from_index (d, ph->index, &tort);
  if (already_failed (&tort))
    return tort;
  pool_toc_entry start = POOL_TOC_NULL_ENTRY, end = POOL_TOC_NULL_ENTRY;
  ob_retort ret = find_index_lookup_ends (d, ts, bound, &start, &end);
  if (OB_OK == ret)
    ret = find_timestamp (d, ts, &start, &end, bound, idx);
  return ret;
}

/// Routines for depositing a protein in the pool

/// Prepare the pool for a new write by updating the oldest entry and
/// computing the correct place to write the new entry.  The new write
/// entry is returned.  The major issue here is to handle the
/// wraparound at the end of the file, when we overwrite old proteins.

// As an interesting side note, due to the wraparound effect we can
// have entries at the end of the pool which have not been overwritten
// but are nonetheless gone.  This happens when a large protein would
// wrap around the end of the pool, so the write pointer is advanced
// to the beginning of the pool - and the oldest entry pointer is
// advanced beyond the poor little orphaned proteins at the end of the
// pool.

// XXX have no idea what the below comment means. -VAL
//
// XXX XXX XXX there may not be an oldest entry at this point if we
// are going to overwrite every protein in the pool!  The new oldest
// entry will be ourselves, after we finish writing.  The normal
// signal for this is for the newest entry to be less than the oldest
// entry...

static unt64 pool_prepare_write (pool_mmap_data *d, bprotein p, ob_retort *errp)
{
  if (already_failed (errp))
    return 0;

  const unt64 flags = get_flags (d);
  if (0 != (flags & POOL_FLAG_FROZEN))
    {
      *errp = POOL_FROZEN;
      return 0;
    }

  // If the pool is empty, oldest entry == write entry
  if (is_pool_empty (d))
    // Don't use get_oldest_entry(), will complain on empty pool
    return primitive_get_oldest_entry (d);

  int64 sz;
  unt64 oldest_entry = get_oldest_entry (d);
  unt64 new_oldest_entry = oldest_entry;
  // First, decide where we're going to write.  If the end of the
  // protein wraps around the end of the pool, move our write pointer
  // to the beginning of the pool.
  unt64 newest_entry = get_newest_entry (d);
  unt64 write_entry =
    (newest_entry + (sz = entry_size_from_entry (d, newest_entry, errp)));
  unt64 write_len = entry_size_from_protein (d, p);
  unt64 first_entry = get_first_entry (d, errp);
  int64 first_index = entry_to_index (d, first_entry, errp);

  // Map the begining of the entry on to the pool
  byte *begin_addr = sz + entry_to_address (d, newest_entry, errp);
  if ((begin_addr + write_len) >= (d->mem + d->mapped_size))
    {
      // When we wrap, the entry keeps increasing
      write_entry = first_entry + d->mapped_size;
      // If we wrap, the entry we are writing may be completely beyond
      // the old entry, not even overlapping.  So when we wrap, we
      // have to push the oldest pointer ahead to the first entry, and
      // then search forward to the next entry that isn't overwritten.
      // Note that there can be no such entry, in which case we need
      // to set the oldest entry to the slot beyond the one we are
      // writing.  Readers need to be aware of this, via
      // is_entry_stompled().
      new_oldest_entry = first_entry;
    }
  byte *write_addr = entry_to_address (d, write_entry, errp);

  // Okay, now we know where we're writing.  Find out if we're
  // overlapping the oldest pointer and if so, move the oldest pointer
  // up.  We have to work in addresses because in entry space, nothing
  // ever overlaps.
  byte *new_oldest_addr = entry_to_address (d, new_oldest_entry, errp);
  // If we've moved the oldest pointer ahead of the write pointer, we
  // won't even enter this clause.
  while ((write_addr <= new_oldest_addr)
         && ((write_addr + write_len) > new_oldest_addr))
    {
      // If in our search for the oldest valid protein, we've caught
      // up to the newest entry, then, heck, we're all out of proteins
      // because we durn overwrote them all with this new protein.  No
      // problem-o, our new oldest entry is the one we're about to
      // write.
      if (new_oldest_entry == newest_entry)
        {
          new_oldest_entry = write_entry;
          break;
        }
      // Move forward to the next entry - but first harken to my words:
      //
      // When we are at the last entry in the pool, we need to jump to
      // the first entry instead of reading the garbage following the
      // last good entry.  The best way to check for this is to
      // compare the index of the current entry and that of the first
      // entry, and use the first entry if it's next in line.
      //
      if ((entry_to_index (d, new_oldest_entry, errp) + 1) == first_index)
        new_oldest_entry = first_entry;
      else
        new_oldest_entry += entry_size_from_entry (d, new_oldest_entry, errp);
      new_oldest_addr = entry_to_address (d, new_oldest_entry, errp);

      if (already_failed (errp))
        return 0;

      // Sanity checks to see if we've run off into la-la land
      if (new_oldest_entry > write_entry)
        {
          OB_LOG_ERROR_CODE (0x20104017,
                             "hose '%s' pool '%s': new oldest entry "
                             "(%" OB_FMT_64 "u) passed write (%" OB_FMT_64 "u"
                             ") during wraparound\n",
                             hname (d), pname (d), new_oldest_entry,
                             write_entry);
          *errp = POOL_CORRUPT;
          return 0;
        }

      const int64 new_oldest_index = entry_to_index (d, new_oldest_entry, errp);
      const int64 newest_index = entry_to_index (d, newest_entry, errp);
      const int64 oldest_index = entry_to_index (d, oldest_entry, errp);
      if (already_failed (errp))
        return 0;

      // Can only compare indexes if the oldest entry points to
      // something valid; i.e., isn't the same as the write pointer.
      if ((new_oldest_entry != write_entry)
          && (new_oldest_index >= (newest_index + 1)))
        {
          OB_LOG_ERROR_CODE (
            0x20104018,
            "hose '%s' pool '%s': "
            "new oldest index (%" OB_FMT_64 "u) passed write index (%" OB_FMT_64
            "u) during wraparound\n",
            hname (d), pname (d), new_oldest_index, newest_index + 1);
          *errp = POOL_CORRUPT;
          return 0;
        }
      if (new_oldest_index < oldest_index)
        {
          OB_LOG_ERROR_CODE (
            0x20104019,
            "hose '%s' pool '%s': "
            "new oldest index (%" OB_FMT_64 "u) is before old oldest "
            "index (%" OB_FMT_64 "u) during wraparound\n"
            "Some additional info for debugging bug 2085:\n"
            "newest_index = %" OB_FMT_64 "u\n"
            "first_index  = %" OB_FMT_64 "u\n"
            "new_oldest_entry = 0x%016" OB_FMT_64 "x\n"
            "newest_entry     = 0x%016" OB_FMT_64 "x\n"
            "oldest_entry     = 0x%016" OB_FMT_64 "x\n"
            "first_entry      = 0x%016" OB_FMT_64 "x\n"
            "sz = %" OB_FMT_64 "u\n"
            "write_len = %" OB_FMT_64 "u\n",
            hname (d), pname (d), new_oldest_index, oldest_index, newest_index,
            first_index, new_oldest_entry, newest_entry, oldest_entry,
            first_entry, sz, write_len);
          *errp = POOL_CORRUPT;
          return 0;
        }
    }

  // The new oldest entry can actually be ahead of the newest entry,
  // as long as it's the same as our write pointer.
  if ((new_oldest_entry > newest_entry) && (new_oldest_entry != write_entry))
    {
      OB_LOG_ERROR_CODE (0x2010401a,
                         "hose '%s' pool '%s': CORRUPTION!!! oldest "
                         "(%" OB_FMT_64 "u) > newest (%" OB_FMT_64 "u)\n",
                         hname (d), pname (d), new_oldest_entry, newest_entry);
      *errp = POOL_CORRUPT;
      return 0;
    }
  if (write_entry < new_oldest_entry)
    {
      OB_LOG_ERROR_CODE (0x2010401b,
                         "hose '%s' pool '%s': CORRUPTION!!! write (%" OB_FMT_64
                         "u) < oldest (%" OB_FMT_64 "u)\n",
                         hname (d), pname (d), write_entry, new_oldest_entry);
      *errp = POOL_CORRUPT;
      return 0;
    }
  if ((write_addr + write_len) > (d->mem + d->mapped_size))
    {
      OB_LOG_ERROR_CODE (
        0x2010401c,
        "hose '%s' pool '%s': "
        "CORRUPTION!!! write %p - %" OB_FMT_64 "u > pool size %p\n",
        hname (d), pname (d), write_addr, write_len, d->mem + d->mapped_size);
      *errp = POOL_CORRUPT;
      return 0;
    }

  // IMPORTANT: no side-effects above this line!

  if (oldest_entry != new_oldest_entry)
    {
      if (0 != (flags & POOL_FLAG_STOP_WHEN_FULL))
        {
          *errp = POOL_FULL;
          return 0;
        }
      set_oldest_entry (d, new_oldest_entry, errp);
      pool_mmap_sync (d, errp);
    }
  return write_entry;
}

/// Given a write entry and some data, write the data to the proper
/// location in the mmap()ed region.  Update the write entry to point
/// to the byte following the last byte written.

static void write_mmap_file (pool_mmap_data *d, unt64 *write_entry,
                             const void *data, unt64 size, ob_retort *errp)
{
  byte *write_addr = entry_to_address (d, *write_entry, errp);
  if (already_failed (errp))
    return;
  // Copy the data into the mmap file
  memcpy (write_addr, data, size);
  // Bump the write entry
  *write_entry += size;
}

static unt64 compute_entry_checksum (bprotein p, int64 plen, pool_timestamp ts,
                                     int64 idx)
{
  unt64 ts_as_integer;
  memcpy (&ts_as_integer, &ts, sizeof (ts_as_integer));
  return ob_city_hash64_with_seeds (p, plen, ts_as_integer, idx);
}

/// The actual exported protein deposit function.

ob_retort pool_mmap_deposit (pool_hose ph, bprotein p, int64 *idx,
                             pool_timestamp *ret_ts)
{
  pool_mmap_data *d = pool_mmap_get_data (ph);
  const unt64 flags = get_flags (d);

  protein freeme = NULL;
  int64 plen;
  if (get_slaw_version (d) == SLAW_VERSION_CURRENT)
    plen = protein_len (p);
  else
    {
      // Translate protein to old version before depositing.
      // This involves more memory allocation than strictly
      // necessary, but I'm not going to shed tears over the
      // performance of legacy pools.
      freeme = slaw_dup (p);
      if (!freeme)
        return OB_NO_MEM;
      ob_retort tort = slaw_convert_to (&freeme, get_slaw_version (d), &plen);
      if (tort < OB_OK)
        {
          protein_free (freeme);
          return tort;
        }
      p = freeme;
    }

  // Get the time of day outside the lock - it can take a long time,
  // in terms of lock contention, and won't make any significant
  // difference in accuracy.
  //
  // XXX Could allow non-monotonic timestamps with regard to protein
  // ordering, which applications may want to rely on - but man,
  // gettimeofday() is SLOOOWWWW.  Non-monotonicity will occur anyway
  // when the system clock is set - e.g., by hand, or by NTP updates,
  // etc., so either applications have to deal with this, or the pool
  // code implements its own monotonicity guarantees.
  // YYY Dispute the claim that gettimeofday is /SLO+W+/.  It's a
  // vsyscall, not a real syscall, and in most cases it boils down
  // to only a little bit more on top of RDTSC, so it shouldn't be
  // that bad.
  pool_timestamp timestamp = pool_timestamp_now ();

  // Note that we are dealing in terms of entries, not proteins.
  // Entries contain the protein and also the timestamp, index, and
  // jumpback length.
  unt64 entry_size = entry_size_from_protein (d, p);

  // This lock should enclose as little as possible.  Manipulation of
  // the pool's write entry, oldest entry, and newest entry, as
  // well as the actual writing of the protein must be inside it.
  ob_retort pret;
  if ((pret = pool_deposit_lock (ph)) != OB_OK)
    {
      protein_free (freeme);
      return pret;
    }

  check_for_size_change (d, &pret);

  // First sanity check: Is the entry larger than the pool?
  if (entry_size > pool_entry_space (d) && !already_failed (&pret))
    pret = POOL_PROTEIN_BIGGER_THAN_POOL;

  // The updated newest index isn't visible until we (a) write the
  // entry (containing the index) (b) set the newest entry to point to
  // it.
  int64 newest_index = get_newest_index (d, &pret);
  if (newest_index < 0)
    newest_index = ob_atomic_int64_ref (&d->conf_chunk->next_index);
  else
    newest_index++;

  unt64 former_newest_entry = get_newest_entry (d);

  // Update the oldest entry and write entry for the pool - deals with
  // proteins wrapping around the end of the queue, and with
  // overwriting the previous oldest protein.
  unt64 write_entry = pool_prepare_write (d, p, &pret);
  if (former_newest_entry == write_entry && !already_failed (&pret))
    {
      OB_LOG_ERROR_CODE (0x2010401d,
                         "hose '%s' pool '%s': "
                         "CORRUPTION!!! prepare write returned "
                         "newest entry %" OB_FMT_64 "u\n",
                         hname (d), pname (d), write_entry);
      pret = POOL_CORRUPT;
    }

  // Now we know where to write, the oldest entry has been adjusted,
  // and we know what the index of this entry will be.  All we have to
  // do is write out all the parts of the entry and update the newest
  // entry.  Note that write_mmap_file increments the write_entry by
  // the size of the data written.

  // This is where the newest entry will be when we're done, save it
  unt64 newest_entry = write_entry;
  // timestamp
  write_mmap_file (d, &write_entry, &timestamp, sizeof (timestamp), &pret);
  // index
  write_mmap_file (d, &write_entry, &newest_index, sizeof (newest_index),
                   &pret);
  // checksum; optional
  if (0 != (flags & POOL_FLAG_CHECKSUM))
    {
      unt64 checksum =
        compute_entry_checksum (p, plen, timestamp, newest_index);
      write_mmap_file (d, &write_entry, &checksum, sizeof (checksum), &pret);
    }
  // protein itself
  write_mmap_file (d, &write_entry, p, plen, &pret);
  // jumpback length
  write_mmap_file (d, &write_entry, &entry_size, sizeof (entry_size), &pret);

  pool_mmap_sync (d, &pret);

  // Readers may be looking at the newest entry at any time, so don't
  // update it until the protein is fully written out.
  set_newest_entry (d, newest_entry, &pret);

  // Check to see if we overwrote the pool header
  check_oldest_less_than_newest (d, &pret);

  // Memoize deposit in the pool index
  if (d->ptoc && !already_failed (&pret))
    {
      pool_toc_entry e = {newest_index, newest_entry, timestamp};
      if (!pool_toc_append (d->ptoc, e, get_oldest_entry (d)))
        OB_LOG_ERROR_CODE (
          0x2010401e,
          "hose '%s' pool '%s': "
          "failure registering protein in pool index: "
          "offset: %" OB_FMT_64 "u, time: %lf, index: %" OB_FMT_64 "u\n",
          hname (d), pname (d), write_entry, timestamp, newest_index);
    }

  if (!already_failed (&pret))
    ob_atomic_int64_set (&d->conf_chunk->next_index, newest_index + 1);

  // Drop the deposit lock.
  ob_err_accum (&pret, pool_deposit_unlock (ph));

  // Fifo-based single pool awaiters are also awoken by the multi-pool
  // awake.  Fifo await/awake doesn't need to hold the lock because it
  // re-checks for a protein deposit after setting up the fifo - the
  // fifo covers the race window between checking for a protein
  // deposit and going to sleep.
  ob_err_accum (&pret, pool_multi_wake_awaiters (ph));

  // Return the index of this protein if requested
  if (idx)
    *idx = newest_index;
  if (ret_ts)
    *ret_ts = timestamp;
  // All done!!
  protein_free (freeme);
  return pret;
}

static unt64 remodulate (unt64 old, unt64 offset, unt64 modulo);

ob_retort pool_mmap_advance_oldest (pool_hose ph, int64 idx_in)
{
  pool_mmap_data *d = pool_mmap_get_data (ph);
  unt64 entry;
  ob_retort pret = OB_OK;
  int64 newest_index = get_newest_index (d, &pret);
  if (already_failed (&pret))
    return pret;
  if (idx_in == newest_index + 1 && get_mmap_version (d) > 0)
    {
      // a very special case that makes the pool completely empty
      pret = pool_deposit_lock (ph);
      newest_index = get_newest_index (d, &pret);
      if (pret < OB_OK)
        return pret;
      // Is it still true, now that we have the lock?
      if (idx_in == newest_index + 1)
        {
          const unt64 new_oldest =
            remodulate (primitive_get_newest_entry (d), get_header_size (d),
                        d->mapped_size);
          // This is what makes the pool empty
          primitive_set_oldest_entry (d, new_oldest);
          primitive_set_newest_entry (d, 0);
          goto cleanup;
        }
      // If not, unlock and we're back to the "ordinary" path
      ob_err_accum (&pret, pool_deposit_unlock (ph));
    }
  if (idx_in > newest_index)
    // Return an error if idx_in is too new
    return POOL_NO_SUCH_PROTEIN;
  pret = find_entry (d, idx_in, &entry);
  if (pret == POOL_NO_SUCH_PROTEIN)
    // Since we already checked for idx_in being too new, this must mean
    // it's too old.  That's not an error; we just return a success code
    // saying we didn't have to do anything.
    return OB_NOTHING_TO_DO;
  else if (pret < OB_OK)
    // some unexpected error?
    return pret;

  pret = pool_deposit_lock (ph);
  if (pret < OB_OK)
    return pret;

  // No more early returns allowed from here on out, because we
  // are holding the lock.

  // Is the entry found by find_entry() outside the lock still valid?
  // This would check if we are *less* than the oldest entry:
  //   const bool stompled = is_entry_stompled (d, entry, &pret);
  // But we really want to know if we are less than or *equal* to oldest:
  {
    const bool stompled = (entry <= get_oldest_entry (d));
    if (already_failed (&pret))
      goto cleanup;
    if (stompled)
      {
        // A different way idx_in can be too old.  Once again, this
        // is not an error; we just return a success code
        // saying we didn't have to do anything.
        pret = OB_NOTHING_TO_DO;
        goto cleanup;
      }
  }

  // Here's where it happens!
  set_oldest_entry (d, entry, &pret);

cleanup:
  // Drop the deposit lock.
  ob_err_accum (&pret, pool_deposit_unlock (ph));
  return pret;
}

/// Routines for reading proteins out of the pool
///
/// The exciting bit is that we're reading the protein from the pool
/// without holding a lock.  This is tricky, since writes may be
/// happening concurrently and may overwrite anything we read.  What
/// this means in practice is that each time we read data from the
/// pool, we have to test to see if it was overwritten by a new
/// protein (stompled), before we trust that data - e.g., use the
/// information in it to calculate the next index or entry, or send a
/// protein back to the caller.  We also have to be aware that writes
/// may change the oldest and newest entry pointers at any time.
/// Observant readers will notice that there's a small window where
/// the data we copied was correct, but gets overwritten before we do
/// the check, and so we end up discarding good data.  This is okay -
/// it's the tradeoff we're making, fast deposits at the expense of
/// reads.
///
/// Ordering constraints:
///
/// oldest_entry must be set before writing to pool
/// oldest_index must be derived from oldest_entry by reader
/// newest_entry must be set after newest entry fully written
/// newest_index must be derived from newest_entry by reader
///
/// Keeping the indexes separately from the entry pointers opens up a
/// race condition window between the updates to the index and the
/// entry, and since the read path uses both, it can get them out of
/// sync, with disastrous consequences.
///
/// The write entry (place to write the next protein deposited) is
/// derived from the newest entry by the depositor.
///
/// Readers are multi-threaded with respect to deposits, while deposits
/// are single-threaded.

/// Has this index been stompled?  Index must be <= newest index and
/// pool must not be empty.

static bool is_index_stompled (const pool_mmap_data *d, int64 idx,
                               ob_retort *errp)
{
  // get_oldest_index retries until it gets a valid index
  return (idx < get_oldest_index (d, errp));
}

/// Pick the best starting point for a forward search to find the
/// entry with the desired index.  The returned starting_entry must be between
/// the beginning of the mmap file and the desired entry.  (Analogously,
/// the optional ending_entry, if returned, must be between the desired entry
/// and the end of the mmap file.)
/// Returns @c true on success and @c false if the entry is stompled.

static bool find_starting_point (const pool_mmap_data *d, int64 target_index,
                                 // output-only parameters
                                 int64 *starting_index, unt64 *starting_entry,
                                 int64 *ending_index, unt64 *ending_entry,
                                 // input-output parameter
                                 ob_retort *errp)
{
  // Does this entry even exist?
  if (is_index_stompled (d, target_index, errp) || already_failed (errp))
    return false;

  // Retry until we get a valid first entry
  unt64 first_entry;
  int64 first_index;
  do
    {
      first_entry = get_first_valid_entry (d, errp);
      first_index = entry_to_index (d, first_entry, errp);
      if (already_failed (errp))
        return false;
    }
  while (first_index == -1);

  // A common case when doing pool_next() - the requested entry is the
  // entry following the last entry we read.
  //
  // Note that if the cached index is the last protein in the pool
  // (as laid out in memory), then the following entry is the
  // first entry.  We know that this cached entry is not the
  // newest entry, because that case is caught before we ever
  // enter this function.

  bool cached_okay;
  // Is the cached entry before the desired entry?
  if ((cached_okay = !is_index_stompled (d, d->cached_index, errp))
      && (target_index >= d->cached_index))
    {
      // Catch the aforementioned wrap-around case, where the
      // following protein is actually the first entry in the
      // pool, rather than following this protein (in terms of how
      // entries are laid out in memory).
      if (first_index > d->cached_index && target_index >= first_index)
        {
          *starting_entry = first_entry;
          *starting_index = first_index;
        }
      else
        {
          *starting_entry = d->cached_entry;
          *starting_index = d->cached_index;
        }
    }
  else if (target_index >= first_index)
    {
      // Next best starting point: first entry.
      *starting_entry = first_entry;
      *starting_index = first_index;
    }
  else
    {
      // Only one option left...
      *starting_entry = get_oldest_entry (d);
      *starting_index = entry_to_index (d, *starting_entry, errp);
    }

  OB_LOG_DEBUG_CODE (0x20104043,
                     "cached_okay  = %s\n"
                     "target_index = %" OB_FMT_64 "d\n"
                     "first_index  = %" OB_FMT_64 "d\n"
                     "first_entry  = 0x%016" OB_FMT_64 "x\n",
                     cached_okay ? "true" : "false", target_index, first_index,
                     first_entry);

  // Find backwards search point
  if (cached_okay && target_index < d->cached_index
      && !(target_index < first_index && d->cached_index >= first_index))
    {
      *ending_entry = d->cached_entry;
      *ending_index = d->cached_index;
      goto is_backwards_search_point_okay;
    }

  if (target_index >= first_index)
    {
      *ending_entry = get_newest_entry (d);
      *ending_index = entry_to_index (d, *ending_entry, errp);
is_backwards_search_point_okay:
      if (entry_to_address (d, *ending_entry, errp)
            > entry_to_address (d, first_entry, errp)
          && !is_entry_stompled (d, first_entry, errp))
        return true;
    }

  // In this case, don't allow backwards searching
  *ending_entry = 0;
  *ending_index = -1;
  return true;
}

/// Attempts to use the pool index to find a narrower search window.
/// Returns true if it did narrow the search window.

static bool refine_range (const pool_mmap_data *d, int64 target_index,
                          // input-output parameters
                          int64 *starting_index, unt64 *starting_entry,
                          int64 *ending_index, unt64 *ending_entry,
                          // output parameter
                          bool *found,
                          // input-output parameter
                          ob_retort *errp)
{
  if (already_failed (errp) || !d->ptoc)
    return false;

  pool_toc_entry start = POOL_TOC_NULL_ENTRY, end = POOL_TOC_NULL_ENTRY;
  *found = pool_toc_find_idx (d->ptoc, target_index, &start, &end);
  if (*found && (*ending_index == -1 || *ending_entry >= start.offset)
      && (*starting_index == -1 || start.offset >= *starting_entry)
      && start.idx == entry_to_index (d, start.offset, errp))
    {
      // yes, we refined the starting point
      *starting_entry = start.offset;
      *starting_index = start.idx;
      if (!POOL_TOC_ENTRY_NULL_P (end) && *ending_index != -1
          && end.offset < *ending_entry
          && end.idx == entry_to_index (d, end.offset, errp))
        {
          *ending_entry = end.offset;
          *ending_index = end.idx;
        }
      return true;
    }
  return false;  // no, we did not refine the starting point
}

/// Finds the next available index and entry in the pool, checking
/// for consistency and stompledness, returning @c false in the
/// latter case.

static bool find_next_index (const pool_mmap_data *d, int64 *idx, unt64 *entry,
                             ob_retort *errp)
{
  if (already_failed (errp))
    return false;
  int64 entry_len = entry_size_from_entry_safe (d, *entry, errp);
  if (entry_len == -1 || already_failed (errp))
    // Search entry stompled, start over
    return false;
  *entry += entry_len;
  int64 last_index = *idx;
  *idx = entry_to_index (d, *entry, errp);
  if (*idx == -1 || already_failed (errp))
    // Stompled again
    return false;
  // Sanity check the new index - maybe we got garbage?
  if (*idx != last_index + 1)
    {
      OB_LOG_ERROR_CODE (0x2010401f,
                         "hose '%s' pool '%s': "
                         "Next search index %" OB_FMT_64 "u"
                         " but last search index %" OB_FMT_64 "u\n",
                         hname (d), pname (d), *idx, last_index);
      *errp = POOL_CORRUPT;
      return false;
    }
  return true;
}

/// Finds the previous index and entry in the pool, checking
/// for consistency and stompledness, returning @c false in the
/// latter case.

static bool find_previous_index (const pool_mmap_data *d, int64 *idx,
                                 unt64 *entry, ob_retort *errp)
{
  if (already_failed (errp))
    return false;
  int64 jumpback_len = jumpback_size_from_entry_safe (d, *entry, errp);
  if (jumpback_len == -1 || already_failed (errp))
    return false;
  *entry -= jumpback_len;
  int64 last_index = *idx;
  *idx = entry_to_index (d, *entry, errp);
  if (*idx == -1 || already_failed (errp))
    return false;
  if (*idx != last_index - 1)
    {
      OB_LOG_ERROR_CODE (0x20104020,
                         "hose '%s' pool '%s': "
                         "Searching backwards: index before %" OB_FMT_64
                         "d was %" OB_FMT_64 "d\n",
                         hname (d), pname (d), *idx, last_index);
      *errp = POOL_CORRUPT;
      return false;
    }
  return true;
}

/// Tries to find @a idx in the given range, using the pool index
/// to narrow it, and reverting to a linear search when the index
/// is unable to provide useful information (presumably because
/// it's suffering compactions that invalidate lookup results).
/// Returns @c true if entry was found and @c false if stompledness
/// happened during the search.

static bool find_in_range (pool_mmap_data *d, int64 idx, unt64 *ret_entry,
                           int64 *search_index, unt64 *search_entry,
                           int64 *backwards_index, unt64 *backwards_entry,
                           ob_retort *errp)
{
  bool refined = false, forward = false, seeking = true;
  unt64 success_count = 0, failure_count = 0, count = 0, step = 0;
  while (seeking && !already_failed (errp))
    {
      if (count++ == 0
          || (count > TOO_SMALL_TO_MATTER && !refined && step != 0
              && count % step == 0))
        {
          bool approx_found = false;
          refined =
            refine_range (d, idx, search_index, search_entry, backwards_index,
                          backwards_entry, &approx_found, errp);
          if (approx_found)
            success_count++;
          else
            failure_count++;
          step = pool_toc_step (d->ptoc);
          forward = (*backwards_index < 0
                     || (*backwards_index - idx) > (idx - *search_index));
        }
      // The loop runs as long as our current search entry has
      // not been stompled and the index is not found.
      if (forward)
        seeking = (*search_index > -1 && *search_index < idx
                   && find_next_index (d, search_index, search_entry, errp));
      else
        seeking =
          (*backwards_index > idx
           && find_previous_index (d, backwards_index, backwards_entry, errp));
    }

  if (already_failed (errp))
    return false;

  collect_index_stats (d, success_count, failure_count, count);

  if (idx == *search_index)
    *ret_entry = *search_entry;
  else if (idx == *backwards_index)
    *ret_entry = *backwards_entry;
  else
    return false;

  return true;
}

/// Given the index of an entry, find the offset of the entry in the
/// mmap()ed region.  Can fail if the protein with the index we're
/// looking for has been overwritten.  The index must already have
/// passed basic sanity checks before we get to this point - i.e., not
/// negative, not a future protein, etc.  The pool must be non-empty.

static ob_retort find_entry (pool_mmap_data *d, int64 idx, unt64 *ret_entry)
{
  // The (hopefully) common case: read the very last protein
  // deposited.  entry_to_index () doesn't return garbage
  // indexes, just -1 if the entry has been stompled.
  unt64 search_entry = get_newest_entry (d);
  ob_retort tort = OB_OK;
  int64 search_index = entry_to_index (d, search_entry, &tort);
  if (already_failed (&tort))
    return tort;
  if (idx == search_index)
    {
      // As Napoleon Dynamite would say, "Yesssss!" *pump fist*
      *ret_entry = search_entry;
      return OB_OK;
    }

  // Sigh.  Must trundle through all the proteins in the pool to find
  // the one we want.  We're going to have to start over if the oldest
  // entry passes by our current search entry.  Keep retrying until we
  // either find what we're looking for or the entry we're looking for
  // gets stompled.
  search_index = -1;
  search_entry = 0;
  int64 backwards_index = -1;
  unt64 backwards_entry = 0;
  while (find_starting_point (d, idx, &search_index, &search_entry,
                              &backwards_index, &backwards_entry, &tort))
    {
      OB_LOG_DEBUG_CODE (0x20104042,
                         "idx             = %" OB_FMT_64 "d\n"
                         "search_index    = %" OB_FMT_64 "d\n"
                         "search_entry    = 0x%016" OB_FMT_64 "x\n"
                         "backwards_index = %" OB_FMT_64 "d\n"
                         "backwards_entry = 0x%016" OB_FMT_64 "x\n",
                         idx, search_index, search_entry, backwards_index,
                         backwards_entry);
      if (find_in_range (d, idx, ret_entry, &search_index, &search_entry,
                         &backwards_index, &backwards_entry, &tort)
          || already_failed (&tort))
        // Go home. We don't check whether the entry is still good,
        // because the caller has to check again after copying things
        // out of it, anyway.
        return tort;
    }
  // Drat, the entry we were looking for got stompled.
  ob_err_accum (&tort, POOL_NO_SUCH_PROTEIN);
  return tort;
}

typedef protein (*prot_copy_func) (volatile protein prot, int64 len,
                                   pool_fetch_op *op, ob_retort *errp);

static protein vanilla_prot_copy (volatile protein prot, int64 len,
                                  pool_fetch_op *unused, ob_retort *errp)
{
  if (already_failed (errp))
    return NULL;

  protein new_prot = (protein) malloc (len);
  if (new_prot == NULL)
    {
      *errp = OB_NO_MEM;
      return NULL;
    }

  memcpy (new_prot, prot, len);
  return new_prot;
}

/* Put guard words around the prot8 and slaw8 arrays, so we can detect
 * out-of-bounds reads with valgrind.  (Nothing in the code below should
 * need more than two octs, but let's verify that.) */
struct my_octs
{
  unt64 guard1[2];
  struct _slaw prot8[2];
  unt64 guard2[2];
  struct _slaw slaw8[2];
  unt64 guard3[2];
};

static protein fetch_prot_copy (volatile protein prot, int64 len,
                                pool_fetch_op *op, ob_retort *errp)
{
  if (already_failed (errp))
    return NULL;

  struct my_octs octs;
  OB_INVALIDATE (octs);
  if (len < sizeof (octs.prot8))
    {
      *errp = POOL_CORRUPT;
      return NULL;
    }

  memcpy (&octs.prot8, prot, sizeof (octs.prot8));
  if (SLAW_IS_SWAPPED_PROTEIN (octs.prot8))
    {
      *errp = POOL_UNSUPPORTED_OPERATION; /* fall back to slow path */
      return NULL;
    }

  if (!SLAW_IS_PROTEIN (octs.prot8))
    {
      *errp = POOL_CORRUPT; /* looks like garbage to me */
      return NULL;
    }

  if (PROTEIN_IS_NONSTANDARD (octs.prot8)
      || ((SLAW_PROTEIN_OCT2 (octs.prot8) & SLAW_PROTEIN_FUTURE_FLAG) != 0))
    {
      *errp = POOL_UNSUPPORTED_OPERATION; /* fall back to slow path */
      return NULL;
    }

  const bool has_descrips = PROTEIN_HAS_DESCRIPS (octs.prot8);
  const bool has_ingests = PROTEIN_HAS_INGESTS (octs.prot8);

  opaque_protein_info opi;
  OB_CLEAR (opi);

  const void **ptrs[2];
  int64 *lens[2];
  int64 *bytes[2];
  int64 *nums[2];

  OB_CLEAR (ptrs);
  OB_CLEAR (lens);
  OB_CLEAR (bytes);
  OB_CLEAR (nums);

  int n = 0;
  if (has_descrips)
    {
      ptrs[n] = &opi.descrips;
      lens[n] = &opi.descrips_len;
      bytes[n] = &op->descrip_bytes;
      nums[n++] = &op->num_descrips;
    }
  if (has_ingests)
    {
      ptrs[n] = &opi.ingests;
      lens[n] = &opi.ingests_len;
      bytes[n] = &op->ingest_bytes;
      nums[n++] = &op->num_ingests;
    }

  op->total_bytes = len;
  op->descrip_bytes = op->ingest_bytes = 0;
  op->num_descrips = op->num_ingests = -1;

  const int64 olen = len / 8;

  int i;
  int64 pos = 2;
  for (i = 0; i < n; i++)
    {
      if (pos < 2 || pos >= olen)
        {
          *errp = POOL_CORRUPT; /* would be going out of range */
          return NULL;
        }
      octs.slaw8[0] = prot[pos];
      if (pos + 1 != olen)
        octs.slaw8[1] = prot[pos + 1];
      *(ptrs[i]) = prot + pos;
      *(lens[i]) = *(bytes[i]) = slaw_len (octs.slaw8);
      int64 solen = *(lens[i]) / 8;
      if (pos + solen > olen)
        {
          *errp = POOL_CORRUPT; /* would be going out of range */
          return NULL;
        }
      *(nums[i]) = slaw_list_count (octs.slaw8);
      pos += solen;
    }

  if (PROTEIN_HAS_RUDE (octs.prot8))
    {
      if (PROTEIN_IS_VERY_RUDE (octs.prot8))
        {
          opi.rude_len =
            (SLAW_PROTEIN_OCT2 (octs.prot8) & SLAW_PROTEIN_VERY_RUDE_MASK);
          int64 end = pos + ((opi.rude_len + 7) / 8);
          if (end < 2 || end > olen)
            {
              *errp = POOL_CORRUPT; /* would be going out of range */
              return NULL;
            }
          opi.rude = prot + pos;
        }
      else
        {
          opi.rude_len =
            ((SLAW_PROTEIN_OCT2 (octs.prot8) & SLAW_PROTEIN_WEE_RUDE_MASK)
             >> SLAW_PROTEIN_WEE_RUDE_SHIFTY);
          if (opi.rude_len < 1 || opi.rude_len > 7)
            {
              *errp = POOL_CORRUPT; /* wee rude only legal for 1-7 bytes */
              return NULL;
            }
          opi.rude = SLAW_SPECIAL_BYTES ((octs.prot8 + 1), opi.rude_len);
        }
    }

  op->rude_bytes = opi.rude_len;

  // Adjust opi based on what was requested in op
  if (!op->want_descrips)
    {
      opi.descrips = NULL;
      opi.descrips_len = 0;
    }

  if (!op->want_ingests)
    {
      opi.ingests = NULL;
      opi.ingests_len = 0;
    }

  if (op->rude_offset < 0)
    {
      opi.rude = NULL;
      opi.rude_len = 0;
    }
  else
    {
      opi.rude_len = op->rude_bytes - op->rude_offset;
      if (op->rude_length >= 0 && op->rude_length < opi.rude_len)
        opi.rude_len = op->rude_length;
      if (opi.rude_len < 0 || !opi.rude)
        opi.rude_len = 0;
      if (opi.rude_len > 0)
        opi.rude = ((const unt8 *) opi.rude) + op->rude_offset;
    }

  return protein_from_opaque (&opi);
}

static ob_retort pool_mmap_nth_protein1 (pool_mmap_data *d, int64 idx,
                                         protein *return_prot,
                                         pool_timestamp *ret_ts,
                                         prot_copy_func pcfunc,
                                         pool_fetch_op *arg);

/// Exported mmap implementation of nth_protein().

ob_retort pool_mmap_nth_protein (pool_hose ph, int64 idx, protein *return_prot,
                                 pool_timestamp *ret_ts)
{
  ob_retort tort;
  pool_mmap_data *d = pool_mmap_get_data (ph);

  do
    {
      tort = OB_OK;
      check_for_size_change (d, &tort);
      if (already_failed (&tort))
        break;
      tort = pool_mmap_nth_protein1 (d, idx, return_prot, ret_ts,
                                     vanilla_prot_copy, NULL);
    }
  while (POOL_SIZE_CHANGED == tort);

  return tort;
}

// Exported mmap implementation of fetch().

static ob_retort pool_mmap_fetch (pool_hose ph, pool_fetch_op *ops, int64 nops,
                                  int64 *oldest_idx_out, int64 *newest_idx_out,
                                  bool clamp)
{
  bool again;

  pool_mmap_data *d = pool_mmap_get_data (ph);
  if (get_slaw_version (d) != SLAW_VERSION_CURRENT)
    /* For old slaw formats, fall back to slow path. */
    return POOL_UNSUPPORTED_OPERATION;

  int64 i;
  for (i = 0; ops && i < nops; i++)
    {
      // Get all the outputs into a defined state
      ops[i].tort = OB_UNKNOWN_ERR;
      ops[i].ts = 0;
      ops[i].total_bytes = 0;
      ops[i].descrip_bytes = 0;
      ops[i].ingest_bytes = 0;
      ops[i].rude_bytes = 0;
      ops[i].num_descrips = 0;
      ops[i].num_ingests = 0;
      ops[i].p = NULL;
    }

  ob_retort tort = OB_OK;
  for (i = 0; ops && i < nops && tort != POOL_UNSUPPORTED_OPERATION; i++)
    do
      {
        tort = OB_OK;
        check_for_size_change (d, &tort);
        if (already_failed (&tort))
          goto woe;
        tort = pool_mmap_nth_protein1 (d, ops[i].idx, &ops[i].p, &ops[i].ts,
                                       fetch_prot_copy, &ops[i]);
        ops[i].tort = tort;
        again = (POOL_SIZE_CHANGED == tort);
        if (clamp && POOL_NO_SUCH_PROTEIN == tort)
          {
            int64 idx = -1;
            if (OB_OK == pool_mmap_oldest_index (ph, &idx))
              {
                if (idx > ops[i].idx
                    || (OB_OK == pool_mmap_newest_index (ph, &idx)
                        && idx < ops[i].idx))
                  {
                    ops[i].idx = idx; /* clamp the index to oldest or newest */
                    again = true;     /* and go round again */
                  }
              }
          }
      }
    while (again);

  if (tort != POOL_UNSUPPORTED_OPERATION)
    {
      private_maybe_fill_index (ph, pool_mmap_oldest_index, oldest_idx_out);
      private_maybe_fill_index (ph, pool_mmap_newest_index, newest_idx_out);
      return OB_OK;
    }

woe:
  for (i = 0; ops && i < nops; i++)
    {
      // Splatter retort and free any allocated proteins
      ops[i].tort = tort;
      Free_Protein (ops[i].p);
    }
  if (oldest_idx_out)
    *oldest_idx_out = tort;
  if (newest_idx_out)
    *newest_idx_out = tort;

  return tort;
}

static OB_UNUSED ob_retort pool_mmap_nth_protein1 (pool_mmap_data *d, int64 idx,
                                                   protein *return_prot,
                                                   pool_timestamp *ret_ts,
                                                   prot_copy_func pcfunc,
                                                   pool_fetch_op *arg)
{
  // Do we have any entries at all?
  if (is_pool_empty (d))
    return POOL_NO_SUCH_PROTEIN;

  // Below retries until you get something sane
  ob_retort tort = OB_OK;
  int64 newest_index = get_newest_index (d, &tort);
  int64 oldest_index = get_oldest_index (d, &tort);
  if (already_failed (&tort))
    return tort;

  // XXX I think we want the following two features:
  //
  // index of -1 means the last protein
  // index of -n means relative to the last protein
  //
  // Intuitively, -2 would be the last protein index - 2, but then
  // there's no easy way to express the protein before the last
  // protein, since -1 means the last protein.  For now, just handle
  // -1 and figure out interface issues later.
  // (bug 1290 is currently open on this)
  if (idx == -1)
    idx = newest_index;

  // Is this index still around?  Superfluous, since find_entry will
  // discover this, but nice to do the check up front.
  if (idx < oldest_index ||
      // Is this index in the future?
      idx > newest_index)
    return POOL_NO_SUCH_PROTEIN;

  // We begin by finding the entry of the index we want
  unt64 entry;
  ob_retort pret = find_entry (d, idx, &entry);
  if (pret != OB_OK)
    // Protein was discarded before we could get to it, sigh.
    return pret;

  // Now we have an entry which can get overwritten at any point.  The
  // only way we can make progress is to copy out a piece of
  // information, check whether it was overwritten, and if it wasn't,
  // THEN make use of it.  If we can do this all the way through to
  // the point of making a copy of the whole protein, then we win.
  // Otherwise, the protein got stompled and we have to return
  // discarded protein.

  // Locate the protein
  protein prot = get_protein_from_entry (d, entry, &tort);
  // We'll check if the timestamp has been overwritten shortly
  pool_timestamp ts = timestamp_from_entry (d, entry, &tort);
  const unt64 expected_checksum = checksum_from_entry (d, entry, &tort);
  // Make a copy of the protein - first get the length of it
  int64 len = protein_len_safe (d, prot, entry, &tort);
  if (already_failed (&tort))
    return tort;
  // Did the entry get stompled while we were getting the length?
  if (len == -1)
    return POOL_NO_SUCH_PROTEIN;
  // Sanity check on protein size
  if (len <= 0 || len > MAX_SLAW_SIZE)
    {
      OB_LOG_ERROR_CODE (0x20104051,
                         "hose '%s' pool '%s':\n"
                         "protein length %" OB_FMT_64 "d is\n"
                         "greater than maximum %" OB_FMT_64 "u,\n"
                         "and therefore probably bogus.\n",
                         hname (d), pname (d), len, MAX_SLAW_SIZE);
      return POOL_CORRUPT;
    }
  // Now we can trust the protein length (and timestamp) we read
  protein new_prot = pcfunc (prot, len, arg, &tort);

  // Can we trust the protein we just copied?
  ob_retort stomp_tort = OB_OK;
  const bool stomp = is_entry_stompled (d, entry, &stomp_tort);
  if (stomp || already_failed (&tort) || already_failed (&stomp_tort))
    {
      free (new_prot);
      if (stomp_tort < OB_OK)
        return stomp_tort;
      else if (stomp)
        return POOL_NO_SUCH_PROTEIN;
      else
        return tort;
    }
  const unt64 flags = get_flags (d);
  if (0 != (flags & POOL_FLAG_CHECKSUM))
    {
      const unt64 checksum = compute_entry_checksum (new_prot, len, ts, idx);
      if (checksum != expected_checksum)
        {
          free (new_prot);
          OB_LOG_ERROR_CODE (0x2010404f,
                             "hose '%s' pool '%s':\n"
                             "Expected checksum %016" OB_FMT_64 "x\n"
                             "But got %016" OB_FMT_64 "x\n",
                             hname (d), pname (d), expected_checksum, checksum);
          return POOL_CORRUPT;
        }
    }
  // Woot!
  *return_prot = new_prot;
  if (get_slaw_version (d) != SLAW_VERSION_CURRENT)
    pret = slaw_convert_from (return_prot, SLAW_ENDIAN_UNKNOWN,
                              get_slaw_version (d));
  if (ret_ts)
    *ret_ts = ts;
  // Heuristic: don't cache the position if we're near the beginning
  // or end of the pool, since those are easy to find anyway.  A cached
  // position in the "middle" of the pool is more valuable, so don't
  // lose it.
  if (idx >= oldest_index + TOO_SMALL_TO_MATTER
      && idx <= newest_index - TOO_SMALL_TO_MATTER)
    {
      // Cache the location and index of this protein to speed up next read
      d->cached_entry = entry;
      d->cached_index = idx;
    }
  return pret;
}

/// Unwind state after a failed participate or during a withdraw.

static ob_retort pool_mmap_participate_cleanup (pool_hose ph, ob_retort pret)
{
  pool_mmap_data *d = NULL;
  if ((d = (pool_mmap_data *) ph->ext))
    {
      ob_err_accum (&pret, pool_munmapify (ph));
#ifdef _MSC_VER
      if (d->file)
        {
          if (!CloseHandle (d->file))
            ob_err_accum (&pret, ob_win32err_to_retort (GetLastError ()));
          d->file = 0;
        }
#else
      if (d->file)
        {
          if (fclose (d->file) < 0)
            ob_err_accum (&pret, ob_errno_to_retort (errno));
          d->file = NULL;
        }
#endif

      free (d);
      ph->ext = NULL;
    }

  pool_fifo_multi_destroy_awaiter (ph);
  return pret;
}

static ob_retort pool_mmap_hiatus (pool_hose ph)
{
  return pool_mmap_participate_cleanup (ph, OB_OK);
}

/// Unwind state after failed create.

static ob_retort pool_mmap_create_cleanup (pool_hose ph, ob_retort pret)
{
  pool_mmap_participate_cleanup (ph, pret);
  pool_mmap_destroy_file (ph, false);
  const unt8 mmv =
    pool_mmap_version_from_directory_version (ph->pool_directory_version);
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);
  f.delete_config_file (ph->name);
  return pret;
}

pool_mmap_data *new_pool_mmap_data (pool_hose ph)
{
  pool_mmap_data *d = (pool_mmap_data *) calloc (1, sizeof (pool_mmap_data));
  if (d)
    {
      d->conf_chunk = &(d->legacy_conf);
      d->ph = ph;
      // 0 is a valid index, so set it to -1 to show it's invalid
      d->cached_index = -1;
      // catch any uninitialized reads of the legacy conf
      OB_INVALIDATE (d->legacy_conf);
      d->legacy_conf.next_index = 0;
      d->legacy_conf.flags = 0;
    }
  return d;
}

static void one_flag (bslaw options, unt64 *current, const char *name,
                      unt64 flag)
{
  if (slaw_path_get_bool (options, name, (0 != (flag & *current))))
    *current |= flag;
  else
    *current &= ~flag;
}

typedef enum
{
  NEVER,
  RESIZABLE,
  ALWAYS
} when_supported;

typedef struct
{
  char name[23];
  char flag_char;
  when_supported create;
  when_supported change;
  unt64 flag;
} option_info;

// names must be alphabetized, because we use bsearch()
static const option_info pool_opts[] = {
  {"auto-dispose", 'a', NEVER, RESIZABLE, POOL_FLAG_AUTO_DISPOSE},
  {"checksum", 'c', RESIZABLE, NEVER, POOL_FLAG_CHECKSUM},
  {"flock", 'l', RESIZABLE, NEVER, POOL_FLAG_FLOCK},
  {"frozen", 'f', RESIZABLE, RESIZABLE, POOL_FLAG_FROZEN},
  {"group", 0, ALWAYS, NEVER, 0},
  {"index-capacity", 0, ALWAYS, NEVER, 0},
  {"mode", 0, ALWAYS, NEVER, 0},
  {"owner", 0, ALWAYS, NEVER, 0},
  {"resizable", 0, ALWAYS, NEVER, 0},
  {"single-file", 0, ALWAYS, NEVER, 0},
  {"size", 0, ALWAYS, RESIZABLE, 0},
  {"stop-when-full", 's', RESIZABLE, RESIZABLE, POOL_FLAG_STOP_WHEN_FULL},
  {"sync", 'S', RESIZABLE, RESIZABLE, POOL_FLAG_SYNC},
  {"toc-capacity", 0, ALWAYS, NEVER, 0},
};

#define NUM_OPTIONS (sizeof (pool_opts) / sizeof (pool_opts[0]))

static unt64 flagify (bslaw options, unt64 current)
{
  unt64 flags = current;
  size_t i;
  for (i = 0; i < NUM_OPTIONS; i++)
    if (pool_opts[i].flag)
      one_flag (options, &flags, pool_opts[i].name, pool_opts[i].flag);
  return flags;
}

static void validate_option (const char *oname, const char *pnam,
                             const char *fname, when_supported sup, unt8 mmv)
{
  const char *why;
  switch (sup)
    {
      case NEVER:
        why = "not a valid option";
        break;
      case RESIZABLE:
        if (mmv > 0)
          return;
        why = "only valid for resizable pools";
        break;
      case ALWAYS:
        return;
      default:
        OB_FATAL_BUG_CODE (0x2010404d, "impossible\n");
    }
  OB_LOG_WARNING_CODE (0x2010404e,
                       "For pool '%s':\n"
                       "'%s' is %s in %s\n",
                       pnam, oname, why, fname);
}

typedef int (*bs_func) (const void *, const void *);

static void validate_options (const char *pnam, bslaw map, unt8 mmv,
                              bool create)
{
  bslaw cole;
  for (cole = slaw_list_emit_first (map); cole != NULL;
       cole = slaw_list_emit_next (map, cole))
    {
      const char *oname = slaw_string_emit (slaw_cons_emit_car (cole));
      // XXX: passing strcmp for comparison is slightly evil and depends
      // on the fact that name is the first member of the structure
      // and is NUL-terminated.
      option_info *found = (option_info *) bsearch (
        oname, pool_opts, NUM_OPTIONS, sizeof (pool_opts[0]), (bs_func) strcmp);
      when_supported sup = NEVER;
      if (found)
        sup = (create ? found->create : found->change);
      validate_option (oname, pnam,
                       (create ? "pool_create()" : "pool_change_options()"),
                       sup, mmv);
    }
}

static unt64 round_file_size (unt64 sz)
{
  // round to POOL_SIZE_GRANULARITY
  const unt64 psg = POOL_SIZE_GRANULARITY - 1;
  return ((sz + psg) & ~psg);
}

ob_retort pool_mmap_create (pool_hose ph, const char *type, bprotein options)
{
  unt64 size = mmap_pool_options_file_size (options);
  unt64 toc_capacity = mmap_pool_options_toc_capacity (options);
  const unt8 pdv = ph->pool_directory_version;
  const unt8 mmv = pool_mmap_version_from_directory_version (pdv);
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);

  const unt64 header_size = f.size_of_header (toc_capacity);
  const unt64 min_size = header_size + POOL_MMAP_MIN_SIZE;
  const unt64 max_size = POOL_MMAP_MAX_SIZE;

  validate_options (ph->name, protein_ingests (options), mmv, true);

  if (size < min_size || size > max_size)
    {
      OB_LOG_ERROR_CODE (0x20104022,
                         "invalid size %" OB_FMT_64 "u\n"
                         "must be at least %" OB_FMT_64 "u and no more than "
                         "%" OB_FMT_64 "u\n",
                         size, min_size, max_size);
      return POOL_INVALID_SIZE;
    }

  pool_perms perms = ph->perms;
  // when we create regular files, we don't want them to be executable
  perms.mode = pool_combine_modes (perms.mode, 0666);

  // Allocate type-specific storage
  pool_mmap_data *d = new_pool_mmap_data (ph);
  if (!d)
    return OB_NO_MEM;

  size = round_file_size (size);

  d->mapped_size = d->conf_chunk->file_size = size;
  d->mem = NULL;
  d->file = NULL;
  d->oldnew = NULL;
  d->conf_chunk->header_size = header_size;
  d->ptoc = NULL;
  d->index_success_count = 0;
  d->index_failure_count = 0;
  d->index_failure_rate = 0;

  ph->ext = d;
  ob_retort pret = pool_mmap_create_file (ph, perms.mode, perms.uid, perms.gid);

  if (OB_OK == pret)
    pret = pool_mmapify (ph);

  if (pret == OB_OK)
    {
      // We keep the header at the beginning of the file
      d->oldnew = (pool_mmap_oldnew *) d->mem;

      f.initialize_header (SLAW_VERSION_CURRENT, d->mem, toc_capacity);
      /* Note for the confused, which includes my future self:
       * The following line is where d->conf_chunk gets set to point
       * to the mmap header for v1 files.  Before now, it was pointing
       * at legacy_conf in order to bootstrap v1 files, and it will
       * continue pointing at legacy_conf for v0 files, since they
       * don't have a conf chunk in their header.  Yes, this is
       * a bit convoluted, and I probably should have done it some
       * other way that's clearer, although I'm not sure what that is. */
      pret = f.read_header (d);
      if (header_size != get_header_size (d))
        OB_FATAL_BUG_CODE (0x20104037, "%" OB_FMT_64 "u != %" OB_FMT_64 "u\n",
                           header_size, get_header_size (d));
      d->conf_chunk->file_size = size;
      d->conf_chunk->sem_key = ph->sem_key;
      // Only support flags for "new" pools,
      // and don't allow auto-dispose, since it doesn't make sense here.
      if (mmv > 0)
        {
          d->conf_chunk->flags = flagify (options, POOL_DEFAULT_FLAGS);
          if (POOL_FLAG_AUTO_DISPOSE & d->conf_chunk->flags)
            {
              d->conf_chunk->flags &= ~POOL_FLAG_AUTO_DISPOSE;
              OB_LOG_WARNING_CODE (0x2010404c,
                                   "auto-dispose doesn't make sense when "
                                   "creating a pool,\n"
                                   "since auto-dispose means dispose when "
                                   "there are no open\n"
                                   "hoses, and there are no open hoses when "
                                   "the pool is created!\n(pool: %s)\n",
                                   ph->name);
            }
        }
      if (d->perm_chunk)
        {
          // Use "ph->perms" here instead of "perms" because we want
          // to remember the original mode.
          d->perm_chunk->mode = ph->perms.mode;
          d->perm_chunk->uid = ph->perms.uid;
          d->perm_chunk->gid = ph->perms.gid;
        }
      // Initialize the offsets.  Offsets are relative to d->mem.  We
      // signify an empty pool with newest == 0, and a temporarily empty
      // pool with newest < oldest.
      set_oldest_entry (d, POOL_MMAP_PROTEINS_START_OFFSET (d), &pret);
      primitive_set_newest_entry (d, 0);
      if (OB_OK == pret)
        {
          // Write out our configuration data
          pret = f.write_config_file (ph->name, perms, get_file_size (d),
                                      get_header_size (d), toc_capacity);
        }
    }
  // pool_mmap_participate_cleanup shuts down all the state we built up
  return pret == OB_OK ? pool_mmap_participate_cleanup (ph, pret)
                       : pool_mmap_create_cleanup (ph, pret);
}

static void set_lock_ops_from_flags (pool_hose ph, const pool_mmap_data *d)
{
#ifndef _MSC_VER
  const unt64 flags = get_flags (d);
  if (0 == (flags & POOL_FLAG_FLOCK))
    ph->lock_ops = &pool_sem_ops;
  else
    ph->lock_ops = &pool_flock_ops;
#endif
}

/// Load mmap-specific configuration data.

ob_retort pool_mmap_load_config (pool_hose ph)
{
  if (ph->ext)
    return OB_OK; /* already loaded */

  pool_mmap_data *d = new_pool_mmap_data (ph);
  if (!d)
    return OB_NO_MEM;

  // This is another one of those cases where the pool code is way too
  // general.  Make no mistake about it-- we will only be called
  // with ph->method being "mmap".  (Hey, we are the mmap pool code,
  // after all!)
  assert (0 == strcmp ("mmap", ph->method));

  const unt8 pdv = ph->pool_directory_version;
  const unt8 mmv = pool_mmap_version_from_directory_version (pdv);
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);
  ob_retort pret = f.bootstrap (d);
  if (OB_OK == pret)
    {
      if (d->conf_chunk->file_size < POOL_MMAP_MIN_SIZE
          || d->conf_chunk->file_size < d->conf_chunk->header_size
          || d->conf_chunk->file_size > POOL_MMAP_MAX_SIZE)
        {
          OB_LOG_ERROR_CODE (0x20104023, "invalid size %" OB_FMT_64 "u\n",
                             d->conf_chunk->file_size);
          pret = POOL_INVALID_SIZE;
        }
      else
        {
          ph->ext = d;
          set_lock_ops_from_flags (ph, d);
        }
    }

  if (pret != OB_OK)
    free (d);

  return pret;
}

ob_retort pool_mmap_participate (pool_hose ph)
{
  ob_retort pret;
  if ((pret = pool_mmap_load_config (ph)) != OB_OK)
    return pool_mmap_participate_cleanup (ph, pret);
  if ((pret = pool_mmap_open_file (ph)) != OB_OK)
    return pool_mmap_participate_cleanup (ph, pret);

  pool_mmap_data *d = pool_mmap_get_data (ph);
  d->mapped_size = d->conf_chunk->file_size;

  if ((pret = pool_mmapify (ph)) != OB_OK)
    return pool_mmap_participate_cleanup (ph, pret);

  const unt8 pdv = ph->pool_directory_version;
  const unt8 mmv = pool_mmap_version_from_directory_version (pdv);
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);

  pret = f.read_header (d);
  if (pret < OB_OK)
    return pool_mmap_participate_cleanup (ph, pret);

  if (d->perm_chunk)
    {
      ph->perms.mode = d->perm_chunk->mode;
      ph->perms.uid = d->perm_chunk->uid;
      ph->perms.gid = d->perm_chunk->gid;
    }

  const slaw_vfuncs *vf = get_vfuncs (get_slaw_version (d));
  if (!vf)
    return pool_mmap_participate_cleanup (ph, SLAW_WRONG_VERSION);
  d->vprotein_len = vf->vprotein_len;

  if (d->ptoc && 0 == pool_toc_step (d->ptoc))
    {
      OB_LOG_ERROR_CODE (0x20104038, "step is 0; this is not legal\n");
      return pool_mmap_participate_cleanup (ph, POOL_CORRUPT);
    }

  if (d->ptoc && 0 == pool_toc_capacity (d->ptoc))
    {
      OB_LOG_ERROR_CODE (0x20104039, "capacity is 0; this is not legal\n");
      return pool_mmap_participate_cleanup (ph, POOL_CORRUPT);
    }

  const unt64 flags = get_flags (d);
  if (0
      != (OB_CONST_U64 (0xffffffff) & flags
          & ~(POOL_FLAG_STOP_WHEN_FULL | POOL_FLAG_FROZEN
              | POOL_FLAG_AUTO_DISPOSE | POOL_FLAG_CHECKSUM | POOL_FLAG_FLOCK)))
    {
      // If any of the 0-31 bits are set and we don't recognize them,
      // it's an error.  (It's okay for 32-63 to be unrecognized; we'll
      // ignore them.)
      // Technically, it's not sufficient to check these at participate
      // time, since they could change at any time.  But this is at least
      // a helpful sanity check.
      int i;
      for (i = 0; i < 32; i++)
        {
          if (0 != (1 & (flags >> i)))
            OB_LOG_ERROR_CODE (0x2010403b,
                               "For pool '%s',\n"
                               "mmap flag %d is not supported\n",
                               ph->name, i);
        }
      // Well, it's really more like "unsupported feature", but "wrong
      // version" is close enough.
      return pool_mmap_participate_cleanup (ph, POOL_WRONG_VERSION);
    }

  // This used to be in __pool_participate() in pool.c, but it had
  // to move here because we needed to "sandwich" it between reading
  // the sem key (so pool_open_semaphores could use it) and updating
  // the sem key (in case pool_open_semaphores changed it).
  if (pdv != POOL_DIRECTORY_VERSION_CONFIG_IN_FILE)
    ph->sem_key = d->conf_chunk->sem_key;
  pret = pool_open_semaphores (ph);
  if (pret >= OB_OK)
    d->conf_chunk->sem_key = ph->sem_key;
  else
    return pool_mmap_participate_cleanup (ph, pret);

  pret =
    pool_load_multi_await (ph, ph->perms.mode, ph->perms.uid, ph->perms.gid);
  if (pret < OB_OK)
    return pool_mmap_participate_cleanup (ph, pret);

  return pret;
}

static void last_withdraw_housekeeping (pool_hose ph, ob_retort *errp)
{
  if (already_failed (errp)
      || ph->pool_directory_version == POOL_DIRECTORY_VERSION_CONFIG_IN_FILE)
    return;

  pool_mmap_data *d = pool_mmap_get_data (ph);
#ifndef _MSC_VER
  // Convert our lock to an exclusive lock.  If we can't, that means
  // we are not the last one to withdraw, so we have nothing else to do.
  // It will be unlocked when we close the backing file, which is soon.
  while (flock (fileno (d->file), LOCK_EX | LOCK_NB) < 0)
    {
      const int erryes = errno;
      if (EINTR != erryes)
        {
          if (EWOULDBLOCK != erryes)
            *errp = ob_errno_to_retort (erryes);
          return;
        }
    }
#endif

  const unt64 flags = get_flags (d);
  unt64 real_file_size = get_real_file_size (d, errp);
  if (already_failed (errp))
    return;
  unt64 alleged_size = get_file_size (d);
  *errp = pool_munmapify (ph);
  if (real_file_size != alleged_size)
    {
      change_file_size (d, alleged_size, errp);
#ifdef _MSC_VER
      if (*errp == ob_win32err_to_retort (ERROR_USER_MAPPED_FILE))
        *errp = OB_OK;
#endif
    }
  if (!already_failed (errp) && 0 != (flags & POOL_FLAG_AUTO_DISPOSE))
    *errp = POOL_DELETE_ME;
}

ob_retort pool_mmap_withdraw (pool_hose ph)
{
  report_index_stats (pool_mmap_get_data (ph));
  ob_retort tort = OB_OK;
  last_withdraw_housekeeping (ph, &tort);
  return pool_mmap_participate_cleanup (ph, tort);
}

static ob_retort pool_mmap_get_semkey (pool_hose ph)
{
  const unt8 pool_directory_version = ph->pool_directory_version;
  const unt8 mmv =
    pool_mmap_version_from_directory_version (pool_directory_version);
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);
  ob_retort tort = OB_OK;

  if (pool_directory_version != POOL_DIRECTORY_VERSION_CONFIG_IN_FILE)
    {
      // If the configuration (specifically, the semaphore key) is
      // being stored in the backing file, then only we (the mmap code)
      // know how to get it out.  So we have to set ph->sem_key here,
      // so the generic pool code knows what it is and can destroy it.
      // Since we don't have the backing file mmaped at this point,
      // the easiest way to find out the sem key is with the bootstrap
      // function.
      pool_mmap_data *d = new_pool_mmap_data (ph);
      if (!d)
        return OB_NO_MEM;
      tort = f.bootstrap (d);
      if (tort >= OB_OK)
        {
          set_lock_ops_from_flags (ph, d);
          ph->sem_key = d->conf_chunk->sem_key;
        }
      free (d);
    }

  return tort;
}

ob_retort pool_mmap_dispose (pool_hose ph)
{
  const char *poolName = ph->name;
  const unt8 pool_directory_version = ph->pool_directory_version;
  const unt8 mmv =
    pool_mmap_version_from_directory_version (pool_directory_version);
  const mmap_version_funcs f = pool_mmap_get_version_funcs (mmv);

  ob_retort pret = pool_mmap_get_semkey (ph);
  if (pret < OB_OK)
    OB_LOG_WARNING_CODE (0x20104053,
                         "Unable to get semaphore key for '%s'\ndue to '%s'.\n"
                         "I am going to delete the pool anyway, but it may\n"
                         "leak a semaphore.  See bug 7309 for the rationale\n"
                         "of why we are doing this.\n",
                         poolName, ob_error_string (pret));

  if ((pret = pool_mmap_destroy_file (ph, true)) != OB_OK)
    return pret;

  return f.delete_config_file (poolName);
}

/// Number of bytes currently in use by proteins in the pool.  (Bug 367)
static unt64 pool_mmap_size_used (const pool_mmap_data *d, ob_retort *errp)
{
  unt64 newest;
  unt64 oldest;
  int64 size_of_newest;
  do
    {
      if (already_failed (errp))
        return 0;
      newest = primitive_get_newest_entry (d);
      oldest = primitive_get_oldest_entry (d);
      if (newest == 0 ||    // truly empty (never had any deposits)
          newest < oldest)  // temporarily empty
        return 0;
      size_of_newest = entry_size_from_entry_safe (d, newest, errp);
    }
  while (size_of_newest < 0);  // occurs when stompled
  return (newest - oldest) + size_of_newest;
}

static slaw map_from_flags (unt64 flags)
{
  size_t i;
  char flag_str[1 + NUM_OPTIONS];
  char *p = flag_str;
  slabu *sb = slabu_new ();
  if (!sb)
    return NULL;
  OB_CLEAR (flag_str);
  for (i = 0; i < NUM_OPTIONS; i++)
    if (pool_opts[i].flag)
      {
        bool b = (0 != (flags & pool_opts[i].flag));
        if (slabu_map_put_cf (sb, pool_opts[i].name, slaw_boolean (b)) < 0)
          {
            slabu_free (sb);
            return NULL;
          }
        if (b)
          *p++ = pool_opts[i].flag_char;
      }
  if (slabu_map_put_cf (sb, "flags", slaw_string (flag_str)) < 0)
    {
      slabu_free (sb);
      return NULL;
    }
  return slaw_map_f (sb);
}

ob_retort pool_mmap_info (pool_hose ph, int64 hops, protein *return_prot)
{
  const pool_mmap_data *d = pool_mmap_get_data (ph);
  ob_retort tort = OB_OK;
  unt64 size_used = pool_mmap_size_used (d, &tort);
  if (already_failed (&tort))
    return tort;
  const unt64 flags = get_flags (d);
  const unt64 toc_capacity = pool_toc_capacity (d->ptoc);
  const unt64 toc_step = pool_toc_step (d->ptoc);
  const unt64 toc_count = pool_toc_count (d->ptoc);
  slaw flagslaw = map_from_flags (flags);
  slaw ingests = slaw_map_inline_cf (
    "type", slaw_string ("mmap"), "terminal", slaw_boolean (true), "size",
    slaw_unt64 (get_file_size (d)), "size-used", slaw_unt64 (size_used),
    "mmap-pool-version", slaw_unt32 (get_mmap_version (d)), "slaw-version",
    slaw_unt32 (get_slaw_version (d)),
    // for backwards compatibility
    "index-capacity", slaw_unt64 (toc_capacity), "index-step",
    slaw_unt64 (toc_step), "index-count", slaw_unt64 (toc_count),
    // these new names are preferred
    "toc-capacity", slaw_unt64 (toc_capacity), "toc-step",
    slaw_unt64 (toc_step), "toc-count", slaw_unt64 (toc_count), NULL);
  if (!ingests || !flagslaw)
    {
      slaw_free (ingests);
      slaw_free (flagslaw);
      return OB_NO_MEM;
    }
  ingests = slaw_maps_merge_f (ingests, flagslaw, NULL);
  if (!ingests)
    return OB_NO_MEM;
  *return_prot = protein_from_ff (NULL, ingests);
  if (!*return_prot)
    return OB_NO_MEM;
  return OB_OK;
}

// Find the smallest x such that:
// x > old
// x % modulo == offset
static unt64 remodulate (unt64 old, unt64 offset, unt64 modulo)
{
  assert (offset < modulo);
  unt64 z = (old % modulo);
  unt64 y = (offset + modulo) - z;
  y %= modulo;
  if (y == 0)
    y += modulo;
  unt64 x = y + old;
  assert (x > old);
  assert (x % modulo == offset);
  assert (x < modulo || (x - modulo) <= old);
  return x;
}

// In a style vaguely reminiscent of functional programming, we build up
// a description of what we're going to do, and *then* we do it.
// When the resize is complete, the following should hold true:
// new_newest > new_oldest > old_newest > old_oldest
typedef struct
{
  unt64 new_oldest_entry;
  unt64 new_newest_entry;
  unt64 new_size;
  unt64 old_size;
  // either all three of these are valid, or all three are 0
  // addresses are given as offsets into the backing file
  unt64 memmove_src;
  unt64 memmove_dst;
  unt64 memmove_size;
} how_to_resize_it;

static void print_how2 (how_to_resize_it how2)
{
  OB_LOG_DEBUG_CODE (0x2010403e,
                     "new_oldest_entry = 0x%016" OB_FMT_64 "x\n"
                     "new_newest_entry = 0x%016" OB_FMT_64 "x\n"
                     "new_size         = 0x%016" OB_FMT_64 "x\n"
                     "old_size         = 0x%016" OB_FMT_64 "x\n"
                     "memmove_src      = 0x%016" OB_FMT_64 "x\n"
                     "memmove_dst      = 0x%016" OB_FMT_64 "x\n"
                     "memmove_size     = 0x%016" OB_FMT_64 "x\n",
                     how2.new_oldest_entry, how2.new_newest_entry,
                     how2.new_size, how2.old_size, how2.memmove_src,
                     how2.memmove_dst, how2.memmove_size);
}

static void rebuild_toc (pool_mmap_data *d, unt64 oldest_entry,
                         unt64 newest_entry, ob_retort *errp)
{
  if (already_failed (errp) || !d->ptoc)
    return;

  // clear the pool's table of contents, so we can start over
  pool_toc_init ((byte *) d->ptoc, pool_toc_capacity (d->ptoc));

  if (newest_entry == 0)
    // This means the pool will be empty, so nothing more to do
    return;

  const unt64 file_size = get_file_size (d);
  const unt64 oldest_offset = oldest_entry % file_size;
  const unt64 hdr_size = get_header_size (d);
  unt64 first_entry;
  if (oldest_offset < hdr_size)
    {
      OB_LOG_ERROR_CODE (0x2010404b,
                         "POOL_CORRUPT: %" OB_FMT_64 "u < %" OB_FMT_64 "u\n",
                         oldest_offset, hdr_size);
      *errp = POOL_CORRUPT;
      return;
    }
  else if (oldest_offset == hdr_size)
    first_entry = oldest_entry;
  else
    first_entry = remodulate (oldest_entry, hdr_size, file_size);

  const byte *first = hdr_size + d->mem;
  const unt64 first_idx = *(const unt64 *) (first + POOL_MMAP_INDEX_OFFSET);

  unt64 entry;
  for (entry = oldest_entry; entry <= newest_entry;)
    {
      const byte *p = entry_to_address (d, entry, errp);
      if (already_failed (errp))
        return;
      pool_toc_entry pie;
      pie.offset = entry;
      pie.stamp = *(const float64 *) (p + POOL_MMAP_TIMESTAMP_OFFSET);
      pie.idx = *(const unt64 *) (p + POOL_MMAP_INDEX_OFFSET);
      pool_toc_append (d->ptoc, pie, oldest_entry);
      if (pie.idx + 1 == first_idx)
        entry = first_entry;
      else
        entry += entry_size_from_entry (d, entry, errp);
    }
}

static void do_resize_it (pool_mmap_data *d, how_to_resize_it how2,
                          ob_retort *errp)
{
  if (already_failed (errp))
    return;

  // While we are resizing the pool, we don't want readers to look at it.
  // Therefore, we mimic the "temporarily empty" condition, which causes
  // readers to busy-wait (not the greatest, but should be okay, especially
  // on multi-core machines).
  //
  // "temporarily empty" is indicated by newest < oldest
  //
  // the resize has the following invariant:
  // new_newest > new_oldest > old_newest > old_oldest
  //
  // Therefore, if we set oldest = new_oldest before we resize, and set
  // newest = new_newest after we resize, we will appear to be temporarily
  // empty while we resize.  These transitions happen atomically, and then
  // we can do pretty much whatever we want in between.

  // Step 1: move oldest (start being temporarily empty)
  primitive_set_oldest_entry (d, how2.new_oldest_entry);

  // Step 2: physically resize now if we are making it bigger
  assert (how2.new_size != how2.old_size);
  if (how2.new_size > how2.old_size)
    physically_resize (d, how2.new_size, errp);

  // Step 3: move the data around
  if (how2.memmove_size && !already_failed (errp))
    memmove (how2.memmove_dst + (byte *) d->mem,
             how2.memmove_src + (byte *) d->mem, how2.memmove_size);

  // Step 4: physically resize now if we are making it smaller
  if (how2.new_size < how2.old_size)
    physically_resize (d, how2.new_size, errp);

  // Step 5: rebuild the table of contents
  rebuild_toc (d, how2.new_oldest_entry, how2.new_newest_entry, errp);

  if (already_failed (errp))
    return;

  // Step 6: move newest (done being temporarily empty)
  if (how2.new_newest_entry == 0)
    primitive_set_newest_entry (d, how2.new_newest_entry);
  else
    {
      set_newest_entry (d, how2.new_newest_entry, errp);
      check_oldest_newest_diff (d, errp);
    }
}

static unt64 get_last_valid_entry (pool_mmap_data *d, unt64 first,
                                   ob_retort *errp)
{
  const int64 first_index = entry_to_index (d, first, errp);
  if (already_failed (errp))
    return 0;
  if (first_index > 0)
    {
      unt64 last_entry = 0;
      ob_retort tort = find_entry (d, first_index - 1, &last_entry);
      if (tort >= OB_OK)
        return last_entry;
      else if (tort != POOL_NO_SUCH_PROTEIN)
        {
          *errp = tort;
          return 0;
        }
    }
  // This means the pool has not wrapped around.  Therefore, the
  // last entry is also the newest entry
  return get_newest_entry (d);
}

static unt64 round_down_to_entry (const pool_mmap_data *d, unt64 seg_start,
                                  unt64 seg_len, unt64 available,
                                  ob_retort *errp)
{
  // XXX: eventually use index to speed this up, but for now,
  // let's do it the obvious way
  while (seg_len > available && !already_failed (errp))
    {
      int64 len = entry_size_from_entry (d, seg_start, errp);
      seg_start += len;
      seg_len -= len;
    }
  return seg_len;
}

static how_to_resize_it figure_out_how (pool_mmap_data *d, unt64 new_size,
                                        ob_retort *errp)
{
  how_to_resize_it how2;
  OB_CLEAR (how2);

  if (already_failed (errp))
    return how2;
  const unt64 old_oldest = primitive_get_oldest_entry (d);
  const unt64 old_newest = primitive_get_newest_entry (d);
  const unt64 hdr_size = get_header_size (d);
  how2.new_size = new_size;

  if (0 == old_newest)  // empty pool
    {
      how2.new_oldest_entry = remodulate (old_oldest, hdr_size, new_size);
      how2.new_newest_entry = 0;
      return how2;
    }

  const unt64 old_first = get_first_valid_entry (d, errp);
  const unt64 old_last = get_last_valid_entry (d, old_first, errp);
  const int64 size_last = entry_size_from_entry (d, old_last, errp);
  const int64 size_newest = entry_size_from_entry (d, old_newest, errp);
  if (already_failed (errp))
    return how2;
  const unt64 seg1_start = old_first;
  const unt64 seg1_len = old_newest - old_first + size_newest;
  const unt64 seg2_start = old_oldest;
  const unt64 seg2_len = old_last - old_oldest + size_last;
  const bool noseg2 = (seg1_start == seg2_start && seg1_len == seg2_len);
  how2.old_size = get_file_size (d);
  const unt64 seg1_off = seg1_start % how2.old_size;
  const unt64 seg2_off = seg2_start % how2.old_size;
  OB_LOG_DEBUG_CODE (0x20104044,
                     "old_first   = 0x%016" OB_FMT_64 "x\n"
                     "old_last    = 0x%016" OB_FMT_64 "x\n"
                     "size_last   = 0x%016" OB_FMT_64 "x\n"
                     "old_oldest  = 0x%016" OB_FMT_64 "x\n"
                     "old_newest  = 0x%016" OB_FMT_64 "x\n"
                     "size_newest = 0x%016" OB_FMT_64 "x\n"
                     "seg1_start  = 0x%016" OB_FMT_64 "x\n"
                     "seg1_len    = 0x%016" OB_FMT_64 "x\n"
                     "seg2_start  = 0x%016" OB_FMT_64 "x\n"
                     "seg2_len    = 0x%016" OB_FMT_64 "x\n"
                     "hdr_size    = 0x%016" OB_FMT_64 "x\n"
                     "seg1_off    = 0x%016" OB_FMT_64 "x\n"
                     "seg2_off    = 0x%016" OB_FMT_64 "x\n",
                     old_first, old_last, size_last, old_oldest, old_newest,
                     size_newest, seg1_start, seg1_len, seg2_start, seg2_len,
                     hdr_size, seg1_off, seg2_off);
  if (!noseg2
      && !(seg1_start > seg2_start + seg2_len
           && seg2_off >= seg1_off + seg1_len))
    {
      *errp = POOL_CORRUPT;
      OB_LOG_ERROR_CODE (0x2010403c,
                         "Corruption detected in '%s':\n"
                         "file size = %" OB_FMT_64 "u, hdr size =  %" OB_FMT_64
                         "u\n"
                         "seg1_start = %" OB_FMT_64 "u, seg1_off = %" OB_FMT_64
                         "u, seg1_len = %" OB_FMT_64 "u\n"
                         "seg2_start = %" OB_FMT_64 "u, seg2_off = %" OB_FMT_64
                         "u, seg2_len = %" OB_FMT_64 "u\n",
                         pname (d), how2.old_size, hdr_size, seg1_start,
                         seg1_off, seg1_len, seg2_start, seg2_off, seg2_len);
      return how2;
    }
  if (!noseg2 && hdr_size != seg1_off)
    {
      *errp = POOL_CORRUPT;
      OB_LOG_ERROR_CODE (0x2010403d,
                         "Corruption detected in '%s':\n"
                         "file size = %" OB_FMT_64 "u, hdr size =  %" OB_FMT_64
                         "u\n"
                         "seg1_start = %" OB_FMT_64 "u, seg1_off = %" OB_FMT_64
                         "u, seg1_len = %" OB_FMT_64 "u\n"
                         "seg2_start = %" OB_FMT_64 "u, seg2_off = %" OB_FMT_64
                         "u, seg2_len = %" OB_FMT_64 "u\n",
                         pname (d), how2.old_size, hdr_size, seg1_start,
                         seg1_off, seg1_len, seg2_start, seg2_off, seg2_len);
      return how2;
    }
  if (new_size <= hdr_size || new_size < POOL_MMAP_MIN_SIZE
      || new_size > POOL_MMAP_MAX_SIZE)
    {
      *errp = POOL_INVALID_SIZE;
      return how2;
    }
  const unt64 new_available = new_size - hdr_size;
  OB_LOG_DEBUG_CODE (0x20104041,
                     "seg1_len      = 0x%016" OB_FMT_64 "x\n"
                     "new_available = 0x%016" OB_FMT_64 "x\n"
                     "noseg2        = %s\n",
                     seg1_len, new_available, noseg2 ? "true" : "false");
  if (seg1_len > new_available || noseg2)
    {
      // move seg1
      const unt64 new_seg1_off = hdr_size;
      const unt64 new_seg1_len =
        (seg1_len < new_available
           ? seg1_len
           : round_down_to_entry (d, seg1_start, seg1_len, new_available,
                                  errp));
      if (already_failed (errp))
        return how2;
      const unt64 seg1_src = seg1_off + seg1_len - new_seg1_len;
      // If new_seg1_len is 0, the pool is empty.  Can we handle that?
      if (new_seg1_len == 0)
        {
          how2.new_oldest_entry = remodulate (old_newest, hdr_size, new_size);
          how2.new_newest_entry = 0;
          return how2;
        }
      OB_LOG_DEBUG_CODE (0x2010403f,
                         "seg1_src     = 0x%016" OB_FMT_64 "x\n"
                         "new_seg1_off = 0x%016" OB_FMT_64 "x\n"
                         "new_seg1_len = 0x%016" OB_FMT_64 "x\n",
                         seg1_src, new_seg1_off, new_seg1_len);
      how2.memmove_src = seg1_src;
      how2.memmove_dst = new_seg1_off;
      how2.memmove_size = new_seg1_len;
      how2.new_oldest_entry = remodulate (old_newest, new_seg1_off, new_size);
      if (new_seg1_len == size_newest)
        how2.new_newest_entry = how2.new_oldest_entry;
      else
        how2.new_newest_entry =
          remodulate (how2.new_oldest_entry,
                      new_seg1_off + new_seg1_len - size_newest, new_size);
    }
  else
    {
      // move seg2
      const unt64 remaining = new_available - seg1_len;
      const unt64 new_seg2_len =
        (seg2_len < remaining
           ? seg2_len
           : round_down_to_entry (d, seg2_start, seg2_len, remaining, errp));
      if (already_failed (errp))
        return how2;
      const unt64 seg2_src = seg2_off + seg2_len - new_seg2_len;
      const unt64 new_seg2_off = new_size - new_seg2_len;
      OB_LOG_DEBUG_CODE (0x20104040,
                         "seg2_src     = 0x%016" OB_FMT_64 "x\n"
                         "new_seg2_off = 0x%016" OB_FMT_64 "x\n"
                         "new_seg2_len = 0x%016" OB_FMT_64 "x\n",
                         seg2_src, new_seg2_off, new_seg2_len);
      how2.memmove_src = seg2_src;
      how2.memmove_dst = new_seg2_off;
      how2.memmove_size = new_seg2_len;
      if (new_seg2_len == 0)
        how2.new_oldest_entry =
          remodulate (old_newest, old_oldest % how2.old_size, new_size);
      else
        how2.new_oldest_entry = remodulate (old_newest, new_seg2_off, new_size);
      how2.new_newest_entry = remodulate (how2.new_oldest_entry,
                                          old_newest % how2.old_size, new_size);
    }

  if (how2.memmove_src == how2.memmove_dst || how2.memmove_size == 0)
    {
      how2.memmove_src = how2.memmove_dst = how2.memmove_size = 0;
    }

  return how2;
}

static void resize_pool (pool_mmap_data *d, unt64 new_size, ob_retort *errp)
{
  if (already_failed (errp))
    return;

  if (get_mmap_version (d) < 1)
    {
      // old pools can't be resized
      *errp = POOL_UNSUPPORTED_OPERATION;
      return;
    }

  const how_to_resize_it how2 = figure_out_how (d, new_size, errp);
  if (already_failed (errp))
    return;
  if (how2.new_size == how2.old_size)
    {
      *errp = OB_NOTHING_TO_DO;
      return;
    }
  print_how2 (how2);
  do_resize_it (d, how2, errp);
}

static ob_retort pool_mmap_resize (pool_hose ph, unt64 new_size)
{
  ob_retort tort;
  tort = pool_deposit_lock (ph);
  if (tort < OB_OK)
    return tort;
  resize_pool (pool_mmap_get_data (ph), new_size, &tort);
  ob_err_accum (&tort, pool_deposit_unlock (ph));
  return tort;
}

static ob_retort pool_mmap_change_options (pool_hose ph, bslaw options)
{
  ob_retort tort = OB_OK;

  pool_mmap_data *d = pool_mmap_get_data (ph);
  validate_options (ph->name, protein_ingests (options), get_mmap_version (d),
                    false);

  const unt64 new_size =
    round_file_size (mmap_pool_options_file_size (options));

  if (new_size > 0)
    ob_err_accum (&tort, pool_mmap_resize (ph, new_size));

  // Only support this for "new" pools
  // question: should we hold deposit lock to be safe?
  if (get_mmap_version (d) > 0)
    {
      unt64 old_flags, new_flags;
      do
        {
          old_flags = get_flags (d);
          new_flags = flagify (options, old_flags);
        }
      while (!ob_atomic_int64_compare_and_swap (&d->conf_chunk->flags,
                                                old_flags, new_flags));
    }

  return tort;
}

/// Fill out mmap pool methods.
ob_retort pool_mmap_load_methods (pool_hose ph)
{
  //
  // The below can get much fancier if necessary, e.g.:
  //
  // if (ph->blocking_write)
  //   ph->deposit = pool_mmap_blocking_deposit
  // else
  //   ph->deposit = pool_mmap_deposit;

  ph->create = pool_mmap_create;
  ph->participate = pool_mmap_participate;
  ph->deposit = pool_mmap_deposit;
  ph->nth_protein = pool_mmap_nth_protein;
  ph->newest_index = pool_mmap_newest_index;
  ph->oldest_index = pool_mmap_oldest_index;
  ph->index_lookup = pool_mmap_index_lookup;
  ph->withdraw = pool_mmap_withdraw;
  ph->dispose = pool_mmap_dispose;
  ph->info = pool_mmap_info;
  ph->advance_oldest = pool_mmap_advance_oldest;
  ph->change_options = pool_mmap_change_options;
  ph->hiatus = pool_mmap_hiatus;
  ph->fetch = pool_mmap_fetch;

  // Use fifo-based await support
  ph->await_next_single = pool_fifo_await_next_single;
  // Use fifo-based multi-pool await support
  ph->multi_add_awaiter = pool_fifo_multi_add_awaiter;
  ph->multi_remove_awaiter = pool_fifo_multi_remove_awaiter;
  // Don't need our own participate_creatingly()
  ph->participate_creatingly = NULL;

  ph->get_semkey = pool_mmap_get_semkey;

  return OB_OK;
}
