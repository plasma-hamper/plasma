
/* (c)  oblong industries */

#include <random>
#include <array>
#include <gtest/gtest.h>
#include "libLoam/c/ob-rand.h"
#include "libLoam/c++/GeomSlab.h"
#include "libLoam/c++/ObRetort.h"
#include "libLoam/c++/AxisAlignedBox.h"
#include "libLoam/c++/Quat.h"
#include "libLoam/c++/Str.h"
#include "libLoam/c++/Vect.h"
#include "libLoam/c++/Rectangle.h"
#include "libLoam/c++/Frustum.h"
#include "libLoam/c++/Sphere.h"
#include "libLoam/c++/Line.h"
#include "libLoam/c++/GeomTransform.h"

using namespace oblong::loam;
using namespace oblong::loam::GeomSlab;

const Vect ZERO (0, 0, 0);

constexpr float64 eps = 1.0e-6;
constexpr int32 outer_loop_times = 1000;
constexpr int32 inner_loop_times = 500;

std::mt19937_64 prng (42);
std::uniform_real_distribution<float64> float_around_zero (-1.0, 1.0);
std::normal_distribution<float64> normal_around_zero (0.0, 1.0);

Quat RandQuat ()
{
  Quat ret{0, 0, 0, 0};
  while (ret.AutoDot () <= 0.0)
    {
      ret.a = float_around_zero (prng);
      ret.i = float_around_zero (prng);
      ret.j = float_around_zero (prng);
      ret.k = float_around_zero (prng);
    }
  ret.NormSelf ();
  return ret;
}

Quat Random180Quat ()
{
  Quat ret{0, 0, 0, 0};
  while (ret.AutoDot () <= 0.0)
    {
      ret.a = 0;
      ret.i = float_around_zero (prng);
      ret.j = float_around_zero (prng);
      ret.k = float_around_zero (prng);
    }
  ret.NormSelf ();
  return ret;
}

Quat AdversarialQuat ()
{
  std::uniform_int_distribution<int32> zero_to_twentythree (0, 23);
  const int32 choice = zero_to_twentythree (prng);

  Quat out{0, 0, 0, 0};

  switch (choice)
    {
      case 0:  // 1 in 3 chance of getting a totally random quat
      case 1:
      case 2:
      case 3:
      case 5:
      case 6:
      case 7:
        out = RandQuat ();
        break;
      // 1 in 4 chance of getting a 180 rotation about a random direction.
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
      case 13:
        out = Random180Quat ();
        break;
      // 1 in 4 chance of getting a 180 rotation about a random axis from
      // { +X, -X, +Y, -Y, +Z, -Z}
      case 14:
        out.i = 1.0;
        break;
      case 15:
        out.i = -1.0;
        break;
      case 16:
        out.j = 1.0;
        break;
      case 17:
        out.j = -1.0;
        break;
      case 18:
        out.k = 1.0;
        break;
      case 19:
        out.k = -1.0;
        break;
      // 1 in 6 chance of getting a quat with no rotation component
      case 20:
      case 21:
      case 22:
      case 23:
      default:
        out.a = 1.0;
        break;
    }

  return out;
}

Vect RandomDir ()
{
  Vect out{0, 0, 0};
  while (out.AutoDot () <= 0.0)
    {
      // this technique generates uniform points on the unit sphere
      // Marsaglia, George. Choosing a Point from the Surface of a Sphere.
      // Ann. Math. Statist. 43 (1972), no. 2, 645--646.
      // doi:10.1214/aoms/1177692644.
      // https://projecteuclid.org/euclid.aoms/1177692644
      out.x = normal_around_zero (prng);
      out.y = normal_around_zero (prng);
      out.z = normal_around_zero (prng);
    }
  out.NormSelf ();
  return out;
}

Vect RandomPoint ()
{
  Vect out{};
  out.x = float_around_zero (prng);
  out.y = float_around_zero (prng);
  out.z = float_around_zero (prng);
  return out;
}

TEST (GeomSlabTest, TimeOneMillionRayRectIntersections)
{
  const Vect up = Vect (0.6, 0.48, 0.64);
  const Vect over = Vect (0.8, -0.36, -0.48);
  const Vect corner = -0.5 * (up + over);
  const Rectangle rect = Rectangle (corner, up, over);

  const Vect rayStart = Vect (0.1, 0.1, 10.0);
  const Vect rayDir = Vect (0.01, 0.01, -1.0);
  const Line ray = Line (rayStart, rayDir);
  const Vect rayTo = rayStart + 20.0 * rayDir;

  bool didHit = true;
  Vect hitPt;
  for (int i = 0; i < 1000000; ++i)
    {
      didHit = didHit
               && GeomSlab::RayRectIntersection (rayStart, rayTo, corner, over,
                                                 up, &hitPt);
      didHit = didHit && GeomSlab::RayRectIntersection (ray, rect, &hitPt);
    }
  EXPECT_TRUE (didHit);
}

TEST (GeomSlabTest, TimeOneMillionRayQuadIntersections)
{
  Vect up = Vect (0.6, 0.48, 0.64);
  const Vect over = Vect (0.8, -0.36, -0.48);

  const Vect rayStart = Vect (0.1, 0.1, 10.0);
  const Vect rayDir = Vect (0.01, 0.01, -1.0);
  const Vect rayTo = rayStart + 20.0 * rayDir;

  const Vect corner = -0.5 * (up + over);

  bool didHit = true;
  Vect hitPt;
  for (int i = 0; i < 1000000; ++i)
    {
      didHit = didHit
               && GeomSlab::RayQuadIntersection (rayStart, rayTo, corner, over,
                                                 up, &hitPt);
    }
  EXPECT_TRUE (didHit);
}

TEST (GeomSlabTest, TimeOneMillionRaySphereIntersections)
{
  const Vect up = Vect (0.6, 0.48, 0.64);
  const Vect over = Vect (0.8, -0.36, -0.48);

  const Vect rayStart = Vect (0.1, 0.1, 10.0);
  const Vect rayDir = Vect (0.01, 0.01, -1.0);
  const Vect rayTo = rayStart + 20.0 * rayDir;

  const Vect corner = -0.5 * (up + over);

  bool didHit = true;
  Vect hitPt;
  for (int i = 0; i < 500000; ++i)
    {
      didHit = didHit
               && GeomSlab::RaySphereForwardIntersection (rayStart, rayTo,
                                                          corner, 8.0, &hitPt);
    }
  for (int i = 0; i < 500000; ++i)
    {
      didHit = didHit
               && GeomSlab::RaySphereForwardIntersection (rayStart, rayTo,
                                                          corner, 12.0, &hitPt);
    }
  EXPECT_TRUE (didHit);
}

TEST (GeomSlabTest, RayRectIntersection)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);
  const Vect yUnit (0.0, 1.0, 0.0);

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect corner = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const bool odd = whichTry & 1;
      const float64 over_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;
      const float64 up_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;

      const Vect over = over_multiplicand * orient.QuatRotVect (xUnit);
      const Vect up = up_multiplicand * orient.QuatRotVect (yUnit);

      for (int i = 0; i < inner_loop_times; ++i)
        {
          const float64 a = ob_rand_state_float64 (-0.5, 1.5, randState);
          const float64 b = ob_rand_state_float64 (-0.5, 1.5, randState);

          const Vect hitPt = corner + a * over + b * up;

          const Vect rayDir = RandomDir ();

          const float64 distAlong = ob_rand_state_float64 (eps, 1.0, randState);

          const Vect rayOrigin = hitPt - distAlong * rayDir;

          Vect shouldMatchHitPt;
          const bool didHit = GeomSlab::RayRectIntersection (
            rayOrigin, hitPt, corner, over, up, &shouldMatchHitPt);

          bool shouldBeInside =
            (a > eps) && (a < 1.0 - eps) && (b > eps) && (b < 1.0 - eps);
          bool shouldBeOutside =
            (a < -eps) || (a > 1.0 + eps) || (b < -eps) || (b > 1.0 + eps);

          bool sliver = false;
          // Be extra-forgiving for sliver polygons
          if (sqrt (over.Dot (up) * over.Dot (up)
                    / (over.Dot (over) * up.Dot (up)))
              > 1.0 - eps)
            {
              sliver = true;
              shouldBeInside = false;  // don't need to be inside of slivers
              if (hitPt.DistFrom (corner) < over.Mag () + up.Mag ())
                shouldBeOutside = false;  // be forgiving here too
            }

          if (shouldBeInside && !didHit)
            {
              OB_LOG_WARNING ("Should have hit, but didn't");
              OB_LOG_WARNING ("%s",
                              (sliver ? "IS a sliver" : "is NOT a sliver"));
              OB_LOG_WARNING ("%s -> %s (along %s)", rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 ());
              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              OB_LOG_WARNING ("Orient is, by the way, %s",
                              orient.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          if (shouldBeOutside && didHit)
            {
              OB_LOG_WARNING ("Should not have hit, but did");
              OB_LOG_WARNING ("%s",
                              (sliver ? "IS a sliver" : "is NOT a sliver"));
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 ());
              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          const float64 hitDeltaSqrd = hitPt.SquaredDistFrom (shouldMatchHitPt);

          if (didHit && hitDeltaSqrd >= eps)
            {
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s, dist is %g",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 (), hitDeltaSqrd);
              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              EXPECT_LT (hitDeltaSqrd, eps);
            }
        }
    }

  ob_rand_free_state (randState);
}



TEST (GeomSlabTest, TestPointInsideRectTest)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);
  const Vect yUnit (0.0, 1.0, 0.0);

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect center = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const bool odd = whichTry & 1;
      const float64 over_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;
      const float64 up_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;

      const Vect over = over_multiplicand * orient.QuatRotVect (xUnit);
      const Vect up = up_multiplicand * orient.QuatRotVect (yUnit);

      for (int i = 0; i < inner_loop_times; ++i)
        {
          const float64 a = ob_rand_state_float64 (-0.5, 1.5, randState);
          const float64 b = ob_rand_state_float64 (-0.5, 1.5, randState);

          const Vect point = center + (a - 0.5) * over + (b - 0.5) * up;

          const bool didHit =
            GeomSlab::PointInsideRectTest (point, center, 0.5 * over, 0.5 * up);

          const bool shouldBeInside =
            (a > eps) && (a < 1.0 - eps) && (b > eps) && (b < 1.0 - eps);
          const bool shouldBeOutside =
            (a < -eps) || (a > 1.0 + eps) || (b < -eps) || (b > 1.0 + eps);

          if (shouldBeInside && !didHit)
            {
              OB_LOG_WARNING ("Should have hit, but didn't");
              OB_LOG_WARNING ("Center %s, up %s, over %s, pt %s",
                              center.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 (), point.AsStr ().utf8 ());
              OB_LOG_WARNING ("Orient is, by the way, %s",
                              orient.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          if (shouldBeOutside && didHit)
            {
              OB_LOG_WARNING ("Should not have hit, but did");
              OB_LOG_WARNING ("Center %s, up %s, over %s, pt %s",
                              center.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 (), point.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }
        }
    }

  ob_rand_free_state (randState);
}


TEST (GeomSlabTest, RayQuadIntersection)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect corner = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const bool odd = whichTry & 1;
      const float64 over_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;
      const float64 up_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;

      const Vect over = over_multiplicand * orient.QuatRotVect (xUnit);

      const Vect randodir = RandomDir ();
      const float64 angle =
        std::uniform_real_distribution<float64> (0.05, M_PI - 0.05) (prng);
      const Vect up =
        up_multiplicand * orient.QuatRotVect (xUnit.Rotate (randodir, angle));
      const Rectangle parallelogram = Rectangle (corner, over, up);

      for (int i = 0; i < inner_loop_times; ++i)
        {
          const float64 a = ob_rand_state_float64 (-0.5, 1.5, randState);
          const float64 b = ob_rand_state_float64 (-0.5, 1.5, randState);

          const Vect hitPt = corner + a * over + b * up;

          const Vect rayDir = RandomDir ();

          const float64 distAlong = ob_rand_state_float64 (eps, 1.0, randState);

          const Vect rayOrigin = hitPt - distAlong * rayDir;
          const Line ray = Line (rayOrigin, rayDir);

          Vect shouldMatchHitPt;
          const bool didHit =
            GeomSlab::RayQuadIntersection (rayOrigin, hitPt, corner, over, up,
                                           &shouldMatchHitPt)
            && GeomSlab::RayQuadIntersection (ray, parallelogram,
                                              &shouldMatchHitPt);

          bool shouldBeInside =
            (a > eps) && (a < 1.0 - eps) && (b > eps) && (b < 1.0 - eps);
          bool shouldBeOutside =
            (a < -eps) || (a > 1.0 + eps) || (b < -eps) || (b > 1.0 + eps);
          bool sliver = false;

          // Be extra-forgiving for sliver polygons
          if (sqrt (over.Dot (up) * over.Dot (up)
                    / (over.Dot (over) * up.Dot (up)))
              > 1.0 - eps)
            {
              sliver = true;
              shouldBeInside = false;  // don't need to be inside of slivers
              if (hitPt.DistFrom (corner) < over.Mag () + up.Mag ())
                shouldBeOutside = false;  // be forgiving here too
            }

          if (shouldBeInside && !didHit)
            {
              OB_LOG_WARNING ("Should have hit, but didn't");
              OB_LOG_WARNING ("%s",
                              (sliver ? "IS a sliver" : "is NOT a sliver"));
              if (!sliver)
                {
                  OB_LOG_ERROR ("(sqrt (over . Dot (up) * over . Dot (up) /"
                                "over . Dot (over) * up . Dot (up)) == %g\n"
                                " 1.0 - eps == %g\neps == %g",
                                (sqrt (over.Dot (up) * over.Dot (up)
                                       / over.Dot (over) * up.Dot (up))),
                                1.0 - eps, eps);
                }
              OB_LOG_WARNING ("%s -> %s (along %s)", rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 ());

              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              OB_LOG_WARNING ("Orient is, by the way, %s",
                              orient.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          if (shouldBeOutside && didHit)
            {
              OB_LOG_WARNING ("Should not have hit, but did");
              OB_LOG_WARNING ("%s",
                              (sliver ? "IS a sliver" : "is NOT a sliver"));
              if (!sliver)
                {
                  OB_LOG_ERROR ("(sqrt (over . Dot (up) * over . Dot (up) /"
                                "over . Dot (over) * up . Dot (up)) == %g\n"
                                " 1.0 - eps == %g\neps == %g",
                                sqrt (over.Dot (up) * over.Dot (up)
                                      / (over.Dot (over) * up.Dot (up))),
                                1.0 - eps, eps);
                }
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 ());
              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          const float64 hitDeltaSqrd = hitPt.SquaredDistFrom (shouldMatchHitPt);

          if (didHit && hitDeltaSqrd >= eps)
            {
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s, dist is %g",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 (), hitDeltaSqrd);
              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              EXPECT_LT (hitDeltaSqrd, eps);
            }
        }
    }

  ob_rand_free_state (randState);
}

static bool _noisy_RayTriangleIntersection (const Vect &from, const Vect &to,
                                            const Vect &corner,
                                            const Vect &leg1, const Vect &leg2,
                                            v3float64 *fill_v)
{
  // Almost-verbatim copy of old code.
  // Fix to use standard tricks if it proves buggy via tests
  const Vect aimer = to - from;
  const Vect n = leg1.Cross (leg2).Norm ();
  float64 t = aimer.Dot (n);
  if (t == 0.0)  // aim parallel to target plane, you see
    return false;
  t = (corner - from).Dot (n) / t;
  if (t < 0.0)  // it's all behind us, now
    return false;
  const Vect hit (from + t * aimer);

  const Vect c1 = hit - corner;
  const Vect c2 = c1 - leg1;
  const Vect c3 = c1 - leg2;

  OB_LOG_ERROR ("c1 = %s\nc2 = %s\nc3 = %s", c1.AsStr ().utf8 (),
                c2.AsStr ().utf8 (), c3.AsStr ().utf8 ());

  const Vect n1 = leg1.Cross (c1);
  const Vect n2 = (leg2 - leg1).Cross (c2);
  const Vect n3 = (-leg2).Cross (c3);

  OB_LOG_ERROR ("n1 = %s\nn2 = %s\nn3 = %s", n1.AsStr ().utf8 (),
                n2.AsStr ().utf8 (), n3.AsStr ().utf8 ());

  if (n1.Dot (n2) < 0.0 || n2.Dot (n3) < 0.0)
    return false;

  OB_LOG_ERROR ("n1 . Dot (n2) == %g\nn2 . Dot (n3) == %g", n1.Dot (n2),
                n2.Dot (n3));

  if (NULL != fill_v)
    *fill_v = hit;
  return true;
}


TEST (GeomSlabTest, RayTriangleIntersection)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);
  const Vect yUnit (0.0, 1.0, 0.0);

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect corner = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const bool odd = whichTry & 1;
      const float64 over_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;
      const float64 up_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;

      const Vect over = over_multiplicand * orient.QuatRotVect (xUnit);

      const Vect randodir = RandomDir ();
      const float64 angle =
        std::uniform_real_distribution<float64> (0.05, M_PI - 0.05) (prng);
      const Vect up =
        up_multiplicand * orient.QuatRotVect (xUnit.Rotate (randodir, angle));

      for (int i = 0; i < inner_loop_times; ++i)
        {
          const float64 a = ob_rand_state_float64 (-0.5, 1.5, randState);
          const float64 b = ob_rand_state_float64 (-0.5, 1.5, randState);

          const Vect hitPt = corner + a * over + b * up;

          const Vect rayDir = RandomDir ();

          const float64 distAlong = ob_rand_state_float64 (eps, 1.0, randState);

          const Vect rayOrigin = hitPt - distAlong * rayDir;

          Vect shouldMatchHitPt;
          const bool didHit = GeomSlab::RayTriangleIntersection (
            rayOrigin, hitPt, corner, over, up, &shouldMatchHitPt);

          // Be extra-forgiving for sliver polygons
          const bool sliver = sqrt (over.Dot (up) * over.Dot (up)
                                    / (over.Dot (over) * up.Dot (up)))
                              > 1.0 - eps;
          const bool shouldBeInside =
            sliver ? false : (a > eps) && (a + b < 1.0 - eps) && (b > eps);
          const bool shouldBeOutside =
            sliver && hitPt.DistFrom (corner) < over.Mag () + up.Mag ()
              ? false
              : (a < -eps) || (a + b > 1.0 + eps) || (b < -eps);

          if (shouldBeInside && !didHit)
            {
              OB_LOG_WARNING ("Should have hit, but didn't");
              OB_LOG_WARNING ("%s",
                              (sliver ? "IS a sliver" : "is NOT a sliver"));
              OB_LOG_WARNING ("%s -> %s (along %s)", rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 ());

              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());

              OB_LOG_WARNING ("Orient is, by the way, %s",
                              orient.AsStr ().utf8 ());

              OB_LOG_WARNING ("a is %g, b is %g", a, b);

              EXPECT_FALSE (true);
            }

          if (shouldBeOutside && didHit)
            {
              OB_LOG_WARNING ("Should not have hit, but did");
              OB_LOG_WARNING ("%s",
                              (sliver ? "IS a sliver" : "is NOT a sliver"));
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 ());

              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());

              OB_LOG_WARNING ("a is %g, b is %g", a, b);

              _noisy_RayTriangleIntersection (rayOrigin, hitPt, corner, over,
                                              up, &shouldMatchHitPt);
              EXPECT_FALSE (true);
            }

          const float64 hitDeltaSqrd = hitPt.SquaredDistFrom (shouldMatchHitPt);

          if (didHit && hitDeltaSqrd >= eps)
            {
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s, dist is %g",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 (), hitDeltaSqrd);

              OB_LOG_WARNING ("Corner %s, up %s, over %s",
                              corner.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());

              EXPECT_LT (hitDeltaSqrd, eps);
            }
        }
    }

  ob_rand_free_state (randState);
}



TEST (GeomSlabTest, RayEllipseIntersection)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);
  const Vect yUnit (0.0, 1.0, 0.0);

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect center = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const bool odd = whichTry & 1;
      const float64 over_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;
      const float64 up_multiplicand =
        odd ? ob_rand_state_float64 (0.5, 10.5, randState) : 1.0;

      const Vect over = over_multiplicand * orient.QuatRotVect (xUnit);
      const Vect up = up_multiplicand * orient.QuatRotVect (yUnit);

      for (int i = 0; i < inner_loop_times; ++i)
        {
          const float64 a = ob_rand_state_float64 (-0.5, 1.5, randState);
          const float64 b = ob_rand_state_float64 (-0.5, 1.5, randState);

          const Vect hitPt = center + a * over + b * up;

          const Vect rayDir = RandomDir ();

          const float64 distAlong = ob_rand_state_float64 (eps, 1.0, randState);

          const Vect rayOrigin = hitPt - distAlong * rayDir;

          Vect shouldMatchHitPt;
          const bool didHit = GeomSlab::RayEllipseIntersection (
            rayOrigin, hitPt, center, over, up, &shouldMatchHitPt);

          const bool shouldBeInside = (sqrt (a * a + b * b) < (1.0 - eps));
          const bool shouldBeOutside = (sqrt (a * a + b * b) > (1.0 + eps));


          if (shouldBeInside && !didHit)
            {
              OB_LOG_WARNING ("Should have hit, but didn't");
              OB_LOG_WARNING ("%s -> %s (along %s)", rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 ());
              OB_LOG_WARNING ("Center %s, up %s, over %s",
                              center.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              OB_LOG_WARNING ("Orient is, by the way, %s",
                              orient.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          if (shouldBeOutside && didHit)
            {
              OB_LOG_WARNING ("Should not have hit, but did");
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 ());
              OB_LOG_WARNING ("Center %s, up %s, over %s",
                              center.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              EXPECT_FALSE (true);
            }

          const float64 hitDeltaSqrd = hitPt.SquaredDistFrom (shouldMatchHitPt);

          if (didHit && hitDeltaSqrd >= eps)
            {
              OB_LOG_WARNING ("%s -> %s (along %s) hits at %s, dist is %g",
                              rayOrigin.AsStr ().utf8 (),
                              hitPt.AsStr ().utf8 (), rayDir.AsStr ().utf8 (),
                              shouldMatchHitPt.AsStr ().utf8 (), hitDeltaSqrd);
              OB_LOG_WARNING ("Center %s, up %s, over %s",
                              center.AsStr ().utf8 (), over.AsStr ().utf8 (),
                              up.AsStr ().utf8 ());
              EXPECT_LT (hitDeltaSqrd, eps);
            }
        }
    }

  ob_rand_free_state (randState);
}


TEST (GeomSlabTest, TestLineSphereBothIntersections)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);
  const Vect yUnit (0.0, 1.0, 0.0);
  const Vect zUnit (0.0, 0.0, 1.0);

  Vect shouldMatchHitPt;

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect center = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const Vect over = orient.QuatRotVect (xUnit);
      const Vect up = orient.QuatRotVect (yUnit);
      const Vect rayDir = orient.QuatRotVect (zUnit);

      for (int i = 0; i < inner_loop_times; i += 81)
        {
          const float64 radius = exp ((float64) ((i % 10) - 4.0));
          const float64 back = radius * exp ((float64) ((i / 20) % 5 - 1));
          for (int ix = 0; ix < 9; ++ix)
            {
              const float64 a = 0.25 * (ix - 4);
              for (int iy = 0; iy < 9; ++iy)
                {
                  const float b = 0.25 * (iy - 4);
                  const Vect rayOrigin =
                    center + radius * (a * over + b * up) - back * rayDir;

                  Vect isectMidpoint;
                  float64 halfDist, alongDist;
                  const bool didHit = GeomSlab::LineSphereBothIntersections (
                    rayOrigin, rayDir, center, radius, &isectMidpoint,
                    &halfDist, &alongDist);

                  if (didHit)
                    {
                      float64 shouldBeSmall = fabs ((back / alongDist) - 1.0);

                      ASSERT_LT (shouldBeSmall, eps);

                      float64 shouldBeHalfDist =
                        radius * sqrt (1.0 - a * a - b * b);

                      if (shouldBeHalfDist > 0.0)
                        {
                          shouldBeSmall =
                            fabs ((halfDist / shouldBeHalfDist) - 1.0);

                          if (shouldBeSmall > eps)
                            {
                              OB_LOG_ERROR ("half dist returns %g, "
                                            "should be %g\n"
                                            "ratio %g\nshouldBeSmall %g",
                                            halfDist, shouldBeHalfDist,
                                            halfDist / shouldBeHalfDist,
                                            shouldBeSmall);
                            }
                          ASSERT_LT (shouldBeSmall, eps);
                        }
                    }

                  bool shouldHit = sqrt (a * a + b * b) + eps < 1.0;
                  bool shouldMiss = sqrt (a * a + b * b) - eps > 1.0;

                  if (didHit && shouldMiss)
                    {
                      OB_LOG_ERROR ("Hit and should not have");
                      OB_LOG_ERROR ("isectMidpoint is %s\n"
                                    "halfDist is %g\n"
                                    "alongDist is %g",
                                    isectMidpoint.AsStr ().utf8 (), halfDist,
                                    alongDist);
                    }
                  if ((!didHit) && shouldHit)
                    OB_LOG_ERROR ("Did not hit and should have");
                  if ((didHit && shouldMiss) || ((!didHit) && shouldHit))
                    {
                      OB_LOG_ERROR ("(a, b) = (%g, %g)", a, b);
                      OB_LOG_ERROR ("Center %s\n"
                                    "RayOrigin %s\n"
                                    "RayDir %s\n"
                                    "radius %g",
                                    center.AsStr ().utf8 (),
                                    rayOrigin.AsStr ().utf8 (),
                                    rayDir.AsStr ().utf8 (), radius);
                      EXPECT_FALSE (true);
                    }
                }
            }
        }
    }

  ob_rand_free_state (randState);
}

TEST (GeomSlabTest, TestRaySphereForwardBackwardIsect)
{
  ob_rand_t *randState = ob_rand_allocate_state (0);

  const Vect xUnit (1.0, 0.0, 0.0);
  const Vect yUnit (0.0, 1.0, 0.0);
  const Vect zUnit (0.0, 0.0, 1.0);

  for (int whichTry = 0; whichTry < outer_loop_times; ++whichTry)
    {
      const Vect center = RandomPoint ();
      const Quat orient = AdversarialQuat ();

      const Vect over = orient.QuatRotVect (xUnit);
      const Vect up = orient.QuatRotVect (yUnit);
      const Vect rayDir = orient.QuatRotVect (zUnit);

      for (int i = 0; i < inner_loop_times; i += 81)
        {
          const float64 radius = exp ((i % 10) - 4.0);
          const float64 back = radius * 0.3 * ((i / 10) % 15 - 7);
          for (int ix = 0; ix < 9; ++ix)
            {
              const float64 a = 0.25 * (ix - 4);
              for (int iy = 0; iy < 9; ++iy)
                {
                  const float b = 0.25 * (iy - 4);
                  const Vect rayOrigin =
                    center + radius * (a * over + b * up) - back * rayDir;
                  const Vect rayTo = rayOrigin + 5.0 * rayDir;
                  const Vect backwardsTo = rayOrigin - 5.0 * rayDir;

                  Vect hitPt;
                  const bool didHit = GeomSlab::RaySphereForwardIntersection (
                    rayOrigin, rayTo, center, radius, &hitPt);
                  Vect backHitPt;
                  const bool backwardsHit =
                    GeomSlab::RaySphereBackwardIntersection (
                      rayOrigin, backwardsTo, center, radius, &backHitPt);


                  const bool lineShouldHit = sqrt (a * a + b * b) < (1.0 - eps);
                  const bool lineShouldNotHit =
                    sqrt (a * a + b * b) > (1.0 + eps);

                  bool rayShouldHit = lineShouldHit;
                  bool rayShouldNotHit = lineShouldNotHit;

                  const float64 halfDist =
                    lineShouldHit ? radius * sqrt (1.0 - a * a - b * b) : 0.0;
                  float64 distAlongRayShouldBe = 0.0;
                  if (lineShouldHit)
                    {
                      distAlongRayShouldBe = back - halfDist;
                      if (distAlongRayShouldBe < 0.0)
                        distAlongRayShouldBe += 2.0 * halfDist;
                      if (distAlongRayShouldBe < eps)
                        rayShouldHit = false;
                      if (distAlongRayShouldBe < -eps)
                        rayShouldNotHit = true;
                    }
                  const bool resultsMeanSomething =
                    rayShouldNotHit || rayShouldHit;
                  if (resultsMeanSomething
                      && ((didHit && rayShouldNotHit)
                          || (!didHit && rayShouldHit)
                          || (backwardsHit != didHit)))
                    {
                      OB_LOG_ERROR ("rayOrigin %s\n"
                                    "rayDir %s\n"
                                    "center %s\n"
                                    "radius %g",
                                    rayOrigin.AsStr ().utf8 (),
                                    rayDir.AsStr ().utf8 (),
                                    center.AsStr ().utf8 (), radius);
                      EXPECT_FALSE (didHit && rayShouldNotHit);
                      EXPECT_FALSE ((!didHit) && rayShouldHit);

                      if (backwardsHit != didHit)
                        {
                          OB_LOG_WARNING ("Backwards should match forwards\n"
                                          "backwardsTo is %s\n"
                                          "forwardsTo is %s",
                                          backwardsTo.AsStr ().utf8 (),
                                          rayTo.AsStr ().utf8 ());
                          if (backwardsHit)
                            OB_LOG_WARNING ("Backwards hit at %s",
                                            backHitPt.AsStr ().utf8 ());
                          else
                            OB_LOG_WARNING ("Forwards hit at %s",
                                            hitPt.AsStr ().utf8 ());
                          EXPECT_FALSE (true);
                        }
                    }
                }
            }
        }
    }
  ob_rand_free_state (randState);
}

TEST (GeomSlabTest, Sphere_Frustum_Intersections)
{
  /**
(-3, 3, -2)                    (3, 3, -2)
    \                              /
     o----------------------------o
     |\           FAR            /|
     | \                        / |
     |  \                      /  |
     |   \(-2, 2, 2) (2, 2, 2)/   |
     |    \ /              \ /    |
     |     o----------------o     |
     |     |                |     |
     |     |                |     |
     o-----+----------------+-----o
      \    |                |    / \
       \   |                |   / (3, -3, -2)
        \  |                |  /
         \ |                | /
          \|      near      |/
           o----------------o
          /                  \
    (-2, -2, 2)           (2, -2, 2)
   */
  std::array<Vect, 8> initial_corners = {
    {Vect (-2, -2, 2), Vect (2, -2, 2), Vect (-2, 2, 2), Vect (2, 2, 2),
     Vect (-3, -3, -2), Vect (3, -3, -2), Vect (-3, 3, -2), Vect (3, 3, -2)}};
  Frustum frust = Frustum (initial_corners);
  EXPECT_TRUE (PointInsideFrustum (Vect (0, 0, 0), frust));
  for (auto corner : frust.Corners ())
    {
      EXPECT_TRUE (PointInsideFrustum (corner, frust));
    }

  {  // intersecting spheres
    const auto small_sphere_inside_frustum = Sphere (Vect (1, 1, 1), 0.5);
    EXPECT_TRUE (FrustumSphereIntersect (frust, small_sphere_inside_frustum));
    const auto center_inside_frustum_and_wholly_containing_frustum =
      Sphere (Vect (0, 0, 1), 10.0);
    EXPECT_TRUE (FrustumSphereIntersect (
      frust, center_inside_frustum_and_wholly_containing_frustum));
    const auto center_outside_frustum_but_wholly_containing_frustum =
      Sphere (Vect (15, 15, 15), 100.0);
    EXPECT_TRUE (FrustumSphereIntersect (
      frust, center_outside_frustum_but_wholly_containing_frustum));
    const auto center_outside_frustum_with_partial_intersection =
      Sphere (Vect (0, 0, 100), 100);
    EXPECT_TRUE (FrustumSphereIntersect (
      frust, center_outside_frustum_with_partial_intersection));
    const auto far_from_frustum = Sphere (Vect (15, 15, 15), 1.0);
    EXPECT_FALSE (FrustumSphereIntersect (frust, far_from_frustum));
  }
  {  // non-intersecting spheres
    const auto huge_and_close_to_near_plane = Sphere (Vect (0, 0, 102.25), 100);
    EXPECT_FALSE (FrustumSphereIntersect (frust, huge_and_close_to_near_plane));
    /*
    unfortunately, our current implementation fails this test because the
    sphere intersects several of the frustum's planes, it just doesn't intersect
    them where the frustum actually is
    const auto huge_and_close_to_a_corner_but_not_intersecting =
      Sphere (Vect (579.639, -579.639, 579.639), 1000);
    EXPECT_FALSE (FrustumSphereIntersect (
      frust, huge_and_close_to_a_corner_but_not_intersecting));
      */
  }
}

TEST (GeomSlabTest, AABB_Frustum_Intersections)
{
  std::vector<AxisAlignedBox> boxes;
  for (float64 i = -5; i <= 5; ++i)
    {
      for (float64 j = -5; j <= 5; ++j)
        {
          for (float64 k = -5; k <= 5; ++k)
            {
              std::vector<Vect> points = {Vect (i - 0.5, j - 0.5, k - 0.5),
                                          Vect (i + 0.5, j + 0.5, k + 0.5)};
              AxisAlignedBox box (points);
              boxes.push_back (box);
            }
        }
    }
  std::array<Vect, 8> initial_corners = {
    {Vect (-2, -2, 2), Vect (2, -2, 2), Vect (-2, 2, 2), Vect (2, 2, 2),
     Vect (-3, -3, -2), Vect (3, -3, -2), Vect (-3, 3, -2), Vect (3, 3, -2)}};
  Frustum initial_frustum = Frustum (initial_corners);
  EXPECT_TRUE (PointInsideFrustum (Vect (0, 0, 0), initial_frustum));
  for (auto corner : initial_frustum.Corners ())
    {
      EXPECT_TRUE (PointInsideFrustum (corner, initial_frustum));
    }

#ifdef __SANITIZE_ADDRESS__
#pragma message                                                                \
  "Test AABB_Frustum_Intersections is very slow under asan, abbreviating"
  const int32 iters = 20;
#else
  const int32 iters = 1000;
#endif

  for (int32 asdasd = 0; asdasd < iters; ++asdasd)
    {
      const Quat rotation = AdversarialQuat ();
      const Matrix44 rotation_matrix = rotation.DeriveRotationMatrix ();
      const Frustum frust =
        GeomTransform::Mult (initial_frustum, rotation_matrix);
      int64 intersecting = 0;
      for (auto box : boxes)
        {
          if (BoxFrustumIntersect (box, frust))
            {
              intersecting += 1;
            }
          else
            {
              for (auto corner : box.Corners ())
                ASSERT_FALSE (PointInsideFrustum (corner, frust));
            }
        }
      // 0.101 = ratio of frustum volume to volume of AxisAlignedBox grid
      ASSERT_GT (intersecting, boxes.size () * 0.101);
      // 0.296 = ratio of dilated frustum volume to grid volume
      ASSERT_LT (intersecting, boxes.size () * 0.296);
    }
}

TEST (GeomSlabTest, AABB_Sphere_Intersections)
{
  const AxisAlignedBox big_box_around_origin =
    AxisAlignedBox ({Vect (1, 1, 1), Vect (-1, -1, -1)});
  const AxisAlignedBox zero_size_box_around_origin = AxisAlignedBox ({Vect ()});
  const Sphere big_sphere_at_origin = Sphere (Vect (), 100);
  EXPECT_TRUE (
    BoxSphereIntersect (big_box_around_origin, big_sphere_at_origin));
  EXPECT_TRUE (
    BoxSphereIntersect (zero_size_box_around_origin, big_sphere_at_origin));
  const Sphere big_sphere_far_away = Sphere (Vect (1000, 1000, 1000), 100);
  EXPECT_FALSE (
    BoxSphereIntersect (big_box_around_origin, big_sphere_far_away));
  EXPECT_FALSE (
    BoxSphereIntersect (zero_size_box_around_origin, big_sphere_far_away));
  const Sphere tiny_sphere_near_corner = Sphere (Vect (1, 1, 1), 0.25);
  EXPECT_TRUE (
    BoxSphereIntersect (big_box_around_origin, tiny_sphere_near_corner));
  EXPECT_FALSE (
    BoxSphereIntersect (zero_size_box_around_origin, tiny_sphere_near_corner));
  const Sphere big_and_partially_in_big_box = Sphere (Vect (100, 0, 0), 99.5);
  EXPECT_TRUE (
    BoxSphereIntersect (big_box_around_origin, big_and_partially_in_big_box));
  EXPECT_FALSE (BoxSphereIntersect (zero_size_box_around_origin,
                                    big_and_partially_in_big_box));
  const Sphere big_and_near_corner_but_not_intersecting =
    Sphere (Vect (100, 100, 100), 171.25);
  EXPECT_FALSE (BoxSphereIntersect (big_box_around_origin,
                                    big_and_near_corner_but_not_intersecting));
  EXPECT_FALSE (BoxSphereIntersect (zero_size_box_around_origin,
                                    big_and_near_corner_but_not_intersecting));
  const Sphere big_and_near_corner_and_intersecting =
    Sphere (Vect (100, 100, 100), 171.75);
  EXPECT_TRUE (BoxSphereIntersect (big_box_around_origin,
                                   big_and_near_corner_and_intersecting));
}

TEST (GeomSlabTest, AABB_Ray_Intersections)
{
  const AxisAlignedBox big_box_around_origin =
    AxisAlignedBox ({Vect (1, 1, 1), Vect (-1, -1, -1)});
  const AxisAlignedBox zero_size_box_around_origin = AxisAlignedBox ({Vect ()});

  const Line origin_through_x = Line (Vect (), Vect (1, 0, 0));
  EXPECT_TRUE (
    BoxRayIntersection (zero_size_box_around_origin, origin_through_x));
  EXPECT_TRUE (BoxRayIntersection (big_box_around_origin, origin_through_x));

  const Line on_corner_heading_away = Line (Vect (1, 1, 1), Vect (1, 0, 0));
  EXPECT_FALSE (
    BoxRayIntersection (zero_size_box_around_origin, on_corner_heading_away));
  EXPECT_TRUE (
    BoxRayIntersection (big_box_around_origin, on_corner_heading_away));

  const Line on_corner_inward = Line (Vect (1, 1, 1), Vect (-1, -1, -1));
  EXPECT_TRUE (BoxRayIntersection (big_box_around_origin, on_corner_inward));

  const Line faraway_along_x = Line (Vect (0, 5, 0), Vect (1, 0, 0));
  EXPECT_FALSE (
    BoxRayIntersection (zero_size_box_around_origin, faraway_along_x));
  EXPECT_FALSE (BoxRayIntersection (big_box_around_origin, faraway_along_x));

  const Line through_corner = Line (Vect (-2, 0, 0), Vect (1, 1, 1));
  EXPECT_FALSE (
    BoxRayIntersection (zero_size_box_around_origin, through_corner));
  EXPECT_TRUE (BoxRayIntersection (big_box_around_origin, through_corner));
}
