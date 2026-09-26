
/* (c)  oblong industries */

#include "PoolServer.h"

#include <libPlasma/c++/Slaw.h>
#include <libPlasma/c/pool.h>

#include <string.h>

#include <iostream>

namespace oblong {
namespace plasma {

namespace {

const char *const DEF_HOST = "localhost";
const unt16 DEF_PORT (-1);

const char *non_null (const char *s)
{
  return (!s || !strlen (s)) ? "" : s;
}
}

PoolServer::PoolServer ()
    : host_ (DEF_HOST),
      port_ (DEF_PORT),
      name_ (""),
      type_ (""),
      address_ (MakePoolUri (""))
{
}

PoolServer::PoolServer (const char *h, unt16 p, const char *n, const char *t)
    : host_ (!h || !strlen (h) ? DEF_HOST : h),
      port_ (p),
      name_ (non_null (n)),
      type_ (non_null (t)),
      address_ (MakePoolUri (""))
{
}

bool PoolServer::operator== (const PoolServer &other) const
{
  if (this == &other)
    return true;
  return other.port_ == port_ && other.type_ == type_ && other.host_ == host_
         && other.name_ == name_;
}

bool PoolServer::operator!= (const PoolServer &other) const
{
  return !(other == *this);
}

const Str &PoolServer::Protocol () const
{
  static const Str tcp ("tcp");
  return tcp;
}

const Str &PoolServer::Host () const
{
  return host_;
}

unt16 PoolServer::Port () const
{
  return port_;
}

const Str &PoolServer::Name () const
{
  return name_;
}

const Str &PoolServer::Type () const
{
  return type_;
}

const Str &PoolServer::Address () const
{
  return address_;
}

Str PoolServer::MakePoolUri (const Str &pool_name) const
{
  //  An IPv6 address goes in brackets, so that its colons can't be
  //  mistaken for the one before the port.
  const Str &h = Host ();
  const bool v6 = h.Contains (":") && h.Index ("[") != 0;
  return Str ().Sprintf ("%s://%s%s%s:%d/%s", Protocol ().utf8 (),
                         v6 ? "[" : "", h.utf8 (), v6 ? "]" : "", Port (),
                         pool_name.utf8 ());
}

// ObUniqueTrove<Str> PoolServer::Pools () const
// { slaw s = NULL;
//   pool_list_remote (Address (), &s);
//   Slaw names (s);
//   ObUniqueTrove<Str> result;
//   for (unt64 i = 0, c = names . Count () ; i < c ; ++i)
//     { Str n;
//       if (names . Nth (i) . Into (n)) result . Append (n);
//     }
//   return result;
// }

::std::ostream &operator<< (::std::ostream &os, const PoolServer &s)
{
  return os << "<" << s.Address () << " (" << s.Name () << "/" << s.Type ()
            << ")>";
}
}
}  // namespace oblong::plasma
