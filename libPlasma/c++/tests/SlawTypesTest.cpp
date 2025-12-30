
/* (c)  oblong industries */

#include "SlawTypesTest.h"

#include <libPlasma/c++/slaw-traits.h>
#include <libLoam/c++/Str.h>
#include <limits>

namespace {

template <typename T, size_t N>
void wrapped_slaw_test_v (SlawTypesTest *t, T (&values)[N])
{
  const CallbacksImpl<T, N> c (values);
  t->wrapped_slaw_test (c);
}

template <typename V, typename T, size_t N = 4>
struct vset
{
  static void x (V &v, T &t) { v.x = t; }
  static void y (V &v, T &t) { v.y = t; }
  static void z (V &v, T &t) { v.z = t; }
  static void w (V &v, T &t) { v.w = t; }
};

template <typename V, typename T>
struct vset<V, T, 2>
{
  static void x (V &v, T &t) { v.x = t; }
  static void y (V &v, T &t) { v.y = t; }
  static void z (V &, T &) {}
  static void w (V &, T &) {}
};

template <typename V, typename T>
struct vset<V, T, 3>
{
  static void x (V &v, T &t) { v.x = t; }
  static void y (V &v, T &t) { v.y = t; }
  static void z (V &v, T &t) { v.z = t; }
  static void w (V &, T &) {}
};

template <typename V, typename T, size_t N>
void typed_vector_slaw_test (SlawTypesTest *t)
{
  T components[8] = {static_cast<T> ((std::numeric_limits<T>::min) ()),
                     static_cast<T> ((std::numeric_limits<T>::min) () + 22),
                     static_cast<T> ((std::numeric_limits<T>::max) ()),
                     static_cast<T> ((std::numeric_limits<T>::max) () - 42),
                     T (0),
                     static_cast<T> ((std::numeric_limits<T>::min) () + 1),
                     static_cast<T> ((std::numeric_limits<T>::max) () - 11),
                     T (1969)};

  V v[2] = {};
  vset<V, T, N>::x (v[0], components[0]);
  vset<V, T, N>::y (v[0], components[1]);
  vset<V, T, N>::z (v[0], components[2]);
  vset<V, T, N>::w (v[0], components[3]);
  vset<V, T, N>::x (v[1], components[4]);
  vset<V, T, N>::y (v[1], components[5]);
  vset<V, T, N>::z (v[1], components[6]);
  vset<V, T, N>::w (v[1], components[7]);

  wrapped_slaw_test_v (t, v);
}

template <typename T>
void atomic_numeric_slaw_test (SlawTypesTest *t)
{
  T values[] = {static_cast<T> ((std::numeric_limits<T>::min) ()),
                static_cast<T> ((std::numeric_limits<T>::min) () + 22),
                static_cast<T> ((std::numeric_limits<T>::max) ()),
                static_cast<T> ((std::numeric_limits<T>::max) () - 42), T (0)};
  wrapped_slaw_test_v (t, values);
}

template <typename T>
void numeric_wrapped_slaw_test (SlawTypesTest *t)
{
  atomic_numeric_slaw_test<T> (t);
}

}  // namespace


void SlawTypesTest::wrapped_slaw_test (const Callbacks &)
{
  EXPECT_FALSE ("wrapped_slaw_test not implemented");
}

void SlawTypesTest::TestStrings ()
{
  const char *chr[] = {"v1", "v2", "v3", ""};
  wrapped_slaw_test_v (this, chr);

  oblong::loam::Str strs[] = {"str0", "str1", "str2", "str3", ""};
  wrapped_slaw_test_v (this, strs);
}

void SlawTypesTest::TestBoolean ()
{
  bool values[] = {true, false};
  wrapped_slaw_test_v (this, values);
}

void SlawTypesTest::TestNumeric ()
{
  numeric_wrapped_slaw_test<int8> (this);
  numeric_wrapped_slaw_test<int16> (this);
  numeric_wrapped_slaw_test<int32> (this);
  numeric_wrapped_slaw_test<int64> (this);
  numeric_wrapped_slaw_test<unt8> (this);
  numeric_wrapped_slaw_test<unt16> (this);
  numeric_wrapped_slaw_test<unt32> (this);
  numeric_wrapped_slaw_test<unt64> (this);
  numeric_wrapped_slaw_test<float32> (this);
  numeric_wrapped_slaw_test<float64> (this);

  atomic_numeric_slaw_test<unsigned long> (this);
}

void SlawTypesTest::TestVectorClasses ()
{
  typed_vector_slaw_test<v2int8, int8, 2> (this);
  typed_vector_slaw_test<v2int16, int16, 2> (this);
  typed_vector_slaw_test<v2int32, int32, 2> (this);
  typed_vector_slaw_test<v2int64, int64, 2> (this);
  typed_vector_slaw_test<v2unt8, unt8, 2> (this);
  typed_vector_slaw_test<v2unt16, unt16, 2> (this);
  typed_vector_slaw_test<v2unt32, unt32, 2> (this);
  typed_vector_slaw_test<v2unt64, unt64, 2> (this);
  typed_vector_slaw_test<v2float32, float32, 2> (this);
  typed_vector_slaw_test<v2float64, float64, 2> (this);

  typed_vector_slaw_test<v3int8, int8, 3> (this);
  typed_vector_slaw_test<v3int16, int16, 3> (this);
  typed_vector_slaw_test<v3int32, int32, 3> (this);
  typed_vector_slaw_test<v3int64, int64, 3> (this);
  typed_vector_slaw_test<v3unt8, unt8, 3> (this);
  typed_vector_slaw_test<v3unt16, unt16, 3> (this);
  typed_vector_slaw_test<v3unt32, unt32, 3> (this);
  typed_vector_slaw_test<v3unt64, unt64, 3> (this);
  typed_vector_slaw_test<v3float32, float32, 3> (this);
  typed_vector_slaw_test<v3float64, float64, 3> (this);

  typed_vector_slaw_test<v4int8, int8, 4> (this);
  typed_vector_slaw_test<v4int16, int16, 4> (this);
  typed_vector_slaw_test<v4int32, int32, 4> (this);
  typed_vector_slaw_test<v4int64, int64, 4> (this);
  typed_vector_slaw_test<v4unt8, unt8, 4> (this);
  typed_vector_slaw_test<v4unt16, unt16, 4> (this);
  typed_vector_slaw_test<v4unt32, unt32, 4> (this);
  typed_vector_slaw_test<v4unt64, unt64, 4> (this);
  typed_vector_slaw_test<v4float32, float32, 4> (this);
  typed_vector_slaw_test<v4float64, float64, 4> (this);
}

void SlawTypesTest::TestVect ()
{
  oblong::loam::Vect vs[] = {
    oblong::loam::Vect (0, 1, 2),
    oblong::loam::Vect (-3.14159265358979, 2.718281828459045, 9902345.009123),
    oblong::loam::Vect ()};
  wrapped_slaw_test_v (this, vs);
}

void SlawTypesTest::TestVect4 ()
{
  oblong::loam::Vect4 v4s[] = {oblong::loam::Vect4 (0, 1, 2, 0),
                               oblong::loam::Vect4 (-3.141592653589793,
                                                    2.718281828459045,
                                                    9902345.009123, 42.1),
                               oblong::loam::Vect4 ()};
  wrapped_slaw_test_v (this, v4s);
}

void SlawTypesTest::TestQuat ()
{
  oblong::loam::Quat qs[] = {oblong::loam::Quat (0, 1, 2, 3),
                             oblong::loam::Quat (-3.141592653589793,
                                                 2.718281828459045,
                                                 9902345.009123, 42.1),
                             oblong::loam::Quat ()};
  wrapped_slaw_test_v (this, qs);
}
