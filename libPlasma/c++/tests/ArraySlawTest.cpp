
/* (c)  oblong industries */

#include "libLoam/c++/LoamStreams.h"
#include "libPlasma/c++/PlasmaStreams.h"
#include "libPlasma/c++/SlawIterator.h"
#include <libPlasma/c++/Slaw.h>

#include <gtest/gtest.h>

using namespace oblong::plasma;

namespace {

#define DEFINE_ARRAY_TEST(T)                                                   \
  TEST (ArraySlawTest, T##array)                                               \
  {                                                                            \
    const T values[][5] = {                                                    \
      {static_cast<T> ((std::numeric_limits<T>::min) ()),                      \
       static_cast<T> ((std::numeric_limits<T>::min) () + 22),                 \
       static_cast<T> ((std::numeric_limits<T>::max) ()),                      \
       static_cast<T> ((std::numeric_limits<T>::max) () - 42), T (0)},         \
      {static_cast<T> ((std::numeric_limits<T>::min) () + 18),                 \
       static_cast<T> ((std::numeric_limits<T>::min) ()),                      \
       static_cast<T> ((std::numeric_limits<T>::max) () - 2), T (1), T (0)}};  \
                                                                               \
    const size_t N (2);                                                        \
    const size_t M (5);                                                        \
                                                                               \
    for (size_t i = 0; i < N; ++i)                                             \
      {                                                                        \
        Slaw s (values[i], M);                                                 \
        EXPECT_TRUE (s.IsArray ());                                            \
        EXPECT_FALSE (s.IsAtomic ());                                          \
        EXPECT_FALSE (s.IsComposite ());                                       \
        EXPECT_TRUE (s.CanEmit<const T *> ());                                 \
        const T *v = s.Emit<const T *> ();                                     \
        for (size_t j = 0; j < M; ++j)                                         \
          EXPECT_EQ (values[i][j], v[j]);                                      \
                                                                               \
        typedef std::vector<T> TVect;                                          \
        TVect vals;                                                            \
        EXPECT_TRUE (s.Into (vals));                                           \
        EXPECT_EQ (M, vals.size ());                                           \
        for (size_t j = 0; j < M; ++j)                                         \
          EXPECT_EQ (values[i][j], vals[j]);                                   \
                                                                               \
        std::vector<T> vals2 (values[i], values[i] + M);                       \
        EXPECT_EQ (s, Slaw (vals2));                                           \
                                                                               \
        Slaw t (s);                                                            \
        Slaw u (t);                                                            \
        EXPECT_TRUE (t.Emit<TVect> () == u.Emit<TVect> ());                    \
        s = u;                                                                 \
        t = t = u;                                                             \
        EXPECT_TRUE (s.Emit<TVect> () == u.Emit<TVect> ());                    \
        EXPECT_TRUE (t.Emit<TVect> () == u.Emit<TVect> ());                    \
      }                                                                        \
                                                                               \
    for (size_t i = 0; i < N; ++i)                                             \
      {                                                                        \
        Slaw s (values[i], M);                                                 \
        EXPECT_TRUE (s.IsArray ());                                            \
        EXPECT_EQ (M, size_t (s.Count ()));                                    \
        for (size_t j = 0; j < M; ++j)                                         \
          EXPECT_EQ (Slaw (values[i][j]), s[j]);                               \
      }                                                                        \
  }                                                                            \
                                                                               \
  TEST (TroveSlawTest, T##array)                                               \
  {                                                                            \
    ObTrove<T> t;                                                              \
    t.Append (107);                                                            \
    t.Append (111);                                                            \
    t.Append (112);                                                            \
    Slaw listy (t);                                                            \
    if (Str (#T).Match ("float"))                                              \
      EXPECT_STREQ ("[107.000000, 111.000000, 112.000000]",                    \
                    listy.ToPrintableString ());                               \
    else                                                                       \
      EXPECT_STREQ ("[107, 111, 112]", listy.ToPrintableString ());            \
  }

DEFINE_ARRAY_TEST (int8);
DEFINE_ARRAY_TEST (int16);
DEFINE_ARRAY_TEST (int32);
DEFINE_ARRAY_TEST (int64);
DEFINE_ARRAY_TEST (unt8);
DEFINE_ARRAY_TEST (unt16);
DEFINE_ARRAY_TEST (unt32);
DEFINE_ARRAY_TEST (unt64);
DEFINE_ARRAY_TEST (float32);
DEFINE_ARRAY_TEST (float64);

}  // namespace
