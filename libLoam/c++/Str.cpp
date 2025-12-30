
/* (c)  oblong industries */

#include <libLoam/c++/Str.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#include <unicode/ustring.h>

#include <stdarg.h>
#include <stdio.h>

#include <libLoam/c/ob-log.h>
#include <libLoam/c/ob-sys.h>
#include "libLoam/c/ob-hash.h"

#include <libLoam/c++/Preterite.h>
#include <libLoam/c++/StrMatchData.h>
#include <libLoam/c++/LoamStreams.h>

#include <ostream>
#include <algorithm>
#include <utility>

using namespace oblong::loam;


const Str Str::Null;


static void Heinous_UTF8 (const char *str, long str_start, long char_start,
                          long char_end, long str_end)
{
  str_start = (std::max) (str_start, char_start - 4);
  str_end = (std::min) (str_end, char_end + 4);

  /* Slightly dangerous to use Str when handling a Str error,
   * but we're careful not to cause the same error and infinite loop. */
  Str pos ("pos:");
  Str val ("val:");
  Str caret ("    ");
  for (long i = str_start; i < str_end; i++)
    {
      pos.Append (' ');
      val.Append (' ');
      caret.Append (' ');
      unsigned int x = str[i];
      x &= 0xff;
      pos.Append (Str::Format ("%2ld", i));
      val.Append (Str::Format ("%02x", x));
      while (val.Length () < pos.Length ())
        val.Append (' ');
      while (caret.Length () < val.Length ())
        caret.Append ((i >= char_start && i < char_end) ? '^' : ' ');
    }

  // Yes, I hate trailing whitespace that much...
  pos.ReplaceFirst ("\\s+$", "");
  val.ReplaceFirst ("\\s+$", "");
  caret.ReplaceFirst ("\\s+$", "");
  OB_LOG_WARNING_CODE (0x11000004, "Invalid UTF-8 sequence:\n%s\n%s\n%s",
                       pos.utf8 (), val.utf8 (), caret.utf8 ());
}

static str_array Split_Like_Its_1972 (const char *u8, size_t len, char c)
{
  str_array ret (2.0);
  ret.EnsureRoomFor (16);
  const Str empty;

  for (;;)
    {
      const char *next = len ? (const char *) memchr (u8, c, len) : NULL;
      if (next == NULL)
        {
          ret.Append (empty);
          ret.Nth (-1).Set (u8, len);
          break;
        }
      else
        {
          ret.Append (empty);
          ret.Nth (-1).Set (u8, next - u8);
          size_t skip = 1 + (next - u8);
          len -= skip;
          u8 += skip;
        }
    }

  return ret;
}

void Str::_Init () noexcept
{
  u8 = NULL;
  u8IsStale = true;
  u8ByteLength = u8ByteCapacity = 0;
  uString = NULL;
  uStringIsStale = true;
  codePointLength = -1;
  match = NULL;
}

Str::Str () noexcept
{
  _Init ();
}

Str::Str (std::nullptr_t) noexcept : Str () {}

Str::Str (const char *text)
{
  _Init ();
  if (text)
    _SetU8 (text, strlen (text));
}

Str::Str (const char *text, int64 length)
{
  _Init ();
  if (text)
    _SetU8 (text, length);
}

#if defined _MSC_VER
Str::Str (const wchar_t *text)
{
  _Init ();
  if (text)
    {
      uString = new UnicodeString (text);
      if (uString->isBogus ())
        {
          Del_Ptr (uString);
          uStringIsStale = true;
        }
      else
        uStringIsStale = false;
    }
}

Str::Str (const wchar_t *text, int64 length)
{
  _Init ();
  if (text)
    {
      uString = new UnicodeString (text, length);
      if (uString->isBogus ())
        {
          Del_Ptr (uString);
          uStringIsStale = true;
        }
      else
        uStringIsStale = false;
    }
}
#endif

#if defined __linux__ && U_ICU_VERSION_MAJOR_NUM >= 60
Str::Str (const char16_t *text)
{
  _Init ();
  if (text)
    {
      uString = new UnicodeString (text);
      if (uString->isBogus ())
        {
          Del_Ptr (uString);
          uStringIsStale = true;
        }
      else
        uStringIsStale = false;
    }
}

Str::Str (const char16_t *text, int64 length)
{
  _Init ();
  if (text)
    {
      uString = new UnicodeString (text, length);
      if (uString->isBogus ())
        {
          Del_Ptr (uString);
          uStringIsStale = true;
        }
      else
        uStringIsStale = false;
    }
}
#endif

Str::Str (const UnicodeString &text)
{
  uString = new UnicodeString (text);
  if (uString->isBogus ())
    {
      Del_Ptr (uString);
      uStringIsStale = true;
    }
  else
    uStringIsStale = false;
  u8 = NULL;
  u8IsStale = true;
  u8ByteLength = u8ByteCapacity = 0;
  codePointLength = -1;
  match = NULL;
}

Str::Str (const UnicodeString &text, int64 start_marker, int64 length)
{
  uString = new UnicodeString (text, start_marker, (int32_t) length);
  if (uString->isBogus ())
    {
      Del_Ptr (uString);
      uStringIsStale = true;
    }
  else
    uStringIsStale = false;
  u8 = NULL;
  u8IsStale = true;
  u8ByteLength = u8ByteCapacity = 0;
  codePointLength = -1;
  match = NULL;
}

Str::Str (const Str &text)
{
  u8 = NULL;
  u8IsStale = true;
  uString = NULL;
  uStringIsStale = true;
  codePointLength = -1;
  match = NULL;
  _CopyFieldsIntoThis (text);
  _CopyMatchDataIntoThis (text);
}

Str::Str (Str &&text) noexcept
    : u8 (text.u8),
      u8IsStale (text.u8IsStale),
      u8ByteLength (text.u8ByteLength),
      u8ByteCapacity (text.u8ByteCapacity),
      uString (text.uString),
      uStringIsStale (text.uStringIsStale),
      codePointLength (text.codePointLength),
      match (text.match)
{
  if (match)
    match->SetBackPointer (this);

  text.u8 = NULL;
  text.u8IsStale = true;
  text.u8ByteLength = 0;
  text.u8ByteCapacity = 0;
  text.uString = NULL;
  text.uStringIsStale = true;
  text.codePointLength = -1;
  text.match = NULL;
}


Str::Str (const UChar32 code_point)
{
  uString = new UnicodeString (code_point);
  if (uString->isBogus ())
    {
      Del_Ptr (uString);
      uStringIsStale = true;
    }
  else
    uStringIsStale = false;
  u8 = NULL;
  u8IsStale = true;
  u8ByteLength = u8ByteCapacity = 0;
  codePointLength = -1;
  match = NULL;
}

Str &Str::operator= (const Str &text)
{
  if (this == &text)
    return *this;

  Del_Ptr (match);
  Del_Ptr (uString);

  _CopyFieldsIntoThis (text);
  _CopyMatchDataIntoThis (text);
  return *this;
}

Str &Str::operator= (Str &&text) noexcept
{
  using std::swap;

  if (this == &text)
    return *this;

  Str tmp (std::move (*this));

  swap (u8, text.u8);
  swap (u8IsStale, text.u8IsStale);
  swap (u8ByteLength, text.u8ByteLength);
  swap (u8ByteCapacity, text.u8ByteCapacity);
  swap (uString, text.uString);
  swap (uStringIsStale, text.uStringIsStale);
  swap (codePointLength, text.codePointLength);
  swap (match, text.match);

  if (match)
    match->SetBackPointer (this);

  return *this;
}

Str::~Str ()
{
  free (u8);
  Del_Ptr (match);
  Del_Ptr (uString);
}

Str &Str::Set (const Str &text)
{
  if (this == &text)
    return *this;

  Del_Ptr (match);
  Del_Ptr (uString);

  _CopyFieldsIntoThis (text);
  return *this;
}

Str &Str::Set (const char *text)
{
  Del_Ptr (match);
  Del_Ptr (uString);

  if (text)
    _SetU8 (text, strlen (text));
  else
    {
      free (u8);
      u8 = NULL;
      u8IsStale = true;
    }

  uString = NULL;
  uStringIsStale = true;
  codePointLength = -1;
  match = NULL;

  return *this;
}

Str &Str::Set (const char *text, int64 length)
{
  Del_Ptr (match);
  Del_Ptr (uString);

  if (text)
    _SetU8 (text, length);
  else
    {
      free (u8);
      u8 = NULL;
      u8IsStale = true;
    }

  uString = NULL;
  uStringIsStale = true;
  codePointLength = -1;
  match = NULL;

  return *this;
}

Str &Str::Set (const UnicodeString &text)
{
  free (u8);
  Del_Ptr (match);
  if (uString)
    uString->setTo (text);
  else
    uString = new UnicodeString (text);
  if (uString->isBogus ())
    {
      Del_Ptr (uString);
      uStringIsStale = true;
    }
  else
    uStringIsStale = false;
  u8 = NULL;
  u8IsStale = true;
  u8ByteLength = u8ByteCapacity = 0;
  codePointLength = -1;
  return *this;
}

Str &Str::Sprintf (const char *format, ...)
{
  va_list args;
  va_start (args, format);
  Str s = VFormat (format, args);
  va_end (args);
  Set (s);
  return *this;
}

Str Str::Format (const char *format, ...)
{
  va_list args;
  va_start (args, format);
  Str ret = VFormat (format, args);
  va_end (args);
  return ret;
}

Str Str::VFormat (const char *format, va_list args)
{
  char *strbuf = NULL;
  Str ret;
  if (vasprintf (&strbuf, format, args) < 0)
    OB_FATAL_BUG_CODE (0x11000000, "fatal: vasprintf of '%s' failed!\n",
                       format);

  ret.u8 = strbuf;
  ret.u8IsStale = false;
  ret.u8ByteLength = ret.u8ByteCapacity = strlen (strbuf);
  return ret;
}

Str Str::Dup () const
{
  return Str (*this);
}

const char *Str::utf8 () const
{
  if (IsNull ())
    return "";
  _FreshenU8 ();
  return u8;
}

const UnicodeString &Str::ICUUnicodeString () const
{
  if (IsNull ())
    {
      static UnicodeString empty_ustring = UnicodeString ("");
      return empty_ustring;
    }
  _FreshenUString ();
  return *uString;
}


int64 Str::Length () const
{
  if (IsNull ())
    return 0;
  // if we have a cached length, return it
  if (codePointLength > -1)
    return codePointLength;

  if (uString && !uStringIsStale)
    return codePointLength = uString->countChar32 ();
  else if (u8 && !u8IsStale)
    {
      codePointLength = UTF8_Length (u8, 0, u8ByteLength);
      if (codePointLength < 0)
        {
          /* This means the UTF8 was invalid.  In this case, we turn
           * it over to ICU, because ICU does something reasonable.
           * (It replaces invalid UTF-8 sequences with the replacement
           * character, U+FFFD.)  This avoids bug 2865. */
          _FreshenUString ();
          codePointLength = uString->countChar32 ();
        }
      return codePointLength;
    }

  // seems like we've got nothing. return 0.
  return 0;
}

int64 Str::ByteLength () const
{
  _FreshenU8 ();
  return u8ByteLength;
}

UChar32 Str::At (int64 index) const
{
  if (IsNull ())
    return 0;

  int64 len = Length ();
  // negative offsets wrap backwards
  if (index < 0)
    index = len + index;
  // out of bounds offsets return 0
  if (index < 0 || index >= len)
    return 0;

  if (uString && !uStringIsStale)
    {
      int32 idx = uString->moveIndex32 (0, index);
      return uString->char32At (idx);
    }

  else if (u8 && !u8IsStale)
    {
      int32 idx = 0;
      UChar32 chr;
      U8_FWD_N ((uint8_t *) u8, idx, u8ByteLength, index);
      U8_GET_UNSAFE ((uint8_t *) u8, idx, chr);
      return chr;
    }
  return 0;
}


Str Str::Slice (int64 index) const
{
  int64 str_len = Length ();
  if (index < 0)
    index = str_len + index;
  int64 len = str_len - index;
  return Slice (index, len);
}

Str Str::Slice (int64 index, int64 length) const
{
  if (IsNull ())
    return Str ("");

  if (index < 0)  // negative offsets wrap backwards
    index = Length () + index;
  if (length < 1)
    return Str ();
  _FreshenUString ();
  int32 idx = uString->moveIndex32 (0, index);
  int32 jdx = uString->moveIndex32 (idx, length);
  UnicodeString u_slice;
  uString->extract (idx, (jdx - idx), u_slice);
  Str slice (u_slice);
  return slice;
}

Str Str::Concat (const Str &other) const
{
  Str longer (*this);
  longer.Append (other);
  return longer;
}

Str Str::Concat (const UChar32 code_point) const
{
  return Concat (Str (code_point));
}

Str &Str::Append (const Str &other)
{
  _FreshenUString ();
  uString->append (other.ICUUnicodeString ());
  u8IsStale = true;
  codePointLength = -1;
  Match ();
  return *this;
}

Str &Str::Append (const UChar32 code_point)
{
  return Append (Str (code_point));
}

Str &Str::Insert (int64 index, const Str &other)
{
  if (index < 0)
    index = Length () + index;

  _FreshenUString ();
  int32 idx = uString->moveIndex32 (0, index);
  uString->insert (idx, other.ICUUnicodeString ());
  u8IsStale = true;
  codePointLength = -1;
  Match ();
  return *this;
}

Str &Str::Insert (int64 index, const UChar32 code_point)
{
  return Insert (index, Str (code_point));
}

int Str::Compare (const Str &other) const
{
  if (IsNull ())
    {
      if (other.IsNull ())
        return 0;
      else
        return -1;
    }
  if (other.IsNull ())
    return 1;

  _FreshenUString ();
  return uString->compareCodePointOrder (other.ICUUnicodeString ());
}

int Str::CaseCmp (const Str &other) const
{
  if (IsNull ())
    {
      if (other.IsNull ())
        return 0;
      else
        return -1;
    }
  _FreshenUString ();
  return uString->caseCompare (other.ICUUnicodeString (),
                               U_COMPARE_CODE_POINT_ORDER);
}

bool Str::operator< (const Str &other) const
{
  return (Compare (other) < 0);
}

bool Str::operator> (const Str &other) const
{
  return (Compare (other) > 0);
}

// bool Str::operator == (const Str &other)  const
// { return ! Compare (other); }

// bool Str::operator == (const char *other)  const
// { return ! Compare (Str (other)); }

// bool Str::operator != (const Str &other)  const
// { return Compare (other); }

// bool Str::operator != (const char *other)  const
// { return Compare (Str (other)); }


// bool operator == (const char *str_c, const Str &str)
// { return ! str . Compare (Str (str_c)); }

// bool operator != (const char *str_c, const Str &str)
// { return str . Compare (Str (str_c)); }

Str oblong::loam::operator+ (const UChar32 code_point, const Str &str)
{
  return Str (code_point) + str;
}

Str oblong::loam::operator+ (const char *str_c, const Str &str)
{
  return Str (str_c) + str;
}


Str &Str::Upcase ()
{
  _FreshenUString ();
  uString->toUpper ();
  u8IsStale = true;
  // Yes, upcasing a string can change its length (bug 1233, code point U+0149)
  codePointLength = -1;
  Match ();
  return *this;
}

Str &Str::Downcase ()
{
  _FreshenUString ();
  uString->toLower ();
  u8IsStale = true;
  // Since upcasing can change the length, presumably downcasing can, too
  codePointLength = -1;
  Match ();
  return *this;
}

Str &Str::Strip ()
{
  _FreshenUString ();
  uString->trim ();
  u8IsStale = true;
  codePointLength = -1;
  Match ();
  return *this;
}

Str &Str::Chomp (const Str &separator)
{
  _FreshenUString ();
  UnicodeString sep (separator.ICUUnicodeString ());
  if (uString->endsWith (sep))
    {
      uString->truncate (uString->length () - sep.length ());
      u8IsStale = true;
      codePointLength = -1;
    }
  Match ();
  return *this;
}

Str &Str::Reverse ()
{
  _FreshenUString ();
  int32 len = uString->length ();
  // Check whether this string might be subject to this bug that
  // can affect odd-length strings:
  // http://bugs.icu-project.org/trac/ticket/8091
  if (len > 1 && (len & 1) == 1 && U_IS_LEAD (uString->charAt (len / 2)))
    {
      // Work around the bug by adding a superfluous character to make the
      // string length even.
      const UChar superfluous (0xfeff);
      uString->insert (0, superfluous);
      uString->reverse ();
      if (uString->charAt (len) == superfluous)
        uString->truncate (len);
    }
  else
    uString->reverse ();
  u8IsStale = true;
  Match ();
  return *this;
}

int64 Str::_CodeUnitIndexToCodePointIndex (int32 cuidx) const
{
  if (cuidx <= 0)
    return cuidx;
  else
    return uString->countChar32 (0, cuidx);
}

int64 Str::Index (const Str &substring) const
{
  if (IsNull ())
    return -1;
  _FreshenUString ();
  return _CodeUnitIndexToCodePointIndex (
    uString->indexOf (substring.ICUUnicodeString ()));
}

int64 Str::Rindex (const Str &substring) const
{
  if (IsNull ())
    return -1;
  _FreshenUString ();
  return _CodeUnitIndexToCodePointIndex (
    uString->lastIndexOf (substring.ICUUnicodeString ()));
}

bool Str::Contains (const Str &substring) const
{
  if (substring.IsEmpty ())
    return true;
  return (!IsEmpty () && Index (substring) != -1);
}

bool Str::IsEmpty () const
{
  if (IsNull ())
    return true;
  if (!u8IsStale && u8ByteLength == 0)
    return true;
  if (!uStringIsStale && uString->isEmpty ())
    return true;
  return false;
}

void Str::SetToEmpty ()
{
  Set ("");
  Match ();
}

bool Str::IsNull () const
{
  return (!u8 && !uString);
}


void Str::SetToNull ()
{
  Set (NULL);
  Match ();
}

// ----


bool Str::Matches (const Str &pattern) const
{
  if (IsNull ())
    return false;
  StrMatchData mattie (const_cast<Str *> (this), pattern);
  if (mattie.IsInError ())
    OB_LOG_WARNING_CODE (0x11000006, "failed to prepare matcher for '%s': %s",
                         pattern.utf8 (), mattie.ErrorStr ().utf8 ());
  else
    mattie.Match ();
  return bool (mattie);
}

bool Str::Match (const Str &pattern)
{
  if (IsNull ())
    return false;
  Del_Ptr (match);
  match = new StrMatchData (const_cast<Str *> (this), pattern);
  if (match->IsInError ())
    OB_LOG_WARNING_CODE (0x11000005, "failed to prepare matcher for '%s': %s",
                         pattern.utf8 (), match->ErrorStr ().utf8 ());
  else
    match->Match ();
  return bool (*match);
}

bool Str::Match ()
{
  Del_Ptr (match);
  return false;
}

bool Str::MatchAgain ()
{
  if (!match)
    return false;
  match->Match ();
  return bool (*match);
}

bool Str::MatchHasMatched () const
{
  if (match)
    return bool (*match);
  return false;
}

bool Str::MatchHasErred () const
{
  if (match)
    return match->IsInError ();
  return false;
}

Str Str::MatchErrorStr () const
{
  if (match)
    return match->ErrorStr ();
  return Str ("");
}

Str Str::MatchSlice () const
{
  if (match)
    return match->MatchSlice ();
  return Str ("");
}

Str Str::MatchPreSlice () const
{
  if (match)
    return match->PreMatchSlice ();
  return Str ("");
}

Str Str::MatchPostSlice () const
{
  if (match)
    return match->PostMatchSlice ();
  return Str ("");
}

int Str::MatchNumCaptures () const
{
  if (match)
    return match->NumCaptures ();
  return 0;
}

Str Str::MatchNthCaptureSlice (int which_capture) const
{
  if (match)
    return match->NthCaptureSlice (which_capture);
  return Str ("");
}

StrRange Str::MatchRange () const
{
  return MatchNthCaptureRange (0);
}

StrRange Str::MatchNthCaptureRange (int which_capture) const
{
  if (match)
    return match->NthCaptureRange (which_capture);
  StrRange ret;
  ret.idx = -1;
  ret.len = -1;
  return ret;
}

Str &Str::Replace (int64 index, int64 length, const Str &replacement)
{
  if (IsNull ())
    return *this;

  // negative offsets wrap backwards
  if (index < 0)
    index += Length ();
  _FreshenUString ();
  int32 idx = uString->moveIndex32 (0, index);
  int32 jdx = uString->moveIndex32 (idx, length);
  uString->replace (idx, (jdx - idx), replacement.ICUUnicodeString ());
  u8IsStale = true;
  codePointLength = -1;
  Match ();
  return (*this);
}

Str &Str::Replace (int64 index, const UChar32 replacement)
{
  if (IsNull ())
    return *this;

  // negative offsets wrap backwards
  if (index < 0)
    index += Length ();
  _FreshenUString ();
  int32 idx = uString->moveIndex32 (0, index);
  int32 jdx = uString->moveIndex32 (idx, 1);
  uString->replace (idx, (jdx - idx), replacement);
  u8IsStale = true;
  // Since we replaced exactly one code point with exactly one code point,
  // (even though they may be different numbers of code units), we shouldn't
  // need to invalidate the length... in most cases.  However, the way this
  // method is currently written (though not sure whether it's what was
  // intended or not), it's possible to "replace" a character at a position
  // after the end of the string, which basically appends a character.
  // So, in that particular case, the code point length will change.
  // We could detect that case and only invalidate the code point length in
  // that case, but it seems safer to just always invalidate it.
  codePointLength = -1;
  Match ();
  return (*this);
}

Str &Str::ReplaceAll (const Str &pattern, const Str &replacement)
{
  return _Replace (pattern, replacement, &StrMatchData::ReplaceAll);
}

Str &Str::ReplaceFirst (const Str &pattern, const Str &replacement)
{
  return _Replace (pattern, replacement, &StrMatchData::ReplaceFirst);
}

Str &Str::_Replace (const Str &pattern, const Str &replacement,
                    ReplaceFunc func)
{
  if (IsNull ())
    return *this;

  Del_Ptr (match);
  match = new StrMatchData (this, pattern);
  if (match->IsInError ())
    return *this;

  (match->*func) (replacement);
  u8IsStale = true;
  codePointLength = -1;
  // Del_Ptr (match);
  return *this;
}

#define METACHAR(x) (OB_CONST_U64 (1) << ((x) - ' '))

str_array Str::Split (const Str &pattern) const
{
  if (IsEmpty ())
    return str_array ();

  /* This list of metacharacters is taken from gRuleSet_rule_char_pattern
   * in icu/source/i18n/regexst.cpp of ICU.
   * Well, except that { | } aren't on this list, because they are handled
   * by the c < '{' expression in the code below.  (I was lazy and just
   * made a 64-bit mask, which covers all the metacharacters except those.) */
  const unt64 metas = METACHAR ('*') | METACHAR ('?') | METACHAR ('+')
                      | METACHAR ('[') | METACHAR ('(') | METACHAR (')')
                      | METACHAR ('^') | METACHAR ('$') | METACHAR ('\\')
                      | METACHAR ('.');
  char c;

  // optimize for the common case
  if (!u8IsStale && pattern.u8 && !pattern.u8IsStale  // u8 not stale
      && pattern.u8ByteLength == 1                    // one-char pattern
      && (c = pattern.u8[0]) != 0 && c < '{'          // not a metacharacter
      && (c < ' ' || c > '^' || 0 == (1 & (metas >> (c - ' ')))))
    return Split_Like_Its_1972 (u8, u8ByteLength, c);

  StrMatchData m (const_cast<Str *> (this), pattern);
  if (m.IsInError ())
    return str_array ();

  return m.Split ();
}

#undef METACHAR

// ----

void Str::_FreshenUString () const
{
  if (uString && !uStringIsStale)
    return;

  if (u8 && !u8IsStale)
    {
      if (uString)
        delete uString;
      uString = new UnicodeString (u8, "UTF-8");
      if (!uString->isBogus ())
        uStringIsStale = false;
    }
  else
    {
      uString = new UnicodeString ("", "UTF-8");
      uStringIsStale = false;
    }
}

void Str::_FreshenU8 () const
{
  if (u8 && !u8IsStale)
    return;

  // yeah, icky, calling non-const methods from a const method
  Str *self = const_cast<Str *> (this);

  if (!uString || uString->isEmpty ())
    {
      self->_SetU8 ("", 0);
      return;
    }

  // okay, so we need to convert. let's preflight first, to get the
  // byte length of the u8 array
  UErrorCode errorCode = U_ZERO_ERROR;
  // No!  Not the number of code points!  (That caused bug 1240)
  // const int32 ulen = Length ();
  // We want the number of UTF-16 code *units*.
  const int32 ulen = uString->length ();
  const UChar *u16_buff = uString->getBuffer ();
  int32 u8_len;
  u_strToUTF8 (u8, 0, &u8_len, u16_buff, ulen, &errorCode);
  uString->releaseBuffer ();

  // we expect U_BUFFER_OVERFLOW_ERROR
  if (U_FAILURE (errorCode) && errorCode != U_BUFFER_OVERFLOW_ERROR)
    {
      OB_LOG_BUG_CODE (0x11000001, "u_strToUTF8 returned %s\n",
                       u_errorName (errorCode));
      // error converting - set u8 to the empty string
      self->_SetU8 ("", 0);
      return;
    }

  // realloc memory if necessary. note that we grow, but we never
  // shrink. cogitate at some point on whether this is the right
  // memory usage model for our Str class.
  self->_EnsureU8Capacity (u8_len);

  // and the final conversion
  errorCode = U_ZERO_ERROR;
  u16_buff = uString->getBuffer ();
  u_strToUTF8 (u8, u8_len, NULL, u16_buff, ulen, &errorCode);
  uString->releaseBuffer ();
  self->_SetU8Length (u8_len);
  if (U_FAILURE (errorCode))
    OB_LOG_BUG_CODE (0x11000002, "u_strToUTF8 returned %s\n",
                     u_errorName (errorCode));
}


str_marker Str::MoveMarker (str_marker marker, int64 distance) const
{
  if (IsNull ())
    return marker;
  if (distance == 0)
    return marker;

  if (uString && !uStringIsStale)
    {
      if (uString->length () == 0)
        return 0;
      if (marker == 0 && distance < 0)
        return -1;
      int64 new_idx = (int64) uString->moveIndex32 (marker, distance);
      return new_idx;
    }

  else if (u8 && !u8IsStale)
    {
      if (distance > 0)
        {
          U8_FWD_N ((uint8_t *) u8, marker, u8ByteLength, distance);
          if (marker > u8ByteLength)
            marker = u8ByteLength;
        }
      else
        {
          if (u8ByteLength == 0)
            return 0;
          // we unroll U8_BACK_N here, so we can peg to -1 on attempts
          // to decrement past the beginning of the string
          while (distance < 0 && marker > 0)
            {
              U8_BACK_1 ((uint8_t *) u8, 0, marker);
              ++distance;
            }
          if (distance < 0)
            marker = -1;
        }
      return marker;
    }

  return marker;
}

UChar32 Str::Fetch (str_marker marker) const
{
  if (IsNull ())
    return 0;
  if (uString && !uStringIsStale)
    {
      UChar32 c = uString->char32At (marker);
      if (c == 0xffff)
        return 0;
      else
        return c;
    }
  else if (u8 && !u8IsStale)
    {
      UChar32 chr;
      U8_GET_UNSAFE ((uint8_t *) u8, marker, chr);
      return chr;
    }
  return 0;
}

UChar32 Str::FetchAndMoveMarker (str_marker *marker, int64 distance) const
{
  if (IsNull ())
    return 0;
  UChar32 c = Fetch (*marker);
  if (distance)
    {
      int64 new_marker = MoveMarker (*marker, distance);
      *marker = new_marker;
    }
  return c;
}

str_marker Str::begin_marker () const
{
  return 0;
}


str_marker Str::end_marker () const
{
  if (IsNull ())
    return 0;
  if (u8 && !u8IsStale)
    return MoveMarker (u8ByteLength, -1);
  else if (uString && !uStringIsStale)
    return MoveMarker (uString->length (), -1);
  return 0;
}

str_marker Str::begin_bounds_marker () const
{
  if (Length () > 0)
    return -1;
  else
    return 0;
}

str_marker Str::end_bounds_marker () const
{
  if (IsNull ())
    return 0;
  if (Length () > 0)
    return end_marker () + 1;
  else
    return 0;
}


int64 Str::UTF8_Length (const char *str)
{
  return UTF8_Length (str, 0, strlen (str));
}


int64 Str::UTF8_Length (const char *str, int64 index, int64 byte_length)
{
  int32 i = index;
  int64 cnt = 0;
  UChar32 chr;

  while (str + i < str + byte_length)
    {
      const int32 old_i = i;
      U8_NEXT (str, i, byte_length, chr);
      if (chr < 0)  // parse error
        {
          Heinous_UTF8 (str, index, old_i, i, byte_length);
          return -1;
        }
      ++cnt;
    }
  return cnt;
}

void Str::_CopyFieldsIntoThis (const Str &to_copy)
{
  if ((u8IsStale = to_copy.u8IsStale) || to_copy.u8 == NULL)
    Free_Ptr (u8);
  else
    _SetU8 (to_copy.u8, to_copy.u8ByteLength);

  if ((uStringIsStale = to_copy.uStringIsStale))
    uString = NULL;
  else
    uString = new UnicodeString (*to_copy.uString);

  codePointLength = to_copy.codePointLength;
}

void Str::_CopyMatchDataIntoThis (const Str &to_copy)
{
  if (to_copy.match)
    match = new StrMatchData (this, *to_copy.match);
  else
    match = NULL;
}

void Str::_EnsureU8Capacity (int64 capacity)
{
  if (!u8)
    {
      u8 = (char *) malloc (1 + capacity);
      u8ByteCapacity = capacity;
    }
  else if (capacity > u8ByteCapacity)
    {
      char *r = (char *) realloc (u8, 1 + capacity);
      if (r)
        {
          u8 = r;
          u8ByteCapacity = capacity;
        }
    }
}

void Str::_SetU8Length (int64 length)
{
  if (!u8 || u8ByteCapacity < length)
    OB_FATAL_BUG_CODE (0x11000003,
                       "u8 = %p, u8ByteCapacity = %" OB_FMT_64 "d, "
                       "length = %" OB_FMT_64 "d\n",
                       u8, u8ByteCapacity, length);
  u8[length] = 0;
  u8ByteLength = length;
  u8IsStale = false;
}

void Str::_SetU8 (const char *str, int64 length)
{
  _EnsureU8Capacity (length);
  memcpy (u8, str, length);
  _SetU8Length (length);
}

unt64 Str::Hash () const
{
  if (IsNull ())
    return OB_CONST_U64 (0x12345678deadbeef);
  _FreshenU8 ();
  return ob_city_hash64 (u8, u8ByteLength);
}

namespace oblong {
namespace loam {

::std::ostream &operator<< (::std::ostream &os, const oblong::loam::Str &s)
{
  return os << static_cast<const char *> (s);
}
}
}  // end oblong::loam
