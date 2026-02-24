
/* Simple argument-parsing class
 * Copyright (C) 2001 Mignon Pelletier <code at funwithsoftware dot org>
 * Copyright (C) 2009-2010 Oblong Industries */

#include <libLoam/c++/ArgParse.h>

#include <libLoam/c/ob-string.h>
#include <libLoam/c/ob-dirs.h>
#include <libLoam/c/ob-log.h>
#include <libLoam/c/ob-util.h>
#include <libLoam/c/ob-vers.h>
#include <libLoam/c/ob-sys.h>

#include <unicode/msgfmt.h>

#include <map>
#include <list>
#include <functional>
#include <limits.h>
#include <iostream>

using namespace oblong::loam;
using namespace icu;

const int ArgParse::UNLIMITED = INT_MAX;

/// Same as the static method MessageFormat::format(), except
/// we use the "C" locale instead of the user's locale, for
/// consistent results.  (e. g. no commas in integers)
/// XXX: may want to change this someday if we support internationalization
static void my_format (const UnicodeString &pattern,
                       const Formattable *arguments, int32_t cnt,
                       UnicodeString &appendTo, UErrorCode &success)
{
  Locale c ("en", "US", "POSIX");
  MessageFormat temp (pattern, c, success);
  FieldPosition ignore (0);
  temp.format (arguments, cnt, appendTo, ignore, success);
}

static Str format_default (ArgParse::apint n)
{
  UnicodeString result;
  UErrorCode err = U_ZERO_ERROR;
  Formattable arguments[] = {int64_t (n)};
  my_format ("(default {0})", arguments, 1, result, err);
  if (U_FAILURE (err))
    return u_errorName (err);
  else
    return result;
}

static Str format_default (ArgParse::apfloat n)
{
  UnicodeString result;
  UErrorCode err = U_ZERO_ERROR;
  Formattable arguments[] = {n};
  my_format ("(default {0})", arguments, 1, result, err);
  if (U_FAILURE (err))
    return u_errorName (err);
  else
    return result;
}


class OptionHandler
{
 public:
  ArgParse::apstring usage;
  int separator;
  ArgParse::apstringvec aliases;

  OptionHandler (ArgParse::apstring usage_in,
                 int separator_in = ArgParse::SEP_NONE);
  virtual ~OptionHandler ();
  // Returns true if the option takes an argument.  This is the
  // same thing as saying it returns false if the option is a flag.
  // (The implementation in the base class returns true, so we
  // only have to override it for FlagHandler.)
  virtual bool takesarg () const;
  // Returns true if we can stick "no" in front of the option.
  // This will be false for everything except flag, and might be
  // true or false for flag.
  // (The implementation in the base class returns false, so we
  // only have to override it for FlagHandler.)
  virtual bool allownegation () const;
  // Resets the internal state of the OptionHandler, so that
  // Parse() can be called more than once.
  // (This has a no-op implementation in the base class,
  // because not all subclasses need to override it.)
  virtual void reset ();
  // This is a wrapper for handlearg() which first splits up the
  // argument according to "separator".  As an additional, unrelated
  // function, it also will append "prefix" to any non-empty error message.
  ArgParse::apstring handleargsplit (ArgParse::apstring arg,
                                     ArgParse::apstring prefix);

 protected:
  // Takes an argument and sticks it somewhere.  Returns the empty
  // string on success, or an error message on failure.  If
  // takesarg() is false, then arg is either "no" or the empty string.
  virtual ArgParse::apstring handlearg (ArgParse::apstring arg) = 0;
};

// If first is nonnegative, then this is a usage header, and second is
// the text of the header, and first is the indent.  If first is
// negative, then second is the name of an option.
typedef std::pair<int, ArgParse::apstring> UsageInfo;

typedef std::map<ArgParse::apstring, OptionHandler *> OptionHandlerMap;
typedef std::pair<ArgParse::apstring, OptionHandler *> OptionHandlerPair;

typedef std::list<UsageInfo> UsageInfoList;

typedef std::map<ArgParse::apstring, ArgParse::apstring> StringMap;
typedef std::pair<ArgParse::apstring, ArgParse::apstring> StringPair;

class oblong::loam::ArgParseInternalData
{
 public:
  bool allowOneCharOptionsToBeCombined;
  bool allowUnrecognizedOptions;
  ArgParse::apstring errmsg;
  ArgParse::apstringvec leftovers;
  OptionHandlerMap options;
  StringMap aliases;
  UsageInfoList usage;

  ArgParseInternalData ();
  void addOption (ArgParse::apstring name, OptionHandler *oh);
  OptionHandler *findOption (ArgParse::apstring name, bool *no);
};

ArgParseInternalData::ArgParseInternalData ()
    : allowOneCharOptionsToBeCombined (false),
      allowUnrecognizedOptions (false),
      leftovers (2.0)
{
}

void ArgParseInternalData::addOption (ArgParse::apstring name,
                                      OptionHandler *oh)
{
  if (options.find (name) != options.end ())
    {
      /* Okay, they registered the same option name twice.
       * Let's complain about it. */
      OB_LOG_BUG_CODE (0x12000000, "There is already an option '%s'\n",
                       name.utf8 ());
      delete oh;  // since we can't store it anywhere
    }
  else
    {
      options.insert (OptionHandlerPair (name, oh));
      usage.push_back (UsageInfo (-1, name));
    }
}

OptionHandler *ArgParseInternalData::findOption (ArgParse::apstring name,
                                                 bool *no)
{
  // First check if it's an alias.
  StringMap::iterator foundalias = aliases.find (name);
  ArgParse::apstring newname = name;
  if (foundalias != aliases.end ())
    newname = foundalias->second;
  *no = false;
  // Now try to find it by its real name
  OptionHandlerMap::iterator foundoption = options.find (newname);
  if (foundoption != options.end ())
    return foundoption->second;
  // If not found, see if we can find it by stripping "no"
  if (newname.Slice (0, 2) == "no")
    {
      foundoption = options.find (newname.Slice (2, newname.Length () - 2));
      if (foundoption != options.end ())
        {
          // We found it, but is "no" legal for this option?
          OptionHandler *oh = foundoption->second;
          if (oh->allownegation ())
            {
              *no = true;
              return oh;
            }
        }
    }
  return NULL;
}


ArgParse::ArgParse () : easy_help (false)
{
  d = new ArgParseInternalData;
}

ArgParse::ArgParse (int argc, const char *const *argv,
                    const apstring &non_option_arg_description, int indent)
    : easy_help (false)
{
  d = new ArgParseInternalData;
  d->allowOneCharOptionsToBeCombined = true;
  ob_set_program_arguments (argc, argv);
  apstring mySelf (ob_basename (argv[0]));
  mySelf.ReplaceAll ("^lt-", "");  // bug 2288
  apstring usage ("Usage: ");
  usage += mySelf;
  usage += " [options] ";
  usage += non_option_arg_description;
  UsageHeader (usage, indent);
  ArgFlag ("help", "\aprint this help, then exit", &easy_help);
  Alias ("help", "h");
}

ArgParse::~ArgParse ()
{
  Bye ();
}

void ArgParse::Bye ()
{
  if (!d)
    return;  // If Bye() has been called, don't do it again in destructor

  /* Most of the stuff will clean itself up automatically,
   * but since d->options contains pointers we allocated,
   * we need to free them explicitly. */
  for (OptionHandlerMap::iterator e = d->options.begin ();
       e != d->options.end (); e++)
    {
      delete e->second;
      e->second = NULL;
    }
  delete d;
  d = NULL;
}

void ArgParse::Exit (int code)
{
  Bye ();
  exit (code);
}

void ArgParse::EasyFinish (int min_non_option_args, int max_non_option_args)
{
  if (max_non_option_args == 0)  // for convenience
    max_non_option_args = min_non_option_args;
  assert (max_non_option_args >= min_non_option_args);
  int argc;
  const char *const *argv;
  ob_get_program_arguments (&argc, &argv);
  assert (argc > 0);

  if (!Parse (argc - 1, argv + 1))
    {
      std::cerr << ErrorMessage () << std::endl << UsageMessage ();
      Exit (EXIT_FAILURE);
    }

  if (easy_help)
    {
      ob_banner (stdout);
      fflush (stdout);
      std::cout << UsageMessage ();
      Exit (EXIT_SUCCESS);
    }

  int64 von_count = Leftovers ().Count ();
  if (von_count < min_non_option_args || von_count > max_non_option_args)
    {
      std::cerr << "Expected ";
      if (min_non_option_args == max_non_option_args)
        std::cerr << min_non_option_args;
      else
        std::cerr << "between " << min_non_option_args << " and "
                  << max_non_option_args;
      std::cerr << " non-option arguments, but got " << von_count << std::endl;
      std::cerr << UsageMessage ();
      Exit (EXIT_FAILURE);
    }
}

void ArgParse::AllowOneCharOptionsToBeCombined ()
{
  d->allowOneCharOptionsToBeCombined = true;
}

void ArgParse::DontAllowOneCharOptionsToBeCombined ()
{
  d->allowOneCharOptionsToBeCombined = false;
}

void ArgParse::AllowUnrecognizedOptions ()
{
  d->allowUnrecognizedOptions = true;
}

void ArgParse::DontAllowUnrecognizedOptions ()
{
  d->allowUnrecognizedOptions = false;
}

void ArgParse::Alias (apstring realname, apstring aliasname, bool override)
{
  StringMap::iterator found;

  if (d->options.find (aliasname) != d->options.end ())
    {
      OB_LOG_BUG_CODE (0x12000001, "There is already an option '%s'\n",
                       aliasname.utf8 ());
    }
  else if ((found = d->aliases.find (aliasname)) == d->aliases.end ())
    {
      d->aliases.insert (StringPair (aliasname, realname));
      OptionHandlerMap::iterator e = d->options.find (realname);
      if (e != d->options.end ())
        {
          e->second->aliases.Append (aliasname);
        }
      else
        {
          OB_LOG_BUG_CODE (0x12000002, "There is no option '%s'\n",
                           realname.utf8 ());
        }
    }
  else if (override)
    {
      apstring oldname = found->second;
      OptionHandlerMap::iterator old = d->options.find (oldname);
      if (old == d->options.end ())
        OB_FATAL_BUG_CODE (0x12000003, "Invariant violated ('%s' not in map)\n",
                           oldname.utf8 ());
      old->second->aliases.RemoveEvery (aliasname);
      d->aliases[aliasname] = realname;
      OptionHandlerMap::iterator e = d->options.find (realname);
      if (e != d->options.end ())
        {
          e->second->aliases.Append (aliasname);
        }
      else
        {
          OB_LOG_BUG_CODE (0x12000004, "There is no option '%s'\n",
                           realname.utf8 ());
        }
    }
  else
    {
      OB_LOG_BUG_CODE (0x12000005, "There is already an alias '%s'\n",
                       aliasname.utf8 ());
    }
}

void ArgParse::UsageHeader (apstring text, int indent)
{
  d->usage.push_back (UsageInfo (indent, text));
}

ArgParse::apstring ArgParse::ErrorMessage () const
{
  return d->errmsg;
}

const ArgParse::apstringvec &ArgParse::Leftovers () const
{
  return d->leftovers;
}

ArgParse::ArgInfo ArgParse::Query (apstring name) const
{
  ArgInfo ai;
  bool isAlias = false;

  StringMap::iterator foundalias = d->aliases.find (name);
  if (foundalias != d->aliases.end ())
    {
      isAlias = true;
      ai.aliasFor = foundalias->second;
    }

  bool no;
  OptionHandler *oh = d->findOption (name, &no);

  if (isAlias && !oh)
    OB_FATAL_BUG_CODE (0x12000006,
                       "Invariant violated (alias '%s' without a "
                       "real option '%s' "
                       "behind it)\n",
                       name.utf8 (), ai.aliasFor.utf8 ());

  if (oh)
    {
      // copy over entire list
      ai.aliases.Empty ();
      for (int i = 0; i < oh->aliases.Count (); i++)
        ai.aliases.Append (oh->aliases.Nth (i));
      ai.usage = oh->usage;
      if (isAlias)
        ai.typ = TYPE_ALIAS;
      else if (no)
        ai.typ = TYPE_NEGATED;
      else
        ai.typ = TYPE_REAL_OPTION;
    }
  else
    ai.typ = TYPE_NONEXISTENT;

  return ai;
}

ObRetort ArgParse::Parse (const apstringvec &args)
{
  int64 argc = args.Count ();
  assert (argc >= 0 && argc <= UNLIMITED);
  const char **argv = new const char *[argc];
  for (int64 i = 0; i < argc; i++)
    {
      Str s (args.Nth (i));
      // need to copy the utf8 for when s goes out of scope.
      const char *utf8 = s.utf8 ();
      size_t len = 1 + strlen (utf8);
      char *arg = (char *) alloca (len);
      ob_safe_copy_string (arg, len, utf8);
      argv[i] = arg;
    }
  ob_retort tort =
    Parse ((int) argc, argv) ? OB_OK : OB_ARGUMENT_PARSING_FAILED;
  delete[] argv;
  return ObRetort (tort);
}

// See http://c-faq.com/ansi/constmismatch.html
// for exciting information about constness and argv.
bool ArgParse::Parse (int argc, const char *const *argv)
{
  OptionHandler *argeater = NULL;
  OptionHandler *oneshotargeater = NULL;
  bool endofoptions = false;
  const char *eatername = "";

  // These statments are to reset things so we can call
  // Parse() more than once.  This probably isn't a very
  // useful feature, though, so maybe it should be taken
  // out to simplify things.
  d->leftovers.Empty ();
  for (OptionHandlerMap::iterator e = d->options.begin ();
       e != d->options.end (); e++)
    {
      e->second->reset ();
    }
  d->errmsg = "";

  for (int i = 0; i < argc; i++)
    {
      if (d->errmsg.Length () > 0)
        break;
      if (endofoptions)
        {
          d->leftovers.Append (argv[i]);
          continue;
        }
      if (oneshotargeater != NULL)
        {
          d->errmsg = oneshotargeater->handleargsplit (argv[i], eatername);
          oneshotargeater = NULL;
          continue;
        }
      // The argv[i][1] == '\0' part below is so we treat "-"
      // by itself as a regular argument, not as an option,
      // since it's a common abbreviation for stdin, and isn't
      // really meaningful as an option.
      if (argv[i][0] != '-' || argv[i][1] == '\0')
        {
          if (argeater == NULL)
            d->leftovers.Append (argv[i]);
          else
            d->errmsg = argeater->handleargsplit (argv[i], eatername);
          continue;
        }
      // At this point, we know the first character is a dash
      argeater = NULL;
      const char *rest = argv[i] + 1;
      if (*rest == '-')
        {
          rest++;
          if (*rest == '\0')  // just "--" by itself
            {
              endofoptions = true;
              continue;
            }
        }
      int ndashes = rest - argv[i];
      int l;
      for (l = 0; rest[l] != '\0' && rest[l] != ':' && rest[l] != '='; l++)
        ;  // empty; all we do is compute l
      if (d->allowOneCharOptionsToBeCombined && ndashes == 1 && l > 1)
        {
          /* Do all the bundled options except the last one.
           * We will let the last one fall through to the regular
           * logic.  This is because the last option could take
           * an argument, but the others can't. */
          for (int j = 0; j < l - 1; j++)
            {
              apstring onechar (rest + j, 1);
              bool no;
              OptionHandler *oh = d->findOption (onechar, &no);
              if (oh == NULL)
                {
                  d->errmsg = (apstring (argv[i]) + ": '" + onechar
                               + "' is an unrecognized option");
                  /* if only this was Shading Language and I could
                   * say "break 2"... */
                  goto doublebreak;
                }
              if (oh->takesarg ())
                {
                  d->errmsg = (apstring (argv[i]) + ": '" + onechar
                               + "' requires an argument");
                  goto doublebreak;
                }
              d->errmsg = oh->handleargsplit (no ? "no" : "", argv[i]);
              if (d->errmsg.Length () > 0)
                goto doublebreak;
            }
          rest += l - 1;
          l = 1;
        }
      bool no;
      apstring option_name (rest, l);
      OptionHandler *oh = d->findOption (option_name, &no);
      if (oh == NULL)
        {
          if (d->allowUnrecognizedOptions)
            {
              d->leftovers.Append (argv[i]);
              continue;
            }
          else
            {
              d->errmsg = apstring (argv[i]) + ": unrecognized option";
              break;
            }
        }
      if (oh->takesarg ())
        {
          if (rest[l] != '\0')
            {
              d->errmsg = oh->handleargsplit (rest + l + 1, argv[i]);
            }
          else if (oh->separator != SEP_ARGV)
            {
              oneshotargeater = oh;
            }
          if (oh->separator == SEP_ARGV)
            {
              argeater = oh;
            }
          eatername = argv[i];
        }
      else
        {
          if (rest[l] != '\0')
            {
              d->errmsg = apstring (argv[i]) + ": doesn't take an argument";
            }
          else
            {
              d->errmsg = oh->handleargsplit (no ? "no" : "", argv[i]);
            }
        }
    }
doublebreak:
  if (d->errmsg.Length () == 0 && oneshotargeater != NULL)
    d->errmsg = "missing an argument at end of command line";
  return (d->errmsg.Length () == 0);
}

void ArgParse::ErrorExit (Str errorMessage)
{
  ob_banner (stderr);
  fprintf (stderr, "%s\n%s", errorMessage.utf8 (), UsageMessage ().utf8 ());
  Exit (EXIT_FAILURE);
}

int CompareByLength (const ArgParse::apstring &a, const ArgParse::apstring &b)
{
  if (a.Length () < b.Length ())
    return -1;
  else if (a.Length () > b.Length ())
    return 1;
  else
    return a.Compare (b);
}

ArgParse::apstring ArgParse::UsageMessage () const
{
  apstring ret;
  int indent = 25;

  for (UsageInfoList::iterator e = d->usage.begin (); e != d->usage.end (); e++)
    {
      if (e->first >= 0)
        {
          // Add the usage text verbatim
          indent = e->first;
          ret += e->second;
          ret += "\n";
        }
      else
        {
          // Add usage info for the option named in e->second.
          OptionHandlerMap::iterator foundoption = d->options.find (e->second);
          if (foundoption == d->options.end ())
            {
              // If this happens, something is wrong.
              continue;
            }
          OptionHandler *oh = foundoption->second;
          // First step is to make a list of all of its names
          // (all the aliases, plus the real name)
          apstringvec names = oh->aliases;
          names.Append (e->second);
          // Now sort the list by length, so the shortest is first
          names.Quicksort (CompareByLength);
          bool first = true;
          apstring line = "  ";
          for (ObCrawl<apstring> cr = names.Crawl (); !cr.isempty ();)
            {
              apstring cur = cr.popfore ();
              if (first)
                {
                  if (d->allowOneCharOptionsToBeCombined && cur.Length () != 1)
                    line += "    ";
                }
              else
                {
                  line += ", ";
                }
              first = false;
              if (d->allowOneCharOptionsToBeCombined && cur.Length () != 1)
                line += "-";
              line += "-";
              line += cur;
            }
          for (apstring::iterator it = oh->usage.begin ();
               it != oh->usage.end (); it++)
            {
              if (*it == '\n')
                {
                  ret += line;
                  ret += "\n";
                  line = "";
                }
              else if (*it == '\a')
                {
                  int spaces = indent - line.Length ();
                  if (spaces < 1)
                    {
                      /* break onto a new line if we're already past
                     * the indent */
                      ret += line;
                      ret += "\n";
                      line = "";
                      spaces = indent;
                    }
                  for (int i = 0; i < spaces; i++)
                    line += " ";
                }
              else
                {
                  line += *it;
                }
            }
          ret += line;
          ret += "\n";
        }
    }
  return ret;
}

/*-********************************************************************-*/

static ArgParse::apstring parseInt (ArgParse::apstring arg,
                                    ArgParse::apint *value)
{
  char *endptr;
  ArgParse::apint newvalue =
    (ArgParse::apint) strtoll (arg.utf8 (), &endptr, 0);
  if (*endptr != '\0' || arg.Length () == 0)
    return ArgParse::apstring ("\"") + arg + "\" is not a valid integer";
  if (value)
    *value = newvalue;
  return "";
}

static ArgParse::apstring parseFloat (ArgParse::apstring arg,
                                      ArgParse::apfloat *value)
{
  char *endptr;
  ArgParse::apfloat newvalue =
    (ArgParse::apfloat) strtod (arg.utf8 (), &endptr);
  if (*endptr != '\0' || arg.Length () == 0)
    return (ArgParse::apstring ("\"") + arg
            + "\" is not a valid floating-point number");
  if (value)
    *value = newvalue;
  return "";
}

/*-********************************************************************-*/

OptionHandler::OptionHandler (ArgParse::apstring usage_in, int separator_in)
    : usage (usage_in), separator (separator_in), aliases (2.0)
{
}

OptionHandler::~OptionHandler () {}

bool OptionHandler::takesarg () const
{
  return true;
}

bool OptionHandler::allownegation () const
{
  return false;
}

void OptionHandler::reset () {}

ArgParse::apstring OptionHandler::handleargsplit (ArgParse::apstring arg,
                                                  ArgParse::apstring prefix)
{
  if (separator < 0)
    {
      // For SEP_NONE or SEP_ARGV, we have no extra work to do here.
      // (Except to add the prefix)
      ArgParse::apstring ret = handlearg (arg);
      if (ret.Length () == 0)
        return "";
      return prefix + ": " + ret;
    }

  ObTrove<Str> tr = arg.Split (Str (UChar32 (separator)));
  ObCrawl<Str> kr = tr.Crawl ();

  while (!kr.isempty ())
    {
      ArgParse::apstring ret = handlearg (kr.popfore ());
      if (ret.Length () != 0)
        return prefix + ": " + ret;
    }

  return "";
}

// Okay, this is the point in the file where it starts getting
// pretty repetitive.  I'm sure there's a better way I could've
// done this...

/*-********************************************************************-*/

class FlagHandler : public OptionHandler
{
 public:
  bool allow_negation;
  bool already_used;
  ArgParse::apflag *value;

  FlagHandler (ArgParse::apstring usage_in, ArgParse::apflag *value_in,
               bool allow_negation_in);
  bool takesarg () const override;
  bool allownegation () const override;
  void reset () override;
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

FlagHandler::FlagHandler (ArgParse::apstring usage_in,
                          ArgParse::apflag *value_in, bool allow_negation_in)
    : OptionHandler (usage_in),
      allow_negation (allow_negation_in),
      already_used (false),
      value (value_in)
{
}

bool FlagHandler::takesarg () const
{
  return false;
}

bool FlagHandler::allownegation () const
{
  return allow_negation;
}

void FlagHandler::reset ()
{
  already_used = false;
}

ArgParse::apstring FlagHandler::handlearg (ArgParse::apstring arg)
{
  ArgParse::apflag newvalue = (arg.Length () == 0);
#if 0
  if (already_used)
    {
      // If they specified the same flag before, let them
      // get away with it.  Only complain if one is negated
      // and one is not.
      if (newvalue != *value)
        return "negated flag used with non-negated flag";
    }
  else
#endif
  {
    if (value)
      *value = newvalue;
    already_used = true;
  }
  return "";
}

void ArgParse::ArgFlag (apstring name, apstring usage, apflag *value,
                        bool allow_negation)
{
  d->addOption (name, new FlagHandler (usage, value, allow_negation));
}

/*-********************************************************************-*/

using boost::logic::tribool;

class Flag3Handler : public OptionHandler
{
 public:
  tribool *value;

  Flag3Handler (ArgParse::apstring usage_in, tribool *value_in);
  bool takesarg () const override;
  bool allownegation () const override;
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

Flag3Handler::Flag3Handler (ArgParse::apstring usage_in, tribool *value_in)
    : OptionHandler (usage_in), value (value_in)
{
}

bool Flag3Handler::takesarg () const
{
  return false;
}

bool Flag3Handler::allownegation () const
{
  return true;
}

ArgParse::apstring Flag3Handler::handlearg (ArgParse::apstring arg)
{
  ArgParse::apflag newvalue = (arg.Length () == 0);
  if (value)
    *value = newvalue;
  return "";
}

void ArgParse::ArgFlag (apstring name, apstring usage, tribool *value)
{
  d->addOption (name, new Flag3Handler (usage, value));
}

/*-********************************************************************-*/

class IntHandler : public OptionHandler
{
 public:
  bool already_used;
  ArgParse::apint *value;

  IntHandler (ArgParse::apstring usage_in, ArgParse::apint *value_in);
  void reset () override;
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

IntHandler::IntHandler (ArgParse::apstring usage_in, ArgParse::apint *value_in)
    : OptionHandler (usage_in), already_used (false), value (value_in)
{
}

void IntHandler::reset ()
{
  already_used = false;
}

ArgParse::apstring IntHandler::handlearg (ArgParse::apstring arg)
{
#if 0
  if (already_used)
    { return "option specified more than once";
    }
#endif
  already_used = true;
  return parseInt (arg, value);
}

void ArgParse::ArgInt (apstring name, apstring usage, apint *value,
                       bool append_default)
{
  if (append_default && value)
    usage.Append (format_default (*value));
  d->addOption (name, new IntHandler (usage, value));
}

/*-********************************************************************-*/

class FloatHandler : public OptionHandler
{
 public:
  bool already_used;
  ArgParse::apfloat *value;

  FloatHandler (ArgParse::apstring usage_in, ArgParse::apfloat *value_in);
  void reset () override;
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

FloatHandler::FloatHandler (ArgParse::apstring usage_in,
                            ArgParse::apfloat *value_in)
    : OptionHandler (usage_in), already_used (false), value (value_in)
{
}

void FloatHandler::reset ()
{
  already_used = false;
}

ArgParse::apstring FloatHandler::handlearg (ArgParse::apstring arg)
{
#if 0
  if (already_used)
    { return "option specified more than once";
    }
#endif
  already_used = true;
  return parseFloat (arg, value);
}

void ArgParse::ArgFloat (apstring name, apstring usage, apfloat *value,
                         bool append_default)
{
  if (append_default && value)
    usage.Append (format_default (*value));
  d->addOption (name, new FloatHandler (usage, value));
}

/*-********************************************************************-*/

class StringHandler : public OptionHandler
{
 public:
  bool already_used;
  ArgParse::apstring *value;

  StringHandler (ArgParse::apstring usage_in, ArgParse::apstring *value_in);
  void reset () override;
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

StringHandler::StringHandler (ArgParse::apstring usage_in,
                              ArgParse::apstring *value_in)
    : OptionHandler (usage_in), already_used (false), value (value_in)
{
}

void StringHandler::reset ()
{
  already_used = false;
}

ArgParse::apstring StringHandler::handlearg (ArgParse::apstring arg)
{
#if 0
  if (already_used)
    { return "option specified more than once";
    }
#endif
  already_used = true;
  if (value)
    *value = arg;
  return "";
}

void ArgParse::ArgString (apstring name, apstring usage, apstring *value,
                          bool append_default)
{
  if (append_default && value)
    {
      usage.Append ("(default ");
      // not sure why, but this was giving me a leak in valgrind:
      //   usage.Append (*value);
      // so instead force the strings to be unrelated by going through utf-8:
      usage.Append (value->utf8 ());
      usage.Append (UChar32 (')'));
    }
  d->addOption (name, new StringHandler (usage, value));
}

/*-********************************************************************-*/

class IntsHandler : public OptionHandler
{
 public:
  ArgParse::apintvec *value;

  IntsHandler (ArgParse::apstring usage_in, ArgParse::apintvec *value_in,
               int separator_in);
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

IntsHandler::IntsHandler (ArgParse::apstring usage_in,
                          ArgParse::apintvec *value_in, int separator_in)
    : OptionHandler (usage_in, separator_in), value (value_in)
{
}

ArgParse::apstring IntsHandler::handlearg (ArgParse::apstring arg)
{
  ArgParse::apint newvalue;
  ArgParse::apstring ret = parseInt (arg, &newvalue);
  if (ret.Length () == 0 && value)
    value->Append (newvalue);
  return ret;
}

void ArgParse::ArgInts (apstring name, apstring usage, apintvec *values,
                        int separator)
{
  d->addOption (name, new IntsHandler (usage, values, separator));
}

/*-********************************************************************-*/

class FloatsHandler : public OptionHandler
{
 public:
  ArgParse::apfloatvec *value;

  FloatsHandler (ArgParse::apstring usage_in, ArgParse::apfloatvec *value_in,
                 int separator_in);
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

FloatsHandler::FloatsHandler (ArgParse::apstring usage_in,
                              ArgParse::apfloatvec *value_in, int separator_in)
    : OptionHandler (usage_in, separator_in), value (value_in)
{
}

ArgParse::apstring FloatsHandler::handlearg (ArgParse::apstring arg)
{
  ArgParse::apfloat newvalue;
  ArgParse::apstring ret = parseFloat (arg, &newvalue);
  if (ret.Length () == 0 && value)
    value->Append (newvalue);
  return ret;
}

void ArgParse::ArgFloats (apstring name, apstring usage, apfloatvec *values,
                          int separator)
{
  d->addOption (name, new FloatsHandler (usage, values, separator));
}

/*-********************************************************************-*/

class StringsHandler : public OptionHandler
{
 public:
  ArgParse::apstringvec *value;

  StringsHandler (ArgParse::apstring usage_in, ArgParse::apstringvec *value_in,
                  int separator_in);
  ArgParse::apstring handlearg (ArgParse::apstring arg) override;
};

StringsHandler::StringsHandler (ArgParse::apstring usage_in,
                                ArgParse::apstringvec *value_in,
                                int separator_in)
    : OptionHandler (usage_in, separator_in), value (value_in)
{
}

ArgParse::apstring StringsHandler::handlearg (ArgParse::apstring arg)
{
  if (value)
    value->Append (arg);
  return "";
}

void ArgParse::ArgStrings (apstring name, apstring usage, apstringvec *values,
                           int separator)
{
  d->addOption (name, new StringsHandler (usage, values, separator));
}

/*-********************************************************************-*/

/// Make sure we pull in the string conversion for libLoam/c++ retorts
/// (This is only necessary for static libraries.)

static int dummy_auXdMv6e OB_UNUSED = ob_private_hack_pull_in_loamxx_retorts++;
