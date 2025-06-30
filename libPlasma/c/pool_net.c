
/* (c)  oblong industries */

//
// Pack and unpack pool operations for transport over the network.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include "plasma_config.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-endian.h"
#include "libLoam/c/ob-log.h"

#include "libPlasma/c/pool.h"
#include "libPlasma/c/slaw-path.h"
#include "libPlasma/c/private/pool_impl.h"
#include "libPlasma/c/private/plasma-private.h"
#include "libPlasma/c/private/plasma-util.h"
#include "libPlasma/c/protein.h"

static const struct _slaw NIL_SINGLETON[1] = {{SLAW_NIL_ILK}};

static void set_outstanding (outstanding_await_t *dst,
                             outstanding_await_e status, int64 idx,
                             bslaw pattern)
{
  if (pattern != dst->pattern)
    {
      slaw_free (dst->pattern);
      dst->pattern = slaw_dup (pattern);
      if (pattern && !dst->pattern)
        // XXX: a naughty way to handle the error, but I'm lazy today
        OB_FATAL_ERROR_CODE (0x20106017, "memory allocation failed\n");
    }
  dst->status = status;
  dst->idx = idx;
}

void pool_net_free (pool_net_data *freeme)
{
  if (freeme)
    {
      set_outstanding (&freeme->outstanding, OB_NO_AWAIT, 0, NULL);
      free (freeme);
    }
}

// given prevailing confusion and automatic conversion, check for both...
static bool is_nil_or_null (bslaw s)
{
  return (!s || slaw_is_nil (s));
}

static bool outstanding_compatible (const outstanding_await_t *dst, int64 idx,
                                    bslaw pattern)
{
  if (dst->status == OB_NO_AWAIT)
    return false;
  if (idx != dst->idx)
    return false;
#if 0  // XXX: we could filter on the client side, but currently we don't!
  // If the outstanding await doesn't have a pattern, then that's compatible
  // with any pattern (since we can still filter on the client side)
  if (is_nil_or_null (dst->pattern))
    return true;
  // If new await doesn't have a pattern, but outstanding one does (since
  // we didn't return above), then that's not compatible
  if (is_nil_or_null (pattern))
    return false;
#else
  // If both have no pattern, that's compatible
  if (is_nil_or_null (dst->pattern) && is_nil_or_null (pattern))
    return true;
  // If one has a pattern and the other doesn't, that's not compatible
  if (is_nil_or_null (dst->pattern) != is_nil_or_null (pattern))
    return false;
#endif
  // Both have patterns; only compatible if they are the same one
  return slawx_equal (pattern, dst->pattern);
}

static slaw make_err_slaw (ob_retort modern, unt32 net_version)
{
  ob_retort ancient = ob_new_retort_to_old (modern, net_version);

  // Should really be int64, but old clients will expect unt64
  if (net_version <= 2)
    return slaw_unt64 (ancient);
  else
    return slaw_int64 (ancient);
}

/// Network command processing is inspired by section 9.1, "Notation,"
/// of "The Practice of Programming" by Kernighan and Pike (a highly
/// recommended read in general).  It is elegant, bug-resistant, and
/// easy to maintain, but it can be a little subtle in some points and
/// deserves careful documentation.
///
/// Our goal is to package up pool operations, send them over the
/// network, unpack the operations, execute them locally, package up
/// the results, send them back, and unpack them again.  This is
/// basically RPC and XDR (what NFS is implemented on top of), but
/// specialized for pools and proteins.
///
/// Cross-platform data representation is already taken care of by
/// proteins, so let's focus on unpacking and executing operations.
/// We could open-code the pack and unpack operations for each
/// command, but in my experience (and that of K&P), this requires a
/// lot of tedious, verbose, repetitive code which tends to have lots
/// of hidden bugs and is a pain to update and maintain.
///
/// Instead, we'll define a "little language" to describe packed
/// operation formats and then write a single function to pack and
/// unpack operations according to instructions in our little
/// language.  Our little language uses a single character to denote
/// each data type.  The pack and unpack operations take a sequence of
/// characters describing the arguments (or return values) of the
/// operation and pack or unpack them into the variables provided.
/// Think printf() and scanf() - just simpler.
///
/// The format characters are:
///
/// @code
/// 'i' - int64
/// 'p' - protein (same thing as 'x')
/// 'r' - ob_retort (automatically converts for old protocols)
/// 'R' - ob_retort (no conversion)
/// 's' - string
/// 't' - pool_timestamp
/// 'x' - slaw
/// @endcode

static ob_retort _pool_net_pack_op (int op_num, protein *ret_op,
                                    unt32 net_version, const char *fmt,
                                    va_list vargs)
{
  slabu *op_b = slabu_new ();
  if (!op_b)
    return OB_NO_MEM;
  int64 idx = slabu_map_put_cf (op_b, OP_KEY, slaw_int32 (op_num));
  if (idx < 0)
    {
      slabu_free (op_b);
      return OB_NO_MEM;
    }
  // Ops with no args are possible
  if (fmt == NULL)
    {
      slaw m = slaw_map_f (op_b);
      if (!m)
        return OB_NO_MEM;
      *ret_op = protein_from_ff (NULL, m);
      return OB_OK;
    }
  // Make a slaw bundle containing all the args
  slabu *sb = slabu_new ();
  slaw slaw_slaw;
  slaw err_slaw;
  const char *p;
  char *s;
  ob_retort err;
  // Go through each character in the format and add the corresponding
  // arg to the protein.
  for (p = fmt; *p != '\0'; p++)
    {
      switch (*p)
        {
          case 'i':
            idx = slabu_list_add_x (sb, slaw_int64 (va_arg (vargs, int64)));
            break;
          case 'r':
            err = va_arg (vargs, ob_retort);
            err_slaw = make_err_slaw (err, net_version);
            idx = slabu_list_add_x (sb, err_slaw);
            break;
          case 'R':
            err = va_arg (vargs, ob_retort);
            err_slaw =
              slaw_unt64 (err);  // use unt because old clients expect it
            idx = slabu_list_add_x (sb, err_slaw);
            break;
          case 's':
            s = va_arg (vargs, char *);
            slaw slaw_s;
            if (s == NULL)
              slaw_s = slaw_nil ();
            else
              slaw_s = slaw_string (s);
            idx = slabu_list_add_x (sb, slaw_s);
            break;
          case 't':
            idx = slabu_list_add_x (sb, slaw_float64 (va_arg (vargs, float64)));
            break;
          case 'p':
          case 'x':
            slaw_slaw = va_arg (vargs, slaw);
            if (slaw_slaw == NULL)
              idx = slabu_list_add_z (sb, NIL_SINGLETON);
            else
              // Don't need to copy the slaw as we'll be sending off the
              // end result of the protein before returning to the caller.
              idx = slabu_list_add_z (sb, slaw_slaw);
            break;
          default:
            // Since this indicates an internal programming error within libPlasma,
            // I think it makes more sense to abort than to return an error code.
            OB_FATAL_BUG_CODE (0x20106000, "unknown specifier character %c\n",
                               *p);
        }

      // check for failure to allocate memory for slaw.
      if (idx < 0)
        {
          slabu_free (op_b);
          slabu_free (sb);
          return idx;
        }
    }
  slaw list = slaw_list_f (sb);
  if (!list)
    {
      slabu_free (op_b);
      return OB_NO_MEM;
    }
  idx = slabu_map_put_cf (op_b, ARGS_KEY, list);
  if (idx < 0)
    {
      slabu_free (op_b);
      return idx;
    }
  slaw m = slaw_map_f (op_b);
  if (!m)
    return OB_NO_MEM;
  *ret_op = protein_from_ff (NULL, m);
  if (!*ret_op)
    return OB_NO_MEM;
  //slaw_spew_overview (*ret_op, stderr);
  return OB_OK;
}

/// For protocol version 0:
/// One-off stupid 64 bit big-endian conversions to send protein length
/// first.  Protein size info is variable length, making it hard to
/// know how many bytes to read, and we don't want to be messing around
/// in the guts of proteins anyway.
/// For protocol version 1:
/// Just use the octlen built into the protein header oct.

#ifdef __BIG_ENDIAN__
#define I_AM_LITTLE_ENDIAN false
#else
#define I_AM_LITTLE_ENDIAN true
#endif

static ob_retort pool_net_send_protein_len (pool_net_data *net, unt64 len)
{
  if (net->net_version != 0)
    return OB_OK;

  // Only protocol version 0 needs to explicitly send the length

  unt64 host_len = len;
  // actually do endian conversion
  unt64 net_len = host_len;
  if (I_AM_LITTLE_ENDIAN)
    net_len = ob_swap64 (net_len);

  return net->send_nbytes (net->connfd, &net_len, sizeof (net_len),
                           *net->wakeup_handle_loc);
}

static ob_retort pool_net_recv_protein_len (pool_net_data *net,
                                            unt64 *host_len_p,
                                            unt64 *remaining_len_p,
                                            slaw_oct *header_p)
{
  unt64 net_len;
  ob_retort pret;

  pret = net->recv_nbytes (net->connfd, &net_len, sizeof (net_len),
                           *net->wakeup_handle_loc);

  if (pret != OB_OK)
    return pret;

  if (net->net_version != 0)
    {
      if (net->slaw_version != SLAW_VERSION_CURRENT)
        return POOL_WRONG_VERSION;
      *header_p = net_len;
      *host_len_p = protein_len ((protein) header_p);
      *remaining_len_p = *host_len_p - sizeof (net_len);
      return (*host_len_p >= sizeof (net_len)) ? OB_OK : POOL_PROTOCOL_ERROR;
    }

  // actually do endian conversion
  if (I_AM_LITTLE_ENDIAN)
    net_len = ob_swap64 (net_len);
  *host_len_p = net_len;
  *remaining_len_p = net_len;
  return OB_OK;
}

static ob_retort _pool_net_send_op (pool_net_data *net, int op_num,
                                    const char *fmt, va_list vargs)
{
  ob_retort pret;
  protein op_prot;
  ob_log (OBLV_DBUG, 0x20106001, "%s: op %d fmt %s\n", __FUNCTION__, op_num,
          fmt ? fmt : "NULL");
  pret = _pool_net_pack_op (op_num, &op_prot, net->net_version, fmt, vargs);
  if (pret != OB_OK)
    return pret;

  int64 len;
  pret = slaw_convert_to (&op_prot, net->slaw_version, &len);
  if (pret < OB_OK)
    {
      protein_free (op_prot);
      return pret;
    }

  // Send length of protein first
  pret = pool_net_send_protein_len (net, len);
  if (pret != OB_OK)
    {
      protein_free (op_prot);
      return pret;
    }

#ifdef PLASMA_ENABLE_LARGE_TRANSFER_LOGGING
  // Log large protein sends
  if (len > PLASMA_LARGE_TRANSFER_THRESHOLD)
    {
      ob_log (OBLV_INFO, 0x20106002, "Sending large protein: %lld bytes (%.2f MB) for op %d\n",
              (long long) len, len / (1024.0 * 1024.0), op_num);
    }
#endif

  pret = net->send_nbytes (net->connfd, op_prot, len, *net->wakeup_handle_loc);

  protein_free (op_prot);
  return pret;
}

ob_retort pool_net_send_op (pool_net_data *net, int op_num, const char *fmt,
                            ...)
{
  ob_retort pret;
  va_list vargs;
  va_start (vargs, fmt);
  pret = _pool_net_send_op (net, op_num, fmt, vargs);
  va_end (vargs);
  return pret;
}

ob_retort pool_net_recv_op (pool_net_data *net, int *op_num, protein *ret_prot)
{
  ob_retort pret;
  // First pull out the length of the following protein
  unt64 len, remaining_len, header_len;
  slaw_oct header = 0;
  pret = pool_net_recv_protein_len (net, &len, &remaining_len, &header);
  if (pret != OB_OK)
    return pret;
  header_len = len - remaining_len;

  // Avoid DoS or bug due to unrealistic length
  if (len > MAX_SLAW_SIZE)
    return POOL_PROTOCOL_ERROR;

#ifdef PLASMA_ENABLE_LARGE_TRANSFER_LOGGING
  // Log large protein receives
  if (len > PLASMA_LARGE_TRANSFER_THRESHOLD)
    {
      ob_log (OBLV_INFO, 0x20106003, "Receiving large protein: %llu bytes (%.2f MB)\n",
              (unsigned long long) len, len / (1024.0 * 1024.0));
    }
#endif

  // Now we can allocate the right amount of space for the protein
  char *raw_prot = (char *) malloc (len);
  if (!raw_prot)
    return OB_NO_MEM;

  memcpy (raw_prot, &header, header_len);

  pret = net->recv_nbytes (net->connfd, raw_prot + header_len, remaining_len,
                           *net->wakeup_handle_loc);

  if (pret != OB_OK)
    {
      free (raw_prot);
      return pret;
    }
  protein op_prot = (protein) raw_prot;
  pret = slaw_convert_from (&op_prot, SLAW_ENDIAN_UNKNOWN, net->slaw_version);
  if (pret != OB_OK)
    {
      protein_free (op_prot);
      return pret;
    }

  // Yank out the operation number
  bslaw op_s = slaw_map_find_c (protein_ingests (op_prot), OP_KEY);
  const int32 *op_ptr = slaw_int32_emit (op_s);
  if (op_ptr == NULL)
    {
      slaw str = slaw_spew_overview_to_string (op_prot);
      OB_LOG_ERROR_CODE (0x20106003, "Missing '%s' ingest:\n%s\n", OP_KEY,
                         slaw_string_emit (str));
      slaw_free (str);
      protein_free (op_prot);
      return POOL_PROTOCOL_ERROR;
    }
  *op_num = *op_ptr;
  *ret_prot = op_prot;
  ob_log (OBLV_DBUG, 0x20106004, "%s: op %d\n", __FUNCTION__, *op_num);
  return pret;
}

static ob_retort _pool_net_unpack_op (bprotein op_prot, unt32 net_vers,
                                      const char *fmt, va_list vargs)
{
  bslaw list = slaw_map_find_c (protein_ingests (op_prot), ARGS_KEY);
  bslaw arg_slaw;
  slaw *slaw_slaw;
  int64 *idx;
  ob_retort *pret;
  pool_timestamp *ts;
  char **string;
  int i = 0;
  // Go through each character in the format and pull the
  // corresponding arg out of the slaw list.
  //slaw_spew_overview (list, stderr, NULL);
  const char *p;
  for (p = fmt; *p != '\0'; p++)
    {
      const float64 *f64p;
      const int64 *i64p;
      const char *strp;
      switch (*p)
        {
          case 'i':
            idx = va_arg (vargs, int64 *);
            arg_slaw = slaw_list_emit_nth (list, i++);
            i64p = slaw_int64_emit (arg_slaw);
            if (i64p)
              *idx = *i64p;
            else
              {
                slaw str = slaw_spew_overview_to_string (arg_slaw);
                OB_LOG_ERROR_CODE (0x2010601e, "This was not an int64:\n%s\n",
                                   slaw_string_emit (str));
                slaw_free (str);
                return POOL_PROTOCOL_ERROR;
              }
            break;
          case 'r':
            pret = va_arg (vargs, ob_retort *);
            arg_slaw = slaw_list_emit_nth (list, i++);
            if (slaw_is_unt64 (arg_slaw))
              *pret =
                ob_old_retort_to_new (*slaw_unt64_emit (arg_slaw), net_vers);
            else if (slaw_is_int64 (arg_slaw))
              *pret =
                ob_old_retort_to_new (*slaw_int64_emit (arg_slaw), net_vers);
            else
              // XXX: Like the other cases, this should log and return a
              // POOL_PROTOCOL_ERROR instead of setting the out parameter.
              *pret = OB_UNKNOWN_ERR;
            break;
          case 's':
            string = va_arg (vargs, char **);
            arg_slaw = slaw_list_emit_nth (list, i++);
            strp = slaw_string_emit (arg_slaw);
            if (strp)
              {
                *string = strdup (strp);
                if (*string == NULL)
                  return OB_NO_MEM;
              }
            else
              {
                slaw str = slaw_spew_overview_to_string (arg_slaw);
                OB_LOG_ERROR_CODE (0x2010601f, "This was not a string:\n%s\n",
                                   slaw_string_emit (str));
                slaw_free (str);
                return POOL_PROTOCOL_ERROR;
              }
            break;
          case 't':
            ts = va_arg (vargs, float64 *);
            arg_slaw = slaw_list_emit_nth (list, i++);
            f64p = slaw_float64_emit (arg_slaw);
            if (f64p)
              *ts = *f64p;
            else
              {
                slaw str = slaw_spew_overview_to_string (arg_slaw);
                OB_LOG_ERROR_CODE (0x20106012, "This was not a float64:\n%s\n",
                                   slaw_string_emit (str));
                slaw_free (str);
                return POOL_PROTOCOL_ERROR;
              }
            break;
          case 'p':
          case 'x':
            slaw_slaw = va_arg (vargs, slaw *);
            if (!slaw_slaw)
              {
                i++;
                break;
              }
            arg_slaw = slaw_list_emit_nth (list, i++);
            if (
              !arg_slaw ||
              // XXX: potentially dangerous, converting nil to NULL.  (bug 480)
              slaw_is_nil (arg_slaw))
              {
                *slaw_slaw = NULL;
              }
            else
              {
                *slaw_slaw = slaw_dup (arg_slaw);
                if (*slaw_slaw == NULL)
                  return OB_NO_MEM;
              }
            break;
          default:
            // Since this indicates an internal programming error within libPlasma,
            // I think it makes more sense to abort than to return an error code.
            OB_FATAL_BUG_CODE (0x20106005, "unknown specifier character %c\n",
                               *p);
        }
    }
  return OB_OK;
}

ob_retort pool_net_unpack_op (bprotein op_prot, unt32 nv, const char *fmt, ...)
{
  ob_retort pret;
  va_list vargs;
  va_start (vargs, fmt);
  // We need to call unpack_op from another variadic function, and the
  // wrapper to convert to a va_list arg is the only way to reuse code...
  pret = _pool_net_unpack_op (op_prot, nv, fmt, vargs);
  va_end (vargs);
  return pret;
}

ob_retort pool_net_unpack_op_f (protein op_prot, unt32 net_version,
                                const char *fmt, ...)
{
  ob_retort pret;
  va_list vargs;
  va_start (vargs, fmt);
  // We need to call unpack_op from another variadic function, and the
  // wrapper to convert to a va_list arg is the only way to reuse code...
  pret = _pool_net_unpack_op (op_prot, net_version, fmt, vargs);
  va_end (vargs);
  protein_free (op_prot);
  return pret;
}

ob_retort pool_net_send_result (pool_net_data *net, const char *fmt, ...)
{
  ob_retort pret;
  va_list vargs;
  ob_log (OBLV_DBUG, 0x20106006, "sending result\n");
  va_start (vargs, fmt);
  pret = _pool_net_send_op (net, POOL_CMD_RESULT, fmt, vargs);
  va_end (vargs);
  return pret;
}

static bool unawait_old (outstanding_await_t *o)
{
  bool b = (o->status == OB_OLD_AWAITING);
  if (b)
    set_outstanding (o, OB_NO_AWAIT, 0, NULL);
  return b;
}

static ob_retort pool_net_recv_result_internal (pool_net_data *net,
                                                int op_expected,
                                                const char *fmt, va_list vargs)
{
  int op_num;
  protein results = NULL;
  ob_retort pret;

  // fix https://lists.oblong.com/pipermail/buildtools/2012-December/000638.html
  OB_INVALIDATE (op_num);

  do
    {
      Free_Protein (results);
      pret = pool_net_recv_op (net, &op_num, &results);
      if (pret < OB_OK)
        return pret;
    }
  while (unawait_old (&net->outstanding) || op_num != op_expected);

  pret = _pool_net_unpack_op (results, net->net_version, fmt, vargs);
  if (ob_log_is_enabled (__FILE__, OBLV_DBUG, 0x2010601c))
    {
      bslaw list = slaw_map_find_c (protein_ingests (results), ARGS_KEY);
      slaw str = slaw_spew_overview_to_string (list);
      ob_log (OBLV_DBUG, 0x2010601c, "%s\n", slaw_string_emit (str));
      slaw_free (str);
    }
  protein_free (results);
  return pret;
}

ob_retort pool_net_recv_result (pool_net_data *net, const char *fmt, ...)
{
  va_list vargs;
  va_start (vargs, fmt);
  ob_retort pret =
    pool_net_recv_result_internal (net, POOL_CMD_RESULT, fmt, vargs);
  va_end (vargs);
  set_outstanding (&net->outstanding, OB_NO_AWAIT, 0, NULL);
  return pret;
}

ob_retort pool_net_recv_unusual_result (pool_net_data *net, int op_expected,
                                        const char *fmt, ...)
{
  va_list vargs;
  va_start (vargs, fmt);
  ob_retort pret = pool_net_recv_result_internal (net, op_expected, fmt, vargs);
  va_end (vargs);
  return pret;
}

ob_retort pool_net_clear_dirty (pool_hose ph)
{
  ob_retort pret;

  pool_validate_context (ph->ctx, "pool_net_clear_dirty");

  if (!ph->dirty)
    return OB_OK;

  ob_log (OBLV_INFO, 0x20106009, "dirty net pool hose, reconnecting\n");

  pret = ph->withdraw (ph);
  if (pret != OB_OK)
    return pret;

  ph->dirty = 0;

  pret = ph->participate (ph);
  if (pret != OB_OK)
    // FIXME: do we need to mark ph as dead or something?
    return pret;

  // ph is at the same index, verily. but seek anyway, perhaps we need to tell
  // the remote side.
  if ((pret = pool_seekto (ph, ph->index)) != OB_OK)
    OB_LOG_ERROR_CODE (0x2010600a, "warning: re-seek on dirty poho failed\n");

  return OB_OK;
}

/// Generic implementation of network pool operations.  If you have
/// defined a send and recv bytes function, you can use these functions
/// without change.

static ob_retort receive_index_value (pool_hose ph, ob_retort pret, int64 *idx)
{
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }

  if (pret != OB_OK)
    return pret;

  ob_retort remote_pret = OB_OK;
  pret = pool_net_recv_result (ph->net, "ir", idx, &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  return (pret != OB_OK) ? pret : remote_pret;
}

ob_retort pool_net_newest_index (pool_hose ph, int64 *idx)
{
  ob_retort pret;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_NEWEST_INDEX, NULL);
  return receive_index_value (ph, pret, idx);
}

ob_retort pool_net_oldest_index (pool_hose ph, int64 *idx)
{
  ob_retort pret;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_OLDEST_INDEX, NULL);
  return receive_index_value (ph, pret, idx);
}

ob_retort pool_net_index_lookup (pool_hose ph, int64 *idx, pool_timestamp ts,
                                 time_comparison cmp, bool rel)
{
  ob_retort pret;
  if (!pool_net_supports_cmd (ph->net, POOL_CMD_INDEX_LOOKUP))
    return POOL_UNSUPPORTED_OPERATION;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_INDEX_LOOKUP, "tii", ts,
                           rel ? ph->index : -1, (int64) cmp);
  return receive_index_value (ph, pret, idx);
}

ob_retort pool_net_set_hose_name (pool_hose ph, const char *name)
{
  ob_retort pret;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  if (!pool_net_supports_cmd (ph->net, POOL_CMD_SET_HOSE_NAME))
    // Since this is an optional operation, we just say "okay" rather
    // than "unsupported".
    return OB_OK;
  int64 pid = getpid ();
  return pool_net_send_op (ph->net, POOL_CMD_SET_HOSE_NAME, "ssi", name,
                           ob_get_prog_name (), pid);
  // Note: there is no response to this command; none is needed.
}

ob_retort pool_net_deposit (pool_hose ph, bprotein p, int64 *idx,
                            pool_timestamp *ret_ts)
{
  ob_retort pret, remote_pret;
  int64 remote_index;
  slaw optional_timestamp;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_DEPOSIT, "p", p);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  // Use "irx" instead of "irt" to make the timestamp optional.
  // Really old servers sent "ir" instead of "irt".
  pret = pool_net_recv_result (ph->net, "irx", &remote_index, &remote_pret,
                               &optional_timestamp);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  if (idx)
    *idx = remote_index;
  if (ret_ts)
    {
      const float64 *ts_ptr = slaw_float64_emit (optional_timestamp);
      if (ts_ptr)
        *ret_ts = *ts_ptr;
      else
        *ret_ts = OB_NAN;
    }
  slaw_free (optional_timestamp);
  return remote_pret;
}

ob_retort pool_net_nth_protein (pool_hose ph, int64 idx, protein *ret_prot,
                                pool_timestamp *ret_ts)
{
  ob_retort pret, remote_pret;
  pool_timestamp ts;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_NTH_PROTEIN, "i", idx);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret = pool_net_recv_result (ph->net, "ptr", ret_prot, &ts, &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  if (ret_ts)
    *ret_ts = ts;
  return remote_pret;
}

ob_retort pool_net_advance_oldest (pool_hose ph, int64 idx_in)
{
  if (!pool_net_supports_cmd (ph->net, POOL_CMD_ADVANCE_OLDEST))
    return POOL_UNSUPPORTED_OPERATION;

  ob_retort pret, remote_pret;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_ADVANCE_OLDEST, "i", idx_in);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret = pool_net_recv_result (ph->net, "r", &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

ob_retort pool_net_change_options (pool_hose ph, bslaw options)
{
  if (!pool_net_supports_cmd (ph->net, POOL_CMD_CHANGE_OPTIONS))
    return POOL_UNSUPPORTED_OPERATION;

  ob_retort pret, remote_pret;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  pret = pool_net_send_op (ph->net, POOL_CMD_CHANGE_OPTIONS, "p", options);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret = pool_net_recv_result (ph->net, "r", &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  return remote_pret;
}

// XXX: this breaks the abstraction a little bit between net and tcp.
// On return, *gotit indicates whether the hose already has a response
// pending (e. g. from a multi-await).  Does not block.
static ob_retort pool_net_got_response (pool_hose ph, bool *gotit)
{
  ob_select2_t sel2;

  *gotit = false;

  ob_retort tort = ob_select2_prepare (&sel2, OB_SEL2_RECEIVE, ph->net->connfd,
                                       OB_NULL_HANDLE);

  if (tort < OB_OK)
    return tort;

  tort = ob_select2 (&sel2, POOL_NO_WAIT, false);

  if (tort == OB_OK)
    *gotit = true;
  else if (tort == POOL_AWAIT_TIMEDOUT)
    tort = OB_OK;

  ob_retort t2 = ob_select2_finish (&sel2);

  return (tort == OB_OK ? t2 : tort);
}

static ob_retort pool_net_next_internal (pool_hose ph, protein *ret_prot,
                                         pool_timestamp *ret_ts,
                                         int64 *ret_index, bool opportunistic,
                                         bslaw search)
{
  ob_retort pret, remote_pret;
  pool_timestamp ts;
  int64 idx;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;

  const bool fancy =
    pool_net_supports_cmd (ph->net, POOL_CMD_FANCY_ADD_AWAITER);

  // XXX: obscenely complicated state machine; surely there's a
  // clearer/better/shorter way to write it than what I've done here???
  // Also, there's way too much similarity between pool_net_next_internal()
  // and pool_net_multi_add_awaiter().  Need to find a way to factor out
  // the common parts.
  while (pret >= OB_OK)
    {
      // Ensure that remote_pret is undefined,
      // for maximal valgrind debuggingness.
      OB_INVALIDATE (remote_pret);
      // bounds-squish to 0 as required by bug 1286, since older servers
      // won't do this on the server side.
      const int64 squished = (ph->index < 0 ? 0 : ph->index);
      switch (ph->net->outstanding.status)
        {
          case OB_OLD_AWAITING:
            assert (!fancy);
            if (outstanding_compatible (&ph->net->outstanding, squished,
                                        search))
              {
                bool gotit;
                pret = pool_net_got_response (ph, &gotit);
                if (pret < OB_OK)
                  return pret;
                else if (gotit)
                  {
                    set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0,
                                     NULL);
                    pret = pool_net_recv_result (ph->net, "rpti", &remote_pret,
                                                 ret_prot, &ts, &idx);
                    break;
                  }
              }
          // fall thru
          case OB_NO_AWAIT:
            if (opportunistic)
              return POOL_NO_SUCH_PROTEIN;
            // in fancy case, use fancy await as a superset of next
            if (fancy)
              {
                pret = pool_net_send_op (ph->net, POOL_CMD_FANCY_ADD_AWAITER,
                                         "ix", squished, search);
                set_outstanding (&ph->net->outstanding, OB_FANCY_PREPARING,
                                 squished, search);
                continue;
              }
            else
              {
                pret = pool_net_send_op (ph->net, POOL_CMD_NEXT, "i", squished);
                if (pret == POOL_AWAIT_WOKEN_DIRTY)
                  {
                    pret = POOL_AWAIT_WOKEN;
                    ph->dirty = 1;
                  }
                if (pret != OB_OK)
                  return pret;
                pret = pool_net_recv_result (ph->net, "ptir", ret_prot, &ts,
                                             &idx, &remote_pret);
                break;
              }
          case OB_FANCY_PREPARING:
            assert (fancy);
            pret =
              pool_net_recv_unusual_result (ph->net, POOL_CMD_FANCY_RESULT_1,
                                            "rti", &remote_pret, &ts, &idx);
            if (pret < OB_OK)
              continue;
            if (outstanding_compatible (&ph->net->outstanding, squished,
                                        search))
              {
                if (remote_pret == OB_OK)
                  set_outstanding (&ph->net->outstanding, OB_FANCY_ARRIVED,
                                   squished, ph->net->outstanding.pattern);
                else if (remote_pret == POOL_NO_SUCH_PROTEIN)
                  set_outstanding (&ph->net->outstanding, OB_FANCY_AWAITING,
                                   squished, ph->net->outstanding.pattern);
                else
                  {
                    set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0,
                                     NULL);
                    break;
                  }
                continue;
              }
            else
              {
                set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
                continue;
              }
          case OB_FANCY_AWAITING:
            assert (fancy);
            if (outstanding_compatible (&ph->net->outstanding, squished,
                                        search))
              {
                bool gotit;
                pret = pool_net_got_response (ph, &gotit);
                if (pret < OB_OK)
                  return pret;
                else if (gotit)
                  {
                    pret =
                      pool_net_recv_unusual_result (ph->net,
                                                    POOL_CMD_FANCY_RESULT_2,
                                                    "rti", &remote_pret, &ts,
                                                    &idx);
                    if (pret < OB_OK)
                      continue;
                    if (remote_pret == OB_OK)
                      {
                        set_outstanding (&ph->net->outstanding,
                                         OB_FANCY_ARRIVED, squished,
                                         ph->net->outstanding.pattern);
                        if (ret_prot == NULL)
                          break;  // If protein is not needed, leave as "arrived"
                        else
                          continue;
                      }
                    else
                      {
                        set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0,
                                         NULL);
                        break;
                      }
                    OB_FATAL_BUG_CODE (0x20106013, "not reached\n");
                  }
                return POOL_NO_SUCH_PROTEIN;
              }
            if (opportunistic)
              return POOL_NO_SUCH_PROTEIN;
            set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
            continue;
          case OB_FANCY_ARRIVED:
            assert (fancy);
            if (!outstanding_compatible (&ph->net->outstanding, squished,
                                         search))
              {
                if (opportunistic)
                  return POOL_NO_SUCH_PROTEIN;
                set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
                continue;
              }
            remote_pret = OB_OK;
            pret =
              pool_net_recv_unusual_result (ph->net, POOL_CMD_FANCY_RESULT_3,
                                            "tip", &ts, &idx, ret_prot);
            set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
            break;
          default:
            OB_FATAL_BUG_CODE (0x20106014, "Well that should never happen\n");
        }

      break;
    }

  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret = remote_pret;
  if (pret != OB_OK)
    return pret;
  // pool_next() may update the pool index
  pret = pool_seekto (ph, idx + 1);
  if (ret_ts)
    *ret_ts = ts;
  if (ret_index)
    *ret_index = idx;
  return pret;
}

ob_retort pool_net_next (pool_hose ph, protein *ret_prot,
                         pool_timestamp *ret_ts, int64 *ret_index)
{
  return pool_net_next_internal (ph, ret_prot, ret_ts, ret_index, false,
                                 NIL_SINGLETON);
}

ob_retort pool_net_opportunistic_next (pool_hose ph, protein *ret_prot,
                                       pool_timestamp *ret_ts, int64 *ret_index)
{
  return pool_net_next_internal (ph, ret_prot, ret_ts, ret_index, true,
                                 NIL_SINGLETON);
}

ob_retort pool_net_prev (pool_hose ph, protein *ret_prot,
                         pool_timestamp *ret_ts, int64 *ret_index)
{
  ob_retort pret, remote_pret;
  pool_timestamp ts;
  int64 idx;

  if (!pool_net_supports_cmd (ph->net, POOL_CMD_PREV))
    return POOL_UNSUPPORTED_OPERATION;

  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  // The remote end doesn't keep its pool hose index up to date with
  // ours, so send our current index with this command.
  pret = pool_net_send_op (ph->net, POOL_CMD_PREV, "i", ph->index);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret =
    pool_net_recv_result (ph->net, "ptir", ret_prot, &ts, &idx, &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  if (remote_pret != OB_OK)
    return remote_pret;
  pret = pool_seekto (ph, idx);
  if (ret_ts)
    *ret_ts = ts;
  if (ret_index)
    *ret_index = idx;
  return pret;
}

ob_retort pool_net_probe_frwd (pool_hose ph, bslaw search, protein *ret_prot,
                               pool_timestamp *ret_ts, int64 *ret_index)
{
  ob_retort pret, remote_pret;
  pool_timestamp ts;
  int64 idx;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;

  const bool fancy =
    pool_net_supports_cmd (ph->net, POOL_CMD_FANCY_ADD_AWAITER);

  if (fancy)
    return pool_net_next_internal (ph, ret_prot, ret_ts, ret_index, false,
                                   search);

  // bounds-squish to 0 as required by bug 1286, since older servers
  // won't do this on the server side.
  const int64 squished = (ph->index < 0 ? 0 : ph->index);

  // The remote end doesn't keep its pool hose index up to date with
  // ours, so send our current index with this command.
  pret =
    pool_net_send_op (ph->net, POOL_CMD_PROBE_FRWD, "ix", squished, search);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret =
    pool_net_recv_result (ph->net, "ptir", ret_prot, &ts, &idx, &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  if (remote_pret != OB_OK)
    return remote_pret;
  // pool_next() may update the pool index
  pret = pool_seekto (ph, idx + 1);
  if (ret_ts)
    *ret_ts = ts;
  if (ret_index)
    *ret_index = idx;
  return pret;
}

ob_retort pool_net_probe_back (pool_hose ph, bslaw search, protein *ret_prot,
                               pool_timestamp *ret_ts, int64 *ret_index)
{
  ob_retort pret, remote_pret;
  pool_timestamp ts;
  int64 idx;

  if (!pool_net_supports_cmd (ph->net, POOL_CMD_PROBE_BACK))
    return POOL_UNSUPPORTED_OPERATION;

  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;
  // The remote end doesn't keep its pool hose index up to date with
  // ours, so send our current index with this command.
  pret =
    pool_net_send_op (ph->net, POOL_CMD_PROBE_BACK, "ix", ph->index, search);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  pret =
    pool_net_recv_result (ph->net, "ptir", ret_prot, &ts, &idx, &remote_pret);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;
  if (remote_pret != OB_OK)
    return remote_pret;
  pret = pool_seekto (ph, idx);
  if (ret_ts)
    *ret_ts = ts;
  if (ret_index)
    *ret_index = idx;
  return pret;
}

/// Pool TCP Protocol versions 0 and 1 use:
/// @code
/// #define POOL_WAIT_FOREVER  0
/// #define POOL_NO_WAIT      -1
/// @endcode
/// But version 2 and higher uses:
/// @code
/// #define POOL_WAIT_FOREVER -1
/// #define POOL_NO_WAIT       0
/// @endcode
/// So swap these two values if we are talking to
/// protocol version 0 or 1 on the other end.
void pool_net_adjust_timeout_value_for_version (pool_timestamp *timeout,
                                                unt8 net_version)
{
  if (net_version < 2)
    {
      if (*timeout == -1)
        *timeout = 0;
      else if (*timeout == 0)
        *timeout = -1;
    }
}

static ob_retort pool_net_multi_add_awaiter_internal (pool_hose ph,
                                                      protein *ret_prot,
                                                      pool_timestamp *ret_ts,
                                                      int64 *ret_index,
                                                      bslaw search);

static ob_retort fancy_await_next_single (pool_hose ph, pool_timestamp timeout,
                                          protein *ret_prot,
                                          pool_timestamp *ret_ts,
                                          int64 *ret_index, bslaw search)
{
  if (ob_log_is_enabled (__FILE__, OBLV_DBUG, 0x20106018))
    {
      slaw s = slaw_spew_overview_to_string (search);
      ob_log (OBLV_DBUG, 0x20106018,
              "Calling pool_net_multi_add_awaiter_internal with:\n%s\n",
              slaw_string_emit (s));
      slaw_free (s);
    }

  ob_retort tort = pool_net_multi_add_awaiter_internal (ph, ret_prot, ret_ts,
                                                        ret_index, search);
  if (tort != POOL_NO_SUCH_PROTEIN)
    return tort;

  // assert (OB_FANCY_AWAITING == ph->net->outstanding.status);
  if (OB_FANCY_AWAITING != ph->net->outstanding.status)
    OB_FATAL_BUG_CODE (0x20106015,
                       "expected ph->net->outstanding.status for '%s' to be "
                       "%d, but it was %d\n",
                       pool_get_hose_name (ph), (int) OB_FANCY_AWAITING,
                       (int) ph->net->outstanding.status);

  ob_select2_t sel2;
  tort = ob_select2_prepare (&sel2, OB_SEL2_RECEIVE, ph->net->connfd,
                             *ph->net->wakeup_handle_loc);

  if (tort < OB_OK)
    return tort;

  ob_log (OBLV_DBUG, 0x20106019, "Selecting for %f\n", timeout);

  tort = ob_select2 (&sel2, timeout, true);
  ob_retort t2 = ob_select2_finish (&sel2);
  pool_net_multi_remove_awaiter (ph);

  ob_log (OBLV_DBUG, 0x2010601a, "Done with %s\n", ob_error_string (tort));

  if (tort == OB_OK)
    {
      if (t2 < OB_OK)
        return t2;
      else if (is_nil_or_null (search))
        return pool_net_next (ph, ret_prot, ret_ts, ret_index);
      else
        return pool_net_probe_frwd (ph, search, ret_prot, ret_ts, ret_index);
    }

  return tort;
}

ob_retort pool_net_await_probe_frwd (pool_hose ph, bslaw search,
                                     pool_timestamp timeout, protein *ret_prot,
                                     pool_timestamp *ret_ts, int64 *ret_index)
{
  const bool fancy =
    pool_net_supports_cmd (ph->net, POOL_CMD_FANCY_ADD_AWAITER);

  if (timeout == POOL_NO_WAIT)
    {
      ob_retort tort =
        pool_net_probe_frwd (ph, search, ret_prot, ret_ts, ret_index);
      // awaits can never say POOL_NO_SUCH_PROTEIN (bug 720)
      return (tort == POOL_NO_SUCH_PROTEIN ? POOL_AWAIT_TIMEDOUT : tort);
    }
  if (fancy)
    return fancy_await_next_single (ph, timeout, ret_prot, ret_ts, ret_index,
                                    search);

  return POOL_UNSUPPORTED_OPERATION;
}

ob_retort pool_net_await_next_single (pool_hose ph, pool_timestamp timeout,
                                      protein *ret_prot, pool_timestamp *ret_ts,
                                      int64 *ret_index)
{
  const bool fancy =
    pool_net_supports_cmd (ph->net, POOL_CMD_FANCY_ADD_AWAITER);

  if (fancy)
    return fancy_await_next_single (ph, timeout, ret_prot, ret_ts, ret_index,
                                    NIL_SINGLETON);

  // This is the old way for old servers, although it exhibits bug 376

  ob_retort pret, remote_pret;
  pool_timestamp ts;
  int64 idx;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;

  pool_net_adjust_timeout_value_for_version (&timeout, ph->net->net_version);
  pret = pool_net_send_op (ph->net, POOL_CMD_AWAIT_NEXT_SINGLE, "t", timeout);
  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;

  // We will automatically sleep until the remote end sends a reply.
  // This requires that your recv_nbytes is blocking (or that we
  // define blocking vs. non-blocking recv_nbytes).  If the remote end
  // found a protein (rather than timing out), then it will return the
  // protein and timestamp in its reply.  If not, it will return a
  // null protein.
  pret =
    pool_net_recv_result (ph->net, "rpti", &remote_pret, ret_prot, &ts, &idx);
  if (pret == POOL_AWAIT_WOKEN || pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }
  if (pret != OB_OK)
    return pret;

  if (remote_pret == OB_OK)
    {
      if (ret_ts)
        *ret_ts = ts;
      if (ret_index)
        *ret_index = idx;
      /* The lack of this likely caused a bug where pool_await_next did not
       * increment the pool_hose index correctly.
       * See test case test-await-index.sh */
      ob_err_accum (&remote_pret, pool_seekto (ph, idx + 1));
    }

  return remote_pret;
}

ob_retort pool_net_multi_add_awaiter (pool_hose ph, protein *ret_prot,
                                      pool_timestamp *ret_ts, int64 *ret_index)
{
  return pool_net_multi_add_awaiter_internal (ph, ret_prot, ret_ts, ret_index,
                                              NIL_SINGLETON);
}

static ob_retort pool_net_multi_add_awaiter_internal (pool_hose ph,
                                                      protein *ret_prot,
                                                      pool_timestamp *ret_ts,
                                                      int64 *ret_index,
                                                      bslaw search)
{
  ob_retort pret, remote_pret;
  pool_timestamp ts;
  int64 idx;
  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    return pret;

  const bool fancy =
    pool_net_supports_cmd (ph->net, POOL_CMD_FANCY_ADD_AWAITER);

  // XXX: obscenely complicated state machine; surely there's a
  // clearer/better/shorter way to write it than what I've done here???
  // Also, there's way too much similarity between pool_net_next_internal()
  // and pool_net_multi_add_awaiter().  Need to find a way to factor out
  // the common parts.
  while (pret >= OB_OK)
    {
      // Ensure that remote_pret is undefined,
      // for maximal valgrind debuggingness.
      OB_INVALIDATE (remote_pret);
      ob_log (OBLV_DBUG, 0x2010601b, "status is %d\n",
              ph->net->outstanding.status);
      // bounds-squish to 0 as required by bug 1286, since older servers
      // won't do this on the server side.
      const int64 squished = (ph->index < 0 ? 0 : ph->index);
      switch (ph->net->outstanding.status)
        {
          case OB_OLD_AWAITING:
            assert (!fancy);
            if (outstanding_compatible (&ph->net->outstanding, squished,
                                        search))
              break;
          // fall thru
          case OB_NO_AWAIT:
            if (fancy)
              {
                pret = pool_net_send_op (ph->net, POOL_CMD_FANCY_ADD_AWAITER,
                                         "ix", squished, search);
                set_outstanding (&ph->net->outstanding, OB_FANCY_PREPARING,
                                 squished, search);
              }
            else
              {
                pret = pool_net_next (ph, ret_prot, ret_ts, ret_index);
                if (pret != POOL_NO_SUCH_PROTEIN)
                  return pret;
                pret =
                  pool_net_send_op (ph->net, POOL_CMD_MULTI_ADD_AWAITER, NULL);
                set_outstanding (&ph->net->outstanding, OB_OLD_AWAITING,
                                 squished, NIL_SINGLETON);
              }
            continue;
          case OB_FANCY_PREPARING:
            assert (fancy);
            pret =
              pool_net_recv_unusual_result (ph->net, POOL_CMD_FANCY_RESULT_1,
                                            "rti", &remote_pret, &ts, &idx);
            if (pret < OB_OK)
              continue;
            if (outstanding_compatible (&ph->net->outstanding, squished,
                                        search))
              {
                if (remote_pret == OB_OK)
                  set_outstanding (&ph->net->outstanding, OB_FANCY_ARRIVED,
                                   squished, ph->net->outstanding.pattern);
                else if (remote_pret == POOL_NO_SUCH_PROTEIN)
                  set_outstanding (&ph->net->outstanding, OB_FANCY_AWAITING,
                                   squished, ph->net->outstanding.pattern);
                else
                  set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
                continue;
              }
            else
              {
                set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
                continue;
              }
          case OB_FANCY_AWAITING:
            assert (fancy);
            if (outstanding_compatible (&ph->net->outstanding, squished,
                                        search))
              break;
            set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
            continue;
          case OB_FANCY_ARRIVED:
            assert (fancy);
            if (!outstanding_compatible (&ph->net->outstanding, squished,
                                         search))
              {
                set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
                continue;
              }
            if (ret_prot == NULL && ret_index == NULL && ret_ts == NULL)
              return OB_OK;  // leave as "arrived" if no info is needed
            pret =
              pool_net_recv_unusual_result (ph->net, POOL_CMD_FANCY_RESULT_3,
                                            "tip", &ts, &idx, ret_prot);
            set_outstanding (&ph->net->outstanding, OB_NO_AWAIT, 0, NULL);
            if (pret >= OB_OK)
              {
                if (ret_ts)
                  *ret_ts = ts;
                if (ret_index)
                  *ret_index = idx;
                ob_err_accum (&pret, pool_seekto (ph, idx + 1));
                // XXX: need to handle dirtiness
                return pret;
              }
            continue;
          default:
            OB_FATAL_BUG_CODE (0x20106016, "Well that should never happen\n");
        }

#ifdef _MSC_VER
      if (ph->notify_handle == 0)
        {
          //we create and store a waitable socket event into notify_handle
          HANDLE socket_event = WSACreateEvent ();
          if (socket_event == WSA_INVALID_EVENT)
            OB_LOG_ERROR_CODE (0x2010600d, "WSACreateEvent failed %d\n",
                               WSAGetLastError ());
          else
            {
              if (WSAEventSelect (ph->net->connfd, socket_event,
                                  FD_READ | FD_CLOSE)
                  != 0)
                OB_LOG_ERROR_CODE (0x2010600e, "WSAEventSelect failed %d\n",
                                   WSAGetLastError ());
              else
                ph->notify_handle = socket_event;
            }
        }
#else
      ph->notify_handle = ph->net->connfd;
#endif

      return POOL_NO_SUCH_PROTEIN;
    }

  if (pret == POOL_AWAIT_WOKEN_DIRTY)
    {
      pret = POOL_AWAIT_WOKEN;
      ph->dirty = 1;
    }

  return pret;
}

void pool_net_multi_remove_awaiter (OB_UNUSED pool_hose ph)
{
// Dummy awaiter removal, needed for the multi-pool code.  At present,
// we never tear down await state explicitly - it always happens as a
// by-product of sending the next command.  This keeps us from having
// to set up and tear down awaits for multi-pool awaits on every
// single call.

#ifdef _MSC_VER
  //actually we do need this awaiter removal method!

  //this restores the socket to blocking mode and
  //dissociates the notify_handle with the socket
  WSAEventSelect (ph->net->connfd, 0, 0);

  WSACloseEvent (ph->notify_handle);
  ph->notify_handle = 0;
#endif
}

static slaw convert_fetch_op_to_slaw (pool_fetch_op ops)
{
  return slaw_map_inline_cf ("idx",    slaw_int64 (ops.idx),
                             "des",    slaw_boolean (ops.want_descrips),
                             "ing",    slaw_boolean (ops.want_ingests),
                             "roff",   slaw_int64 (ops.rude_offset),
                             "rbytes", slaw_int64 (ops.rude_length),
                             NULL);
}

static slaw convert_fetch_ops_to_slaw (const pool_fetch_op *ops, int64 nops)
{
  int64 i;
  slabu *sb = slabu_new ();
  for (i = 0; sb && i < nops; i++)
    slabu_list_add_x (sb, convert_fetch_op_to_slaw (ops[i]));
  return slaw_list_f (sb);
}

static ob_retort convert_slaw_to_fetch_op (bslaw s, pool_fetch_op *op,
                                           bool clamp)
{
  int64 idx = slaw_path_get_int64 (s, "idx", -1);
  if (idx != op->idx)
    {
      if (clamp)
        op->idx = idx;
      else
        return POOL_PROTOCOL_ERROR;
    }
  op->tort = slaw_path_get_int64 (s, "retort", OB_UNKNOWN_ERR);
  op->ts = slaw_path_get_float64 (s, "time", OB_NAN);
  op->total_bytes = slaw_path_get_int64 (s, "tbytes", -1);
  op->descrip_bytes = slaw_path_get_int64 (s, "dbytes", -1);
  op->ingest_bytes = slaw_path_get_int64 (s, "ibytes", -1);
  op->rude_bytes = slaw_path_get_int64 (s, "rbytes", -1);
  op->num_descrips = slaw_path_get_int64 (s, "ndes", -1);
  op->num_ingests = slaw_path_get_int64 (s, "ning", -1);
  bprotein p = slaw_path_get_slaw (s, "prot");
  if (op->tort >= OB_OK && slaw_is_protein (p))
    op->p = slaw_dup (p);
  else
    op->p = NULL;
  return OB_OK;
}

static ob_retort convert_slaw_to_fetch_ops (bslaw s, pool_fetch_op *ops,
                                            int64 nops, bool clamp)
{
  int64 i;
  bslaw el = NULL;
  ob_retort tort = OB_OK;
  for (i = 0; i < nops; i++)
    ob_err_accum (&tort,
                  convert_slaw_to_fetch_op (el = slaw_list_emit_next (s, el),
                                            &(ops[i]), clamp));
  return tort;
}

ob_retort pool_net_fetch (pool_hose ph, pool_fetch_op *ops, int64 nops,
                          int64 *oldest_idx_out, int64 *newest_idx_out,
                          bool clamp)
{
  int cmd = POOL_CMD_SUB_FETCH_EX;
  if (pool_net_supports_cmd (ph->net, POOL_CMD_SUB_FETCH) && !clamp)
    cmd = POOL_CMD_SUB_FETCH;
  if (!pool_net_supports_cmd (ph->net, cmd))
    return POOL_UNSUPPORTED_OPERATION;

  const int64 clamp64 = clamp;

  int64 dummies[2];
  if (!oldest_idx_out)
    oldest_idx_out = &(dummies[0]);
  if (!newest_idx_out)
    newest_idx_out = &(dummies[1]);

  slaw s = convert_fetch_ops_to_slaw (ops, nops);
  ob_retort pret;
  if (cmd == POOL_CMD_SUB_FETCH)
    pret = pool_net_send_op (ph->net, POOL_CMD_SUB_FETCH, "x", s);
  else
    pret = pool_net_send_op (ph->net, POOL_CMD_SUB_FETCH_EX, "xi", s, clamp64);
  Free_Slaw (s);
  if (pret < OB_OK)
    return pret;
  pret =
    pool_net_recv_result (ph->net, "xii", &s, oldest_idx_out, newest_idx_out);
  if (pret < OB_OK)
    return pret;
  pret = convert_slaw_to_fetch_ops (s, ops, nops, clamp);
  Free_Slaw (s);
  return pret;
}

ob_retort pool_net_server_await (pool_hose ph, pool_net_data *net,
                                 pool_timestamp timeout, protein *ret_prot,
                                 pool_timestamp *ret_ts, int64 *ret_index)
{
  ob_select2_t sel2;
  ob_retort pret, selpret;
  pool_timestamp target = OB_NAN;

  if ((pret = pool_net_clear_dirty (ph)) != OB_OK)
    goto erroneous;
  ob_log (OBLV_DBUG, 0x20106010, "%s: awaiting, timeout %f\n", ph->name,
          timeout);

considered_harmful:
  pret = pool_fifo_multi_add_awaiter (ph, ret_prot, ret_ts, ret_index);
  // Did we get a protein?  Then send it back now.
  if (pret == OB_OK)
    return pret;
  // Did we get a permanent error?
  if (pret != POOL_NO_SUCH_PROTEIN)
    goto erroneous;

  pret =
    ob_select2_prepare (&sel2, OB_SEL2_RECEIVE, net->connfd, ph->notify_handle);
  if (pret < OB_OK)
    goto erroneous;
  selpret = pret =
    ob_select2 (&sel2, private_incremental_timeout (timeout, &target), false);
  if (pret < OB_OK && pret != POOL_AWAIT_TIMEDOUT && pret != POOL_AWAIT_WOKEN)
    {
      ob_select2_finish (&sel2);
      goto erroneous;
    }
  pret = ob_select2_finish (&sel2);
  if (pret < OB_OK)
    goto erroneous;

  ob_log (OBLV_DBUG, 0x20106011, "%s: done awaiting\n", ph->name);

  //when we exit this function we want to leave no notification file in
  //the notification folder, and we want no notification event still open

  //if we were woken by a pool deposit, the notification *file* has
  //been unlinked. however, our handle to the event object (->notify_handle)
  //is still in-use so we need to release it and call multi_remove_awaiter
  //in any case
  pool_fifo_multi_remove_awaiter (ph);

  // Check to see if we got a protein
  pret = pool_next (ph, ret_prot, ret_ts, ret_index);
  if (pret == POOL_NO_SUCH_PROTEIN && selpret == POOL_AWAIT_WOKEN)
    {
      // We can be legitimately woken and have nothing to read,
      // since deposits and notifications are not atomic
      // (intentionally so).  So we can get a protein via
      // pool_next(), then ask to be notified for that pool, and
      // then get notified for the protein we already read.  If
      // this happens, just go back around.
      // (But if we go back around, we subtract the
      // time we've already waited from the timeout.)
      ob_log (OBLV_DBUG, 0x2010601d, "Going back around for %s\n",
              pool_get_hose_name (ph));
      goto considered_harmful;
    }
erroneous:
  // If not, make the protein pointer null, timestamp zero, and index
  // 0, since the client is expecting a reply in that format.  pack_op
  // handles the NULL protein pointer correctly.
  if (pret != OB_OK)
    {
      *ret_prot = NULL;
      *ret_ts = 0;
      *ret_index = 0;
    }
  return pret;
}
