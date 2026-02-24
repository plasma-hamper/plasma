
/* (c)  oblong industries */

/* This is the place for tests of Matrix44, ObColor, ObPseudopod,
 * Quat, Vect, Vect4, ObResolvedPath, and ObMap.
 *
 * It started out very minimal (hence the name) in the spirit of
 * LameGeomSlabTest, just calling methods but not necessarily
 * checking the results.  But since then it has evolved, and now
 * some of the tests are actually meaningful.  We could still use
 * more tests or improvement of the existing tests, though.
 *
 * We're testing a bunch of different classes in one file just out of
 * laziness, to avoid having to add a bunch of new test programs, since
 * each new test program has to be added to Makefile.am and .gitignore
 * (for UNIX) and to libLoamWin32C++.sln and make_check_win32.sh,
 * not to mention creating a new .vcxproj file (for Windows).
 *
 * Ben was especially a proponent of the "have fewer programs"
 * philosophy, although he's not here anymore.
 *
 * At some point in the future, we may want to split up this
 * file a bit, perhaps put all the math stuff (Matrix44, Quat,
 * Vect, Vect4) in one file, and the other stuff in another file.
 */

#include <gtest/gtest.h>
#include "libLoam/c/ob-rand.h"
#include "libLoam/c/ob-string.h"
#include "libLoam/c++/LoamStreams.h"
#include "libLoam/c++/Matrix44.h"
#include "libLoam/c++/ObColor.h"
#include "libLoam/c++/ObMap.h"
#include "libLoam/c++/ObResolvedPath.h"
#include "libLoam/c++/ObPseudopod.h"
#include "libLoam/c++/Quat.h"
#include "libLoam/c++/Vect.h"
#include "libLoam/c++/Vect4.h"
#include <sstream>

using namespace oblong::loam;

const Vect ZERO (0, 0, 0);
const Vect WHY (0, 1, 0);
const Vect XZ (1, 0, 1);
const Vect4 FOO (0, 1, 2, 3);


TEST (Vect, ConstructVect)
{
  Vect *vee = new Vect (1.0, 2.0, 3.0);
  EXPECT_EQ (1.0, vee->X ());
  EXPECT_EQ (2.0, vee->Y ());
  EXPECT_EQ (3.0, vee->Z ());

  // this next used to compile before the constructor-from-Vect-pointer
  // was marked 'explicit'; now no more.
  //  Vect frumpy = Vect (10.0, 10.0, 10.0) + vee;
  Vect frumpy = Vect (10.0, 10.0, 10.0) + Vect (vee);
  EXPECT_EQ (11.0, frumpy.X ());
  EXPECT_EQ (12.0, frumpy.Y ());
  EXPECT_EQ (13.0, frumpy.Z ());

  float64 fluh[3] = {4.0, 5.0, 6.0};
  // ditto below (or: uncomment if you feel that the proof is in the
  // pudding of noncompilation)...
  //  Vect frummer = Vect (10.0, 10.0, 10.0) + fluh;
  Vect frummer = Vect (10.0, 10.0, 10.0) + Vect (fluh);
  EXPECT_EQ (14.0, frummer.X ());
  EXPECT_EQ (15.0, frummer.Y ());
  EXPECT_EQ (16.0, frummer.Z ());

  delete vee;
}



class Matrix44Test : public ::testing::Test
{
 public:
  Matrix44 z;
  Matrix44 id;

  Matrix44Test ()
      : z (ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 (),
           ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 (),
           ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 (),
           ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 (),
           ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 (),
           ob_rand_float64 ())
  {
  }
  void SetUp () override { z.LoadZero (); }
};

TEST_F (Matrix44Test, ZeroEqZero)
{
  EXPECT_TRUE (Matrix44::zeroMat == z);
}

TEST_F (Matrix44Test, IdEqId)
{
  EXPECT_TRUE (Matrix44::idMat == id);
}

TEST_F (Matrix44Test, ZeroNeId)
{
  EXPECT_TRUE (Matrix44::zeroMat != id);
}

TEST_F (Matrix44Test, IdNeZero)
{
  EXPECT_TRUE (Matrix44::idMat != z);
}

TEST_F (Matrix44Test, LoadIdent)
{
  z.LoadIdent ();
  EXPECT_TRUE (z == id);
}

TEST_F (Matrix44Test, LoadRotationNoNorm)
{
  Matrix44 foo (id.LoadRotationNoNorm (WHY, 1.5));
  EXPECT_TRUE (id == foo);
}

TEST_F (Matrix44Test, LoadRotationAboutNoNorm1)
{
  id.LoadRotationAboutNoNorm (WHY, 1.5, XZ);
  z.LoadRotationNoNorm (WHY, 1.5);
  EXPECT_TRUE (z != id);
}

TEST_F (Matrix44Test, LoadRotationAboutNoNorm2)
{
  id.LoadRotationAboutNoNorm (WHY, 1.5, ZERO);
  z.LoadRotationNoNorm (WHY, 1.5);
  EXPECT_TRUE (z == id);
}

TEST_F (Matrix44Test, LoadTranslation)
{
  id.LoadTranslation (WHY);
  z.LoadTranslation (0, 1, 0);
  EXPECT_TRUE (z == id);
}

TEST_F (Matrix44Test, LoadScale)
{
  id.LoadScale (9.23, 9.23, 9.23);
  z.LoadScale (9.23);
  EXPECT_TRUE (z == id);
}

TEST_F (Matrix44Test, LoadShear)
{
  z.LoadShear (ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 (),
               ob_rand_float64 (), ob_rand_float64 (), ob_rand_float64 ());
  id.CopyFrom (z);
  EXPECT_TRUE (z == id);
}

TEST_F (Matrix44Test, Mult1)
{
  EXPECT_TRUE (Matrix44::zeroMat == id.Mult (z));
}

TEST_F (Matrix44Test, BitwiseAnd)
{
  id = z;
  EXPECT_TRUE (Matrix44::zeroMat == id);
}

TEST_F (Matrix44Test, Mult2)
{
  EXPECT_TRUE (Matrix44::zeroMat == id * z);
}


TEST_F (Matrix44Test, Mult3)
{
  id *= z;
  EXPECT_TRUE (Matrix44::zeroMat == id);
}

TEST_F (Matrix44Test, Transpose)
{
  z.Load (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3);
  id.Load (0, 4, 8, 6, 1, 5, 9, 5, 2, 6, 8, 4, 3, 7, 7, 3);
  EXPECT_TRUE (z == id.Transpose ());
  EXPECT_TRUE (z != id);
}

TEST_F (Matrix44Test, TransposeInPlace)
{
  z.Load (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3);
  id.Load (0, 4, 8, 6, 1, 5, 9, 5, 2, 6, 8, 4, 3, 7, 7, 3);
  EXPECT_TRUE (z == id.TransposeInPlace ());
  EXPECT_TRUE (z == id);
}

TEST_F (Matrix44Test, Invert)
{
  const float64 angle = 1.23456;
  z.LoadRotation (WHY, angle);

  Matrix44 s;
  s.LoadScale (0.5, 2.0, 5.0);

  Matrix44 t;
  t.LoadTranslation (0.1, 2.0, 10.0);

  z = z * s * t;


  const Matrix44 inv = z.Invert ();
  const Matrix44 mult = z * inv;
  const Matrix44 id_mine;
  const float64 eps = 1e-06;

  // equality test does not really work due to numeric issues
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      {
        EXPECT_TRUE (fabs (id_mine.m[i][j] - mult.m[i][j]) < eps);
      }
}

TEST_F (Matrix44Test, InvertInPlace)
{
  const float64 angle = 1.23456;
  z.LoadRotation (WHY, angle);

  Matrix44 s;
  s.LoadScale (0.5, 2.0, 5.0);

  Matrix44 t;
  t.LoadTranslation (0.1, 2.0, 10.0);

  z = z * s * t;


  Matrix44 inv = z;
  inv.InvertInPlace ();
  const Matrix44 mult = z * inv;
  const Matrix44 id_mine;
  const float64 eps = 1e-06;

  // equality test does not really work due to numeric issues
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      {
        EXPECT_TRUE (fabs (id_mine.m[i][j] - mult.m[i][j]) < eps);
      }
}

TEST_F (Matrix44Test, SingularInvert)
{
  Matrix44 m (1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 0.0,
              0.0, 1.0, 0.0);

  bool is_singular = false;
  OB_UNUSED Matrix44 nope (m.Invert (&is_singular));

  EXPECT_TRUE (is_singular);
}

TEST_F (Matrix44Test, SingularInvertInPlace)
{
  Matrix44 m1 (1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 0.0,
               0.0, 1.0, 0.0);
  const Matrix44 m2 (m1);

  bool is_singular = false;
  m1.InvertInPlace (&is_singular);

  EXPECT_TRUE (is_singular);
  EXPECT_TRUE (m1 == m2);
}

TEST_F (Matrix44Test, TransformVect)
{
  Vect v1 = id.TransformVect (WHY);
  Vect v2 = WHY;
  id.TransformVectInPlace (v2);
  EXPECT_TRUE (v1 == v2);
}

TEST_F (Matrix44Test, TransformVectList)
{
  Vect v1 = id.TransformVect (WHY);
  Vect *v2 = new Vect[1];
  v2[0] = WHY;
  Vect *v3 = id.TransformVectList (v2, 1);
  EXPECT_TRUE (v1 == v3[0]);
  delete[] v2;
  delete[] v3;
}

TEST_F (Matrix44Test, TransformVectList2List)
{
  Vect v1 = id.TransformVect (WHY);
  Vect *v2 = new Vect[1];
  v2[0] = WHY;
  Vect *v3 = new Vect[1];
  Vect *v4 = id.TransformVectList2List (v2, v3, 1);
  EXPECT_TRUE (v3 == v4);
  EXPECT_TRUE (v1 == v3[0]);
  delete[] v2;
  delete[] v3;
}

TEST_F (Matrix44Test, TransformVectListInPlace)
{
  Vect v1 = id.TransformVect (WHY);
  Vect *v2 = new Vect[1];
  v2[0] = WHY;
  Vect *v3 = id.TransformVectListInPlace (v2, 1);
  EXPECT_TRUE (v2 == v3);
  EXPECT_TRUE (v1 == v3[0]);
  delete[] v2;
}

TEST_F (Matrix44Test, TransformVect4)
{
  Vect4 v1 = id.TransformVect4 (FOO);
  Vect4 v2 = FOO;
  id.TransformVect4InPlace (v2);
  EXPECT_TRUE (v1 == v2);
}

TEST_F (Matrix44Test, TransformVect4List)
{
  Vect4 v1 = id.TransformVect4 (FOO);
  Vect4 *v2 = new Vect4[1];
  v2[0] = FOO;
  Vect4 *v3 = id.TransformVect4List (v2, 1);
  EXPECT_TRUE (v1 == v3[0]);
  delete[] v2;
  delete[] v3;
}

TEST_F (Matrix44Test, TransformVect4List2List)
{
  Vect4 v1 = id.TransformVect4 (FOO);
  Vect4 *v2 = new Vect4[1];
  v2[0] = FOO;
  Vect4 *v3 = new Vect4[1];
  Vect4 *v4 = id.TransformVect4List2List (v2, v3, 1);
  EXPECT_TRUE (v3 == v4);
  EXPECT_TRUE (v1 == v3[0]);
  delete[] v2;
  delete[] v3;
}

TEST_F (Matrix44Test, TransformVect4ListInPlace)
{
  Vect4 v1 = id.TransformVect4 (FOO);
  Vect4 *v2 = new Vect4[1];
  v2[0] = FOO;
  Vect4 *v3 = id.TransformVect4ListInPlace (v2, 1);
  EXPECT_TRUE (v2 == v3);
  EXPECT_TRUE (v1 == v3[0]);
  delete[] v2;
}

TEST (ObColorTest, SetHSB)
{
  ObColor c;
  const float64 hue = (60.0 / 360.0);
  c.SetHSB (hue, 1.0, 1.0);  // yellow
  EXPECT_DOUBLE_EQ (1.0, c.R ());
  EXPECT_DOUBLE_EQ (1.0, c.G ());
  EXPECT_DOUBLE_EQ (0.0, c.B ());
  EXPECT_DOUBLE_EQ (1.0, c.A ());
}

TEST (ObColorTest, v4float32Conversion)
{
  float64 x64 = ob_rand_float64 ();
  float64 y64 = ob_rand_float64 ();
  float64 z64 = ob_rand_float64 ();
  float64 w64 = ob_rand_float64 ();
  float32 x32 = (float32) x64;
  float32 y32 = (float32) y64;
  float32 z32 = (float32) z64;
  float32 w32 = (float32) w64;
  ObColor color (x64, y64, z64, w64);

  EXPECT_EQ (x32, color.ToV4Float32 ().x);
  EXPECT_EQ (y32, color.ToV4Float32 ().y);
  EXPECT_EQ (z32, color.ToV4Float32 ().z);
  EXPECT_EQ (w32, color.ToV4Float32 ().w);
  EXPECT_EQ (x32, ((v4float32) color).x);
  EXPECT_EQ (y32, ((v4float32) color).y);
  EXPECT_EQ (z32, ((v4float32) color).z);
  EXPECT_EQ (w32, ((v4float32) color).w);
}


TEST (ObPseudopodTest, ConstructDestructStack)
{
  // ObPseudopod doesn't really seem to do anything or have any methods,
  // but it does have a non-inline constructor and destructor, so we
  // can make sure we linked against those okay.
  ObPseudopod pseudo;
}

TEST (ObPseudopodTest, ConstructDestructHeap)
{
  ObPseudopod *pseudo = new ObPseudopod;
  pseudo->Delete ();
}

class QuatTest : public ::testing::Test
{
 public:
  const float64 eps, epslax;
  Quat q, qq;
  Vect v;
  Matrix44 m;

  QuatTest () : eps (1e-15), epslax (1e-7), q (1, 2, 3, 4), qq () {}
  void SetUp () override
  {
    q.NormSelf ();
    qq = Quat (q.A () + eps, q.I () + eps, q.J () + eps, q.K () + eps);
  }
};

TEST_F (QuatTest, QuatRotVect)
{
  v = q.QuatRotVect (XZ);
}

TEST_F (QuatTest, DeriveRotationMatrix)
{
  m = q.DeriveRotationMatrix ();
}

TEST_F (QuatTest, LoadDerivedRotationMatrix)
{
  q.LoadDerivedRotationMatrix (m);
}

TEST_F (QuatTest, QuatElements)
{
  EXPECT_DOUBLE_EQ (1.0, q.Mag ());
  EXPECT_FLOAT_EQ (0.182574186f, q.A ());
  EXPECT_FLOAT_EQ (0.365148372f, q.I ());
  EXPECT_FLOAT_EQ (0.547722558f, q.J ());
  EXPECT_FLOAT_EQ (0.730296743f, q.K ());
}

TEST_F (QuatTest, QuatEquals)
{
  EXPECT_TRUE (q.Equals (q));
  EXPECT_FALSE (q.Equals (-q));
  EXPECT_FALSE (q.Equals (qq));
}

TEST_F (QuatTest, QuatRotEquals)
{
  EXPECT_TRUE (q.RotEquals (-q));
  EXPECT_TRUE (q.RotEquals (q));
}

TEST_F (QuatTest, QuatApproxEquals)
{
  EXPECT_FALSE (q.Equals (qq));
  EXPECT_TRUE (q.ApproxEquals (qq, 2 * eps));
}

TEST_F (QuatTest, QuatRotApproxEquals)
{
  EXPECT_TRUE (q.RotApproxEquals (q, eps));
  EXPECT_TRUE (q.RotApproxEquals (-q, eps));
  EXPECT_TRUE (q.RotApproxEquals (qq, 2 * eps));
  EXPECT_TRUE (q.RotApproxEquals (-qq, 2 * eps));
}

TEST_F (QuatTest, QuatFromRandom180)
{
  Quat rot_180;
  Vect norm, over;

  ob_rand_t *r = ob_rand_allocate_state (31337);  // arbitrary seed : also prime
  EXPECT_TRUE (r != NULL);

  for (int i = 0; i < 10; ++i)
    {
      rot_180.i = ob_rand_state_float64 (-1.0, 1.0, r);
      rot_180.j = ob_rand_state_float64 (-1.0, 1.0, r);
      rot_180.k = ob_rand_state_float64 (-1.0, 1.0, r);
      rot_180.a = 0.0;  // 2.0 * acos (0.0) == +/- 180 degrees

      // rare, but possible, case
      if (rot_180.Mag () <= eps)
        rot_180.i = 1.0;

      rot_180.NormSelf ();
      norm = rot_180.QuatRotVect (Vect (0.0, 0.0, 1.0));
      over = rot_180.QuatRotVect (Vect (1.0, 0.0, 0.0));
      Quat should_be_same = Quat::QRotFromNormOver (norm, over);
      if (!rot_180.RotApproxEquals (should_be_same, eps))
        {
          OB_LOG_ERROR ("(%d) failure in %s approx= %s", i,
                        rot_180.AsStr ().utf8 (),
                        should_be_same.AsStr ().utf8 ());
        }
      EXPECT_TRUE (rot_180.RotApproxEquals (should_be_same, eps));
      // test is a bit *strict* -- but it'll be interesting to see when
      // it fails...
    }
  ob_rand_free_state (r);
}

TEST_F (QuatTest, QuatFromRandomRot)
{
  Quat rot_rand;
  Vect norm, over;

  ob_rand_t *r = ob_rand_allocate_state (31337);  // arbitrary seed : also prime
  EXPECT_TRUE (r != NULL);

  for (int i = 0; i < 100; ++i)
    {
      rot_rand.i = ob_rand_state_float64 (-1.0, 1.0, r);
      rot_rand.j = ob_rand_state_float64 (-1.0, 1.0, r);
      rot_rand.k = ob_rand_state_float64 (-1.0, 1.0, r);
      if (i % 2 == 1)
        rot_rand.a = ob_rand_state_float64 (0.0, 1.0, r);
      else
        rot_rand.a = ob_rand_state_float64 (0.0, 0.05, r);
      // Second branch ensures we test near singularity

      // we do not want this to be negative
      // since each quaternion has two "equivalent" forms

      // rare, but possible, case
      if (rot_rand.Mag () <= eps)
        rot_rand.i = 1.0;

      rot_rand.NormSelf ();
      norm = rot_rand.QuatRotVect (Vect (0.0, 0.0, 1.0));
      over = rot_rand.QuatRotVect (Vect (1.0, 0.0, 0.0));
      Quat should_be_same = Quat::QRotFromNormOver (norm, over);
      if (!rot_rand.RotApproxEquals (should_be_same, eps))
        {
          OB_LOG_ERROR ("(%d) failure in %s approx= %s", i,
                        rot_rand.AsStr ().utf8 (),
                        should_be_same.AsStr ().utf8 ());
          OB_LOG_ERROR (
            "over, up, norm \n[%s,\n %s,\n %s]", over.AsStr ().utf8 (),
            norm.Cross (over).AsStr ().utf8 (), norm.AsStr ().utf8 ());
        }
      EXPECT_TRUE (rot_rand.RotApproxEquals (should_be_same, eps));
      // test is a bit *strict* -- but it'll be interesting to see when
      // it fails...
    }
  ob_rand_free_state (r);
}

TEST_F (QuatTest, QuatFromAxisRot)
{
  Vect norm, over;
  for (int i = 0; i < 24; ++i)
    {
      norm = over = Vect (0, 0, 0);
      switch (i % 3)
        {
          case 0:
            norm.x = 1.0;
            over.y = 1.0;
            break;
          case 1:
            norm.y = 1.0;
            over.z = 1.0;
            break;
          default:
            norm.z = 1.0;
            over.x = 1.0;
            break;
        }
      switch ((i / 3) % 4)
        {
          case 3:
            norm = -norm;
            over = -over;
            break;
          case 2:
            norm = -norm;
            break;
          case 1:
            over = -over;
            break;
            // default not used : would leave norm and over unchanged
        }
      if ((i / 6) % 2 == 1)
        {
          Vect tmp = over;
          over = norm;
          norm = tmp;
        }
      Quat rot = Quat::QRotFromNormOver (norm, over);
      Vect should_be_norm = rot.QuatRotVect (Vect (0., 0., 1.));
      Vect should_be_over = rot.QuatRotVect (Vect (1., 0., 0.));
      EXPECT_TRUE (norm.ApproxEquals (should_be_norm, eps));
      EXPECT_TRUE (over.ApproxEquals (should_be_over, eps));
      // test is a bit *strict* -- but it'll be interesting to see when
      // it fails...
    }
}

TEST_F (QuatTest, QuatAxisAngle)
{
  Vect qq_axis = Vect (0.371390676354104, 0.557086014531156, 0.742781352708208);
  EXPECT_DOUBLE_EQ (1.0, qq_axis.Mag ());
  float64 qq_angle = 2.77438463303196;

  float64 angle = q.ExtractAngle ();
  Vect axis = q.ExtractAxis ();
  EXPECT_NEAR (qq_angle, angle, epslax);
  EXPECT_NEAR (qq_axis.X (), axis.X (), epslax);
  EXPECT_NEAR (qq_axis.Y (), axis.Y (), epslax);
  EXPECT_NEAR (qq_axis.Z (), axis.Z (), epslax);

  Quat q_mineq = Quat::QRotFromAxisAngle (qq_axis, qq_angle);
  EXPECT_TRUE (q.ApproxEquals (q_mineq, epslax));
}

TEST_F (QuatTest, QuatToFromRotationMatrix_Bug_2367)
{
  Vect axis (1, 3, 5);
  axis.NormSelf ();
  float64 angle = 1.23456;
  Quat q_mine = Quat::QRotFromAxisAngle (axis, angle);
  Matrix44 m_mine;
  m_mine.LoadRotation (axis, angle);
  Quat q2 = Quat::QRotFromRotationMatrix (m_mine);
  Matrix44 m2 = q2.DeriveRotationMatrix ();

  EXPECT_TRUE (
    q_mine.QuatRotVect (XZ).ApproxEquals (m_mine.TransformVect (XZ), eps));
  EXPECT_TRUE (q_mine.Conj ().QuatRotVect (XZ).ApproxEquals (
    m_mine.Transpose ().TransformVect (XZ), eps));
  EXPECT_TRUE (
    q2.QuatRotVect (XZ).ApproxEquals (m_mine.TransformVect (XZ), eps));
  EXPECT_TRUE (
    m2.TransformVect (XZ).ApproxEquals (m_mine.TransformVect (XZ), eps));
}

TEST_F (QuatTest, QRotFromCoordTransform_Bug_2367)
{
  // The setup
  Vect xhat (1, 0, 0);
  Vect yhat (0, 1, 0);
  Vect zhat (0, 0, 1);
  Vect x1 = q.QuatRotVect (xhat);
  Vect y1 = q.QuatRotVect (yhat);
  Vect z1 = q.QuatRotVect (zhat);
  Vect x2 = q.QuatRotVect (x1);
  Vect y2 = q.QuatRotVect (y1);
  Vect z2 = q.QuatRotVect (z1);

  // The meat
  Quat qcoord = Quat::QRotFromCoordTransform (x1, y1, z1, x2, y2, z2);
  EXPECT_TRUE (q.RotApproxEquals (qcoord, epslax));
}

TEST_F (QuatTest, QRotFromNormOver)
{
  Vect xhat (1, 0, 0);
  Vect yhat (0, 1, 0);
  Vect zhat (0, 0, 1);

  Vect x2 = q.QuatRotVect (xhat);
  Vect y2 = q.QuatRotVect (yhat);
  Vect z2 = q.QuatRotVect (zhat);

  // rotation from norm-over
  Quat qno = Quat::QRotFromNormOver (z2, x2);
  EXPECT_TRUE (qno.RotApproxEquals (q, eps));

  // inverse rotation
  Quat qconj = q.Conj ();

  Vect x3 = qconj.QuatRotVect (x2);
  Vect y3 = qconj.QuatRotVect (y2);
  Vect z3 = qconj.QuatRotVect (z2);

  EXPECT_TRUE (xhat.ApproxEquals (x3, eps));
  EXPECT_TRUE (yhat.ApproxEquals (y3, eps));
  EXPECT_TRUE (zhat.ApproxEquals (z3, eps));
}

// Test that this has been fixed:
// c:\documents and settings\oblong\src\yoiz\libloam\c++\quat.h(207): warning C4717: 'oblong::loam::Quat::Div' : recursive on all control paths, function will cause runtime stack overflow [c:\Documents and Settings\Oblong\src\yoiz\libLoam\c++\win32\libLoamWin32C++.vcxproj]
TEST_F (QuatTest, Div)
{
  Quat *p = new Quat (FOO);
  q.Div (p);
  delete p;
}

TEST_F (QuatTest, DistFrom)
{
  const Quat q1 (4, 5, 6, 7);
  const Quat q2 (FOO);

  EXPECT_DOUBLE_EQ (8.0, q1.DistFrom (q2));
  EXPECT_DOUBLE_EQ (8.0, q1.DistFrom (&q2));
  EXPECT_DOUBLE_EQ (8.0, q2.DistFrom (q1));
  EXPECT_DOUBLE_EQ (8.0, q2.DistFrom (&q1));

  EXPECT_DOUBLE_EQ (0.0, q1.DistFrom (q1));
  EXPECT_DOUBLE_EQ (0.0, q1.DistFrom (&q1));
  EXPECT_DOUBLE_EQ (0.0, q2.DistFrom (q2));
  EXPECT_DOUBLE_EQ (0.0, q2.DistFrom (&q2));
}


TEST_F (QuatTest, QuatMathAlongAxis)
{
  Quat q_test_exp, q_test_act;
  const float64 epsilon = 0.000001;
  const Vect axis = Vect::UnitRandom ();
  Quat q0 = Quat::QRotFromAxisAngle (axis, M_PI * 0.1);
  EXPECT_EQ (q0 * 2, q0 + q0);
  Quat q1 = Quat::QRotFromAxisAngle (axis, M_PI * 0.4);
  Quat q2 = Quat::QRotFromAxisAngle (axis, M_PI * 0.6);
  q_test_exp = q2.Ln ().Exp ();
  q_test_act = (q1.Ln () + 2 * q0.Ln ()).Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  q_test_exp = q1.Ln ().Exp ();
  q_test_act = (q2.Ln () - 2 * q0.Ln ()).Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  q_test_exp = (q1.Ln () + q0.Ln ()).Exp ();
  q_test_act = (q2.Ln () - q0.Ln ()).Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  q_test_exp = Quat::QRotFromAxisAngle (axis, M_PI * 0.5);
  q_test_act = q1.UnitInterp (0.5, q2);
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  q_test_exp = Quat::QRotFromAxisAngle (axis, M_PI * 0.5);
  q_test_act = (q1.Ln () + q0.Ln ()).Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  q_test_exp = Quat::QRotFromAxisAngle (axis, M_PI * 0.45);
  q_test_act = (q1.Ln () + 0.5 * q0.Ln ()).Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
}

TEST_F (QuatTest, QuatMathDifferentAxes)
{
  Quat q_test_exp, q_test_act;
  const float64 epsilon = 0.000001;
  const Vect axis1 = Vect::UnitRandom ();
  const Vect axis2 = Vect::UnitRandom ();
  Quat q1 = Quat::QRotFromAxisAngle (axis1, M_PI * 0.4);
  Quat q2 = Quat::QRotFromAxisAngle (axis2, M_PI * 0.6);
  Quat q_half = q1.UnitInterp (0.5, q2);
  Quat q_1qtr = q1.UnitInterp (0.25, q2);
  Quat q_3qtr = q1.UnitInterp (0.75, q2);
  q_test_exp = q_half;
  q_test_act = q_1qtr.UnitInterp (0.5, q_3qtr);
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
}

TEST_F (QuatTest, QuatMultiplicationOrder)
{
  const float64 epsilon = 0.000001;
  // point at "over"
  Vect o (1, 0, 0);
  // spin backward to "-norm"
  Quat q_1st = Quat::QRotFromAxisAngle (Vect (0, 1, 0), M_PI * 0.5);
  // then spin around "over" to point downward
  Quat q_2nd = Quat::QRotFromAxisAngle (Vect (1, 0, 0), M_PI * -0.5);
  // right-order multiplication should be in effect
  v = (q_2nd * q_1st).QuatRotVect (o);
  EXPECT_TRUE (v.ApproxEquals (Vect (0, -1, 0), epsilon));
  // (otherwise it will spin around its own axis, no effect, and end up at -n)
  v = (q_1st * q_2nd).QuatRotVect (o);
  EXPECT_TRUE (v.ApproxEquals (Vect (0, 0, -1), epsilon));
  // and just to prove the "no effect" part...
  v = q_1st.QuatRotVect (o);
  EXPECT_TRUE (v.ApproxEquals (Vect (0, 0, -1), epsilon));
}

TEST_F (QuatTest, UnitQuaternions)
{
  Quat q1 = Quat::QRotFromAxisAngle (Vect::UnitRandom (), 0.0);
  EXPECT_EQ (1.0, q1.Mag ());
  EXPECT_EQ (1.0, q1.Invert ().Mag ());
  Quat q2 = Quat::QRotFromAxisAngle (Vect::UnitRandom (), 0.0);
  EXPECT_EQ (1.0, (q1 * q2).Mag ());
  EXPECT_EQ (1.0, (q1.Invert () * q2).Mag ());
}

TEST_F (QuatTest, LogSpaceTransformations)
{
  Quat q_test_exp, q_test_act;
  const float64 epsilon = 0.000001;
  Quat q1 = Quat::QRotFromAxisAngle (Vect::UnitRandom (),
                                     ob_rand_float64 (-0.5 * M_PI, 0.5 * M_PI));
  q_test_exp = q1;
  q_test_act = q1.Ln ().Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  q_test_exp = q1;
  q_test_act = q1.Exp ().Ln ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  Quat q2 = Quat::QRotFromAxisAngle (Vect::UnitRandom (),
                                     ob_rand_float64 (-0.5 * M_PI, 0.5 * M_PI));
  Quat x = q1.Invert () * q2;
  q_test_exp = q2;
  q_test_act = q1 * x;
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
  float64 k = ob_rand_float64 (0.0, 1.0);
  Quat q0 = q1.UnitInterp (k, q2);
  q_test_exp = q0;
  q_test_act = q1 * (k * x.Ln ()).Exp ();
  EXPECT_TRUE (q_test_exp.ApproxEquals (q_test_act, epsilon));
}

TEST (ObResolvedPathTest, ForFile)
{
  // I don't actually care whether merrick-street-mpeg4.mov is found or not.
  // In the spirit of a minimal test, all I care is that the finding or
  // not-finding occurs without tearing any holes in the fabric of
  // spacetime.
  Str s = ObResolvedPath::ForFile (ob_share_path, "merrick-street-mpeg4.mov",
                                   "FR", ObResolvedPath::Verbosity_Taciturn);
}

TEST (ObResolvedPathTest, ForFiles)
{
  ObTrove<Str> t =
    ObResolvedPath::ForFiles (ob_share_path, "merrick-street-mpeg4.mov");
}

class ObMapTest : public ::testing::Test
{
 public:
  ObMap<Str, int64> m;
  void SetUp () override;
};

void ObMapTest::SetUp ()
{
  m.Put ("one", 1);
  m.Put ("two", 2);
  m.Put ("three", 3);
  m.Put ("four", 4);
  m.Put ("five", 5);
}

TEST_F (ObMapTest, PutAtIndexLatest)
{
  EXPECT_TRUE (m.PutAtIndex ("five", -5, 2).IsSplend ());
  EXPECT_EQ (5, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("five", m.NthKey (2));
  EXPECT_EQ (-5, m.NthVal (2));
  EXPECT_STREQ ("three", m.NthKey (3));
  EXPECT_EQ (3, m.NthVal (3));
  EXPECT_STREQ ("four", m.NthKey (4));
  EXPECT_EQ (4, m.NthVal (4));
}

TEST_F (ObMapTest, PutAtIndexEarliest)
{
  m.PrivilegeEarliest ();
  EXPECT_TRUE (m.PutAtIndex ("five", -5, 2).IsSplend ());
  EXPECT_EQ (5, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("three", m.NthKey (2));
  EXPECT_EQ (3, m.NthVal (2));
  EXPECT_STREQ ("four", m.NthKey (3));
  EXPECT_EQ (4, m.NthVal (3));
  EXPECT_STREQ ("five", m.NthKey (4));
  EXPECT_EQ (5, m.NthVal (4));
}

TEST_F (ObMapTest, PutAtIndexDuplicates)
{
  m.AllowDuplicates ();
  EXPECT_TRUE (m.PutAtIndex ("five", -5, 2).IsSplend ());
  EXPECT_EQ (6, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("five", m.NthKey (2));
  EXPECT_EQ (-5, m.NthVal (2));
  EXPECT_STREQ ("three", m.NthKey (3));
  EXPECT_EQ (3, m.NthVal (3));
  EXPECT_STREQ ("four", m.NthKey (4));
  EXPECT_EQ (4, m.NthVal (4));
  EXPECT_STREQ ("five", m.NthKey (5));
  EXPECT_EQ (5, m.NthVal (5));
}

TEST_F (ObMapTest, PutAtIndexLatestNew)
{
  EXPECT_TRUE (m.PutAtIndex ("zero", 0, -5).IsSplend ());
  EXPECT_EQ (6, m.Count ());
  EXPECT_STREQ ("zero", m.NthKey (0));
  EXPECT_EQ (0, m.NthVal (0));
  EXPECT_STREQ ("one", m.NthKey (1));
  EXPECT_EQ (1, m.NthVal (1));
  EXPECT_STREQ ("two", m.NthKey (2));
  EXPECT_EQ (2, m.NthVal (2));
  EXPECT_STREQ ("three", m.NthKey (3));
  EXPECT_EQ (3, m.NthVal (3));
  EXPECT_STREQ ("four", m.NthKey (4));
  EXPECT_EQ (4, m.NthVal (4));
  EXPECT_STREQ ("five", m.NthKey (5));
  EXPECT_EQ (5, m.NthVal (5));
}

TEST_F (ObMapTest, PutAtIndexEarliestNew)
{
  m.PrivilegeEarliest ();
  EXPECT_TRUE (m.PutAtIndex ("zero", 0, -5).IsSplend ());
  EXPECT_EQ (6, m.Count ());
  EXPECT_STREQ ("zero", m.NthKey (0));
  EXPECT_EQ (0, m.NthVal (0));
  EXPECT_STREQ ("one", m.NthKey (1));
  EXPECT_EQ (1, m.NthVal (1));
  EXPECT_STREQ ("two", m.NthKey (2));
  EXPECT_EQ (2, m.NthVal (2));
  EXPECT_STREQ ("three", m.NthKey (3));
  EXPECT_EQ (3, m.NthVal (3));
  EXPECT_STREQ ("four", m.NthKey (4));
  EXPECT_EQ (4, m.NthVal (4));
  EXPECT_STREQ ("five", m.NthKey (5));
  EXPECT_EQ (5, m.NthVal (5));
}

TEST_F (ObMapTest, PutAtIndexDuplicatesNew)
{
  m.AllowDuplicates ();
  EXPECT_TRUE (m.PutAtIndex ("zero", 0, -5).IsSplend ());
  EXPECT_EQ (6, m.Count ());
  EXPECT_STREQ ("zero", m.NthKey (0));
  EXPECT_EQ (0, m.NthVal (0));
  EXPECT_STREQ ("one", m.NthKey (1));
  EXPECT_EQ (1, m.NthVal (1));
  EXPECT_STREQ ("two", m.NthKey (2));
  EXPECT_EQ (2, m.NthVal (2));
  EXPECT_STREQ ("three", m.NthKey (3));
  EXPECT_EQ (3, m.NthVal (3));
  EXPECT_STREQ ("four", m.NthKey (4));
  EXPECT_EQ (4, m.NthVal (4));
  EXPECT_STREQ ("five", m.NthKey (5));
  EXPECT_EQ (5, m.NthVal (5));
}

TEST_F (ObMapTest, RemoveNth)
{
  EXPECT_EQ (OB_OK, m.RemoveNth (0));
  EXPECT_EQ (OB_OK, m.RemoveNth (0));
  EXPECT_EQ (OB_OK, m.RemoveNth (0));
  EXPECT_EQ (OB_OK, m.RemoveNth (0));
  EXPECT_EQ (OB_OK, m.RemoveNth (0));
  EXPECT_EQ (OB_BAD_INDEX, m.RemoveNth (0));
  EXPECT_EQ (OB_BAD_INDEX, m.RemoveNth (0));
  EXPECT_EQ (0, m.Count ());
}

TEST_F (ObMapTest, RemoveByKey)
{
  EXPECT_EQ (OB_OK, m.RemoveByKey ("two"));
  EXPECT_EQ (OB_NOT_FOUND, m.RemoveByKey ("two"));
  EXPECT_EQ (4, m.Count ());
}

TEST_F (ObMapTest, RemoveByVal)
{
  EXPECT_EQ (OB_OK, m.RemoveByVal (3));
  EXPECT_EQ (OB_NOT_FOUND, m.RemoveByVal (3));
  EXPECT_EQ (4, m.Count ());
}

TEST_F (ObMapTest, CompactNulls)
{
  EXPECT_EQ (-1, m.CompactNullKeys ());
  EXPECT_EQ (-1, m.CompactNullVals ());
  EXPECT_EQ (0, m.CompactNulls ());
  EXPECT_EQ (5, m.Count ());
}

TEST_F (ObMapTest, AssignToNthKey)
{
  m.NthKey (2) = "capybara";
  EXPECT_EQ (5, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("capybara", m.NthKey (2));
  EXPECT_EQ (3, m.NthVal (2));
  EXPECT_STREQ ("four", m.NthKey (3));
  EXPECT_EQ (4, m.NthVal (3));
  EXPECT_STREQ ("five", m.NthKey (4));
  EXPECT_EQ (5, m.NthVal (4));
}

TEST_F (ObMapTest, AssignToNthVal)
{
  m.NthVal (2) = 456;
  EXPECT_EQ (5, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("three", m.NthKey (2));
  EXPECT_EQ (456, m.NthVal (2));
  EXPECT_STREQ ("four", m.NthKey (3));
  EXPECT_EQ (4, m.NthVal (3));
  EXPECT_STREQ ("five", m.NthKey (4));
  EXPECT_EQ (5, m.NthVal (4));
}

TEST_F (ObMapTest, AssignToValFromKey)
{
  m.ValFromKey ("three") = 456;
  EXPECT_EQ (5, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("three", m.NthKey (2));
  EXPECT_EQ (456, m.NthVal (2));
  EXPECT_STREQ ("four", m.NthKey (3));
  EXPECT_EQ (4, m.NthVal (3));
  EXPECT_STREQ ("five", m.NthKey (4));
  EXPECT_EQ (5, m.NthVal (4));
}

TEST_F (ObMapTest, AssignToKeyFromVal)
{
  m.KeyFromVal (3) = "capybara";
  EXPECT_EQ (5, m.Count ());
  EXPECT_STREQ ("one", m.NthKey (0));
  EXPECT_EQ (1, m.NthVal (0));
  EXPECT_STREQ ("two", m.NthKey (1));
  EXPECT_EQ (2, m.NthVal (1));
  EXPECT_STREQ ("capybara", m.NthKey (2));
  EXPECT_EQ (3, m.NthVal (2));
  EXPECT_STREQ ("four", m.NthKey (3));
  EXPECT_EQ (4, m.NthVal (3));
  EXPECT_STREQ ("five", m.NthKey (4));
  EXPECT_EQ (5, m.NthVal (4));
}

TEST_F (ObMapTest, Iterator)
{
  const char *const expected[] = {"one", "two", "three", "four", "five"};

  size_t i = 0;
  for (ObMap<Str, int64>::const_iterator it = m.begin (); it != m.end (); ++it)
    {
      EXPECT_STREQ (expected[i++], (*it)->Car ());
      EXPECT_EQ (int64 (i), (*it)->Cdr ());
    }

  EXPECT_EQ (size_t (5), i);
}

TEST_F (ObMapTest, GrowthFactors)
{
  ObMap<int32, Str> mpA;
  ObMap<int32, Str> mpB;
  mpB.SetGrowthFactors (15, 1.0);
  ObMap<int32, Str> mpC (1.6);

  EXPECT_EQ (8u, mpA.ArithmeticGrowthFactor ());
  EXPECT_EQ (1.0, mpA.GeometricGrowthFactor ());
  EXPECT_EQ (0, mpA.Capacity ());
  EXPECT_EQ (4, mpA.NextLargerCapacity ());
  mpA.Put (665, "squee");
  EXPECT_EQ (4, mpA.Capacity ());
  EXPECT_EQ (12, mpA.NextLargerCapacity ());

  EXPECT_EQ (15u, mpB.ArithmeticGrowthFactor ());
  EXPECT_EQ (1.0, mpB.GeometricGrowthFactor ());
  EXPECT_EQ (0, mpB.Capacity ());
  EXPECT_EQ (4, mpB.NextLargerCapacity ());
  mpB.Put (667, "squee");
  EXPECT_EQ (4, mpB.Capacity ());
  EXPECT_EQ (19, mpB.NextLargerCapacity ());

  EXPECT_EQ (8u, mpC.ArithmeticGrowthFactor ());
  EXPECT_EQ (1.6, mpC.GeometricGrowthFactor ());
  EXPECT_EQ (0, mpC.Capacity ());
  EXPECT_EQ (4, mpC.NextLargerCapacity ());
  mpC.Put (999, "squee");
  EXPECT_EQ (4, mpC.Capacity ());
  EXPECT_EQ (12, mpC.NextLargerCapacity ());  // hah!
  mpC.Put (1661, "squee");
  mpC.Put (6116, "squee");
  mpC.Put (9339, "squee");
  mpC.Put (6776, "squee");
  EXPECT_EQ (12, mpC.Capacity ());
  EXPECT_EQ (20, mpC.NextLargerCapacity ());  // again with the hah...
  for (int32 q = 0; q < 8; q++)
    mpC.Put (q + 20202, "squee");
  EXPECT_EQ (20, mpC.Capacity ());
  EXPECT_EQ (32, mpC.NextLargerCapacity ());
}

// demonstrates bug 1302
TEST_F (ObMapTest, AssignToNthKeyNotFound)
{
  EXPECT_TRUE (m.NthKey (111).IsEmpty ());
  m.NthKey (111) = "one hundred eleven";
  EXPECT_TRUE (m.NthKey (112).IsEmpty ());
}

// demonstrates bug 1302
TEST_F (ObMapTest, AssignToKeyFromValNotFound)
{
  EXPECT_TRUE (m.KeyFromVal (111).IsEmpty ());
  m.KeyFromVal (111) = "one hundred eleven";
  EXPECT_TRUE (m.KeyFromVal (112).IsEmpty ());
}

// demonstrates bug 1302
TEST_F (ObMapTest, AssignToNthValNotFound)
{
  ObMap<Str, Matrix44> p;
  EXPECT_TRUE (Matrix44::idMat == p.NthVal (111));
  p.NthVal (111).LoadZero ();
  EXPECT_TRUE (Matrix44::idMat == p.NthVal (112));
}

// demonstrates bug 1302
TEST_F (ObMapTest, AssignToValFromKeyNotFound)
{
  ObMap<Str, Matrix44> p;
  EXPECT_TRUE (Matrix44::idMat == p.ValFromKey ("capybara"));
  p.ValFromKey ("capybara").LoadZero ();
  EXPECT_TRUE (Matrix44::idMat == p.ValFromKey ("hyrax"));
}

class ObMapSortTests : public ::testing::Test
{
 public:
  ObMap<Str, int64> m;
  void SetUp () override;
};

void ObMapSortTests::SetUp ()
{
  m.Put ("one", 1);
  m.Put ("two", 2);
  m.Put ("three", 3);
  m.Put ("four", 4);
  m.Put ("five", 5);
  m.Put ("zero", 0);
  m.Put ("neg-one", -1);
  m.Put ("six", 6);
  m.Put ("neg-three", -3);
}

int dumbpare (ObCons<Str, int64> *const &a, ObCons<Str, int64> *const &b)
{
  int64 aa = a->Cdr ();
  int64 bb = b->Cdr ();
  return (aa - bb);
}

int scumpare (ObCons<Str, int64> *const &a, ObCons<Str, int64> *const &b)
{
  Str aa = a->Car ();
  Str bb = b->Car ();
  return (aa > bb);
}

TEST_F (ObMapSortTests, BasicSort)
{
  m.Sort (dumbpare);
  EXPECT_EQ (-3, m.NthVal (0));
  EXPECT_EQ (-1, m.NthVal (1));
  EXPECT_EQ (0, m.NthVal (2));
  EXPECT_EQ (1, m.NthVal (3));
  EXPECT_EQ (2, m.NthVal (4));
  EXPECT_EQ (3, m.NthVal (5));
  EXPECT_EQ (4, m.NthVal (6));
  EXPECT_EQ (5, m.NthVal (7));
  EXPECT_EQ (6, m.NthVal (8));

  m.Sort (scumpare);
  EXPECT_EQ (5, m.NthVal (0));
  EXPECT_EQ (4, m.NthVal (1));
  EXPECT_EQ (-1, m.NthVal (2));
  EXPECT_EQ (-3, m.NthVal (3));
  EXPECT_EQ (1, m.NthVal (4));
  EXPECT_EQ (6, m.NthVal (5));
  EXPECT_EQ (3, m.NthVal (6));
  EXPECT_EQ (2, m.NthVal (7));
  EXPECT_EQ (0, m.NthVal (8));
}

TEST_F (ObMapSortTests, BasicQuicksort)
{
  m.Quicksort (dumbpare);
  EXPECT_EQ (-3, m.NthVal (0));
  EXPECT_EQ (-1, m.NthVal (1));
  EXPECT_EQ (0, m.NthVal (2));
  EXPECT_EQ (1, m.NthVal (3));
  EXPECT_EQ (2, m.NthVal (4));
  EXPECT_EQ (3, m.NthVal (5));
  EXPECT_EQ (4, m.NthVal (6));
  EXPECT_EQ (5, m.NthVal (7));
  EXPECT_EQ (6, m.NthVal (8));

  m.Quicksort (scumpare);
  EXPECT_EQ (5, m.NthVal (0));
  EXPECT_EQ (4, m.NthVal (1));
  EXPECT_EQ (-1, m.NthVal (2));
  EXPECT_EQ (-3, m.NthVal (3));
  EXPECT_EQ (1, m.NthVal (4));
  EXPECT_EQ (6, m.NthVal (5));
  EXPECT_EQ (3, m.NthVal (6));
  EXPECT_EQ (2, m.NthVal (7));
  EXPECT_EQ (0, m.NthVal (8));
}


int mumppare (const Str &a, const Str &b)
{
  return (a > b);
}

TEST_F (ObMapSortTests, KeySort)
{
  m.SortByKey (mumppare);
  EXPECT_EQ (5, m.NthVal (0));
  EXPECT_EQ (4, m.NthVal (1));
  EXPECT_EQ (-1, m.NthVal (2));
  EXPECT_EQ (-3, m.NthVal (3));
  EXPECT_EQ (1, m.NthVal (4));
  EXPECT_EQ (6, m.NthVal (5));
  EXPECT_EQ (3, m.NthVal (6));
  EXPECT_EQ (2, m.NthVal (7));
  EXPECT_EQ (0, m.NthVal (8));
}

TEST_F (ObMapSortTests, KeyQuicksort)
{
  m.QuicksortByKey (mumppare);
  EXPECT_EQ (5, m.NthVal (0));
  EXPECT_EQ (4, m.NthVal (1));
  EXPECT_EQ (-1, m.NthVal (2));
  EXPECT_EQ (-3, m.NthVal (3));
  EXPECT_EQ (1, m.NthVal (4));
  EXPECT_EQ (6, m.NthVal (5));
  EXPECT_EQ (3, m.NthVal (6));
  EXPECT_EQ (2, m.NthVal (7));
  EXPECT_EQ (0, m.NthVal (8));
}


int lumppare (const int64 &a, const int64 &b)
{
  return (a > b);
}

TEST_F (ObMapSortTests, ValSort)
{
  m.SortByVal (lumppare);
  EXPECT_EQ (-3, m.NthVal (0));
  EXPECT_EQ (-1, m.NthVal (1));
  EXPECT_EQ (0, m.NthVal (2));
  EXPECT_EQ (1, m.NthVal (3));
  EXPECT_EQ (2, m.NthVal (4));
  EXPECT_EQ (3, m.NthVal (5));
  EXPECT_EQ (4, m.NthVal (6));
  EXPECT_EQ (5, m.NthVal (7));
  EXPECT_EQ (6, m.NthVal (8));
}

TEST_F (ObMapSortTests, ValQuicksort)
{
  m.QuicksortByVal (lumppare);
  EXPECT_EQ (-3, m.NthVal (0));
  EXPECT_EQ (-1, m.NthVal (1));
  EXPECT_EQ (0, m.NthVal (2));
  EXPECT_EQ (1, m.NthVal (3));
  EXPECT_EQ (2, m.NthVal (4));
  EXPECT_EQ (3, m.NthVal (5));
  EXPECT_EQ (4, m.NthVal (6));
  EXPECT_EQ (5, m.NthVal (7));
  EXPECT_EQ (6, m.NthVal (8));
}


class StringificationTest : public ::testing::Test
{
 public:
  std::ostringstream ss;
  char crap[300];

  const char *str ()
  {
    ob_safe_copy_string (crap, sizeof (crap), ss.str ().c_str ());
    return crap;
  }
};

TEST_F (StringificationTest, Vect)
{
  EXPECT_STREQ ("v3f64(0.000000, 1.000000, 0.000000)", WHY.AsStr ());
  ss << WHY;
  EXPECT_STREQ (WHY.AsStr (), str ());
}

TEST_F (StringificationTest, Vect4)
{
  EXPECT_STREQ ("v4f64(0.000000, 1.000000, 2.000000, 3.000000)", FOO.AsStr ());
  ss << FOO;
  EXPECT_STREQ (FOO.AsStr (), str ());
}

TEST_F (StringificationTest, Quat)
{
  const Quat q (FOO);
  EXPECT_STREQ ("Quat(0.000000, 1.000000, 2.000000, 3.000000)", q.AsStr ());
  ss << q;
  EXPECT_STREQ (q.AsStr (), str ());
}

TEST_F (StringificationTest, Matrix44)
{
  const Matrix44 m;
  EXPECT_STREQ ("m44float64(1.000000, 0.000000, 0.000000, 0.000000 ; "
                "0.000000, 1.000000, 0.000000, 0.000000 ; "
                "0.000000, 0.000000, 1.000000, 0.000000 ; "
                "0.000000, 0.000000, 0.000000, 1.000000)",
                m.AsStr ());
  ss << m;
  EXPECT_STREQ (m.AsStr (), str ());
}

TEST_F (StringificationTest, ObColor)
{
  const ObColor c (FOO);
  EXPECT_STREQ ("ObColor(0.000000, 1.000000, 2.000000, 3.000000)", c.AsStr ());
  ss << c;
  EXPECT_STREQ (c.AsStr (), str ());
}

TEST_F (StringificationTest, ObRetort)
{
  const ObRetort r;
  ss << r;
  EXPECT_STREQ ("OB_OK", str ());
}

TEST_F (StringificationTest, Str)
{
  const Str s ("crackers don't matter");
  ss << s;
  EXPECT_STREQ ("crackers don't matter", str ());
}
