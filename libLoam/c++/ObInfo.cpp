
/* (c)  oblong industries */

#include "ObInfo.h"

#include <libLoam/c/ob-dirs.h>
#include <libLoam/c/ob-vers.h>
#include <libLoam/c++/ObTrove.h>
#include <libLoam/c++/Str.h>

using namespace oblong::loam;

ob_version_of_what GetUnderlyingEnumValue (ObInfo::Version_Of what)
{
  switch (what)
    {
      case ObInfo::Version_Of::Gspeak:
        return OB_VERSION_OF_GSPEAK;
      case ObInfo::Version_Of::Compiler:
        return OB_VERSION_OF_COMPILER;
      case ObInfo::Version_Of::Os:
        return OB_VERSION_OF_OS;
      case ObInfo::Version_Of::Kernel:
        return OB_VERSION_OF_KERNEL;
      case ObInfo::Version_Of::Libc:
        return OB_VERSION_OF_LIBC;
      case ObInfo::Version_Of::Cpu:
        return OB_VERSION_OF_CPU;
      case ObInfo::Version_Of::Yobuild:
        return OB_VERSION_OF_YOBUILD;
      case ObInfo::Version_Of::Machine:
        return OB_VERSION_OF_MACHINE;
      case ObInfo::Version_Of::Abi:
        return OB_VERSION_OF_ABI;
      case ObInfo::Version_Of::Build:
        return OB_BUILD_CONFIGURATION;
    }

  OB_LOG_BUG_CODE (0x110e0000,
                   "Called with a valid Version_Of arg, but "
                   "GetUnderlyingEnumValue hasn't been updated, please report "
                   "this bug.  Returning current g-speak version.");
  return OB_VERSION_OF_GSPEAK;
}


Str ObInfo::GetInfo (ObInfo::Version_Of what)
{
  char *s = ob_get_version (GetUnderlyingEnumValue (what));
  Str thing_string (s);
  free (s);
  return thing_string;
}

int64 ObInfo::GetInfo (ObInfo::Number_Of what)
{
  switch (what)
    {
      case ObInfo::Number_Of::Cpu_Mhz:
        return ob_get_system_info (OB_SYSINFO_CPU_MHZ);
      case ObInfo::Number_Of::Cpu_Cores:
        return ob_get_system_info (OB_SYSINFO_NUM_CORES);
      case ObInfo::Number_Of::Ram:
        return ob_get_system_info (OB_SYSINFO_PHYSICAL_MEGABYTES);
      case ObInfo::Number_Of::Swap:
        return ob_get_system_info (OB_SYSINFO_VIRTUAL_MEGABYTES);
      case ObInfo::Number_Of::Bits_In_Word:
        return 8 * sizeof (void *);
    }

  OB_LOG_BUG_CODE (0x110e0001,
                   "Called with a valid Number_Of argument, but "
                   "GetInfo hasn't been updated to support it yet, please "
                   "report this bug.");
  return 0;
}

ObTrove<Str> ObInfo::GetInfo (ObInfo::Location_Of what)
{
  const char *dir = nullptr;
  switch (what)
    {
      case ObInfo::Location_Of::Prefix:
        dir = ob_get_standard_path (ob_prefix_dir);
        break;
      case ObInfo::Location_Of::Share:
        dir = ob_get_standard_path (ob_share_path);
        break;
      case ObInfo::Location_Of::Etc:
        dir = ob_get_standard_path (ob_etc_path);
        break;
      case ObInfo::Location_Of::Var:
        dir = ob_get_standard_path (ob_var_path);
        break;
      case ObInfo::Location_Of::Tmp:
        dir = ob_get_standard_path (ob_tmp_dir);
        break;
      case ObInfo::Location_Of::Pools:
        dir = ob_get_standard_path (ob_pools_dir);
        break;
      case ObInfo::Location_Of::Yobuild:
        dir = ob_get_standard_path (ob_yobuild_dir);
        break;
    }

  if (dir)
    {
      Str split_str (dir);
      return split_str.Split (Str (OB_PATH_CHAR));
    }
  else
    {
      OB_LOG_BUG_CODE (0x110e0002,
                       "Called with a valid Location_Of argument, "
                       "but GetInfo hasn't been updated to know about this "
                       "location.  Please report this bug.");
      return ObTrove<Str> ();
    }
}

#define CHECK_CPU_FLAG(lst, flag)                                              \
  if (ob_x86_features () & OB_X86_FEATURE_##flag)                              \
  (lst).Append (Str (#flag))

#define CHECK_CPU_FLAG_AS_NAME(lst, flag, name)                                \
  if (ob_x86_features () & OB_X86_FEATURE_##flag)                              \
  (lst).Append (Str (name))


ObTrove<Str> ObInfo::GetCPUFeatureFlags ()
{
  ObTrove<Str> flags (2.0);
  CHECK_CPU_FLAG (flags, SSE);
  CHECK_CPU_FLAG (flags, SSE2);
  CHECK_CPU_FLAG (flags, SSE3);
  CHECK_CPU_FLAG (flags, PCLMULQDQ);
  CHECK_CPU_FLAG (flags, SSSE3);
  CHECK_CPU_FLAG (flags, FMA3);
  CHECK_CPU_FLAG (flags, CX16);
  CHECK_CPU_FLAG_AS_NAME (flags, SSE41, "SSE4.1");
  CHECK_CPU_FLAG_AS_NAME (flags, SSE42, "SSE4.2");
  CHECK_CPU_FLAG (flags, MOVBE);
  CHECK_CPU_FLAG (flags, POPCNT);
  CHECK_CPU_FLAG (flags, AES);
  CHECK_CPU_FLAG (flags, AVX);
  CHECK_CPU_FLAG (flags, CVT16);
  CHECK_CPU_FLAG (flags, RDRND);
  return flags;
}

Str Escape (const Str &str)
{
  const char *sp = str.utf8 ();
  char c;
  Str s ("");

  while ((c = *(sp++)))
    {
      if (c == '\"' || c == '\\')
        s += Str::Format ("\\%c", c);
      else if (c < ' ' || c == 0x7f)
        // The four-digit little-u escape is the one numerical escape
        // that's *supposed* to be valid in both JSON and YAML.
        // Sadly, Ruby's YAML parser (Syck) is not compliant.
        // But there shouldn't really be any of these control characters
        // in ob-version's output anyway.
        s += Str::Format ("\\u%04X", c);
      else
        s += c;
    }
  return s;
}

Str JSONPair (const Str &key, ObTrove<Str> values, bool comma = true)
{
  Str out ("\t");
  out.Append ("\"" + Escape (key) + "\": [");

  int64 num_values = values.Count ();
  for (int64 i = 0; i < num_values; i++)
    {
      out.Append ("\"" + Escape (values.Nth (i)) + "\"");
      if (i + 1 != num_values)
        out.Append (", ");
    }
  out.Append ("]");
  if (comma)
    out.Append (",");
  out.Append ("\n");

  return out;
}

Str JSONPair (const Str &key, int64 value, bool comma = true)
{
  Str out;

  // Match print_info()'s behavior.
  if (value < 0)
    return out;

  out.Append ("\t");
  out.Append ("\"" + Escape (key) + "\": ");
  out.Append (Str ().Sprintf ("%" OB_FMT_64 "d", value));
  if (comma)
    out.Append (",");
  out.Append ("\n");

  return out;
}

Str JSONPair (const Str &key, const Str &value, bool comma = true)
{
  Str out ("\t");
  out.Append ("\"" + Escape (key) + "\": ");
  out.Append ("\"" + Escape (value) + "\"");
  if (comma)
    out.Append (",");
  out.Append ("\n");

  return out;
}

Str ObInfo::ObInfoToYAML ()
{
  // This is light JSON (*cough* yaml *cough*) output thingy
  Str output ("{\n");
  output.Append (
    JSONPair ("plasma-version", GetInfo (ObInfo::Version_Of::Gspeak)));
  output.Append (JSONPair ("abi-version", GetInfo (ObInfo::Version_Of::Abi)));
  output.Append (
    JSONPair ("compiler-version", GetInfo (ObInfo::Version_Of::Compiler)));
  output.Append (
    JSONPair ("bit-width", GetInfo (ObInfo::Number_Of::Bits_In_Word)));
  output.Append (
    JSONPair ("build-version", GetInfo (ObInfo::Version_Of::Build)));
  output.Append (
    JSONPair ("ob-prefix-dir", GetInfo (ObInfo::Location_Of::Prefix)));
  output.Append (JSONPair ("os-version", GetInfo (ObInfo::Version_Of::Os)));
  output.Append (
    JSONPair ("kernel-version", GetInfo (ObInfo::Version_Of::Kernel)));
  output.Append (JSONPair ("libc-version", GetInfo (ObInfo::Version_Of::Libc)));
  output.Append (
    JSONPair ("yobuild-version", GetInfo (ObInfo::Version_Of::Yobuild)));
  output.Append (
    JSONPair ("machine-version", GetInfo (ObInfo::Version_Of::Machine)));
  output.Append (JSONPair ("cpu-version", GetInfo (ObInfo::Version_Of::Cpu)));
  output.Append (JSONPair ("cpu-features", GetCPUFeatureFlags ()));
  output.Append (JSONPair ("cpu-mhz", GetInfo (ObInfo::Number_Of::Cpu_Mhz)));
  output.Append (
    JSONPair ("number-of-cores", GetInfo (ObInfo::Number_Of::Cpu_Cores)));
  output.Append (JSONPair ("ram-megabytes", GetInfo (ObInfo::Number_Of::Ram)));
  output.Append (
    JSONPair ("swap-megabytes", GetInfo (ObInfo::Number_Of::Swap)));
  output.Append (
    JSONPair ("ob-share-path", GetInfo (ObInfo::Location_Of::Share)));
  output.Append (JSONPair ("ob-etc-path", GetInfo (ObInfo::Location_Of::Etc)));
  output.Append (JSONPair ("ob-var-path", GetInfo (ObInfo::Location_Of::Var)));
  output.Append (JSONPair ("ob-tmp-dir", GetInfo (ObInfo::Location_Of::Tmp)));
  output.Append (
    JSONPair ("ob-pools-dir", GetInfo (ObInfo::Location_Of::Pools)));
  output.Append (
    JSONPair ("ob-yobuild-dir", GetInfo (ObInfo::Location_Of::Yobuild), false));
  output.Append ("}");

  return output;
}
