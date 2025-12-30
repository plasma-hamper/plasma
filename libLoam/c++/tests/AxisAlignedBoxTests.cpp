
/* (c)  oblong industries */

#include "gtest/gtest.h"
#include <unordered_set>
#include "libLoam/c++/AxisAlignedBox.h"

using namespace oblong::loam;

TEST (AxisAlignedBoxTests, AABB_Constructors)
{
  std::vector<Vect> vpoints = {Vect (1, 2, 3), Vect (2, 3, 4), Vect (4, 5, 6),
                               Vect (7, 8, 9)};
  AxisAlignedBox box (vpoints);
  EXPECT_TRUE (box.IsValid ());
  const std::array<Vect, 8> vp_corners = box.Corners ();

  std::array<Vect, 4> apoints = {
    {Vect (1, 2, 3), Vect (2, 3, 4), Vect (4, 5, 6), Vect (7, 8, 9)}};
  box = AxisAlignedBox (apoints);
  EXPECT_TRUE (box.IsValid ());
  const std::array<Vect, 8> ap_corners = box.Corners ();

  ObTrove<Vect> opoints;
  opoints.Append (Vect (1, 2, 3));
  opoints.Append (Vect (2, 3, 4));
  opoints.Append (Vect (4, 5, 6));
  opoints.Append (Vect (7, 8, 9));
  box = AxisAlignedBox (opoints);
  EXPECT_TRUE (box.IsValid ());
  const std::array<Vect, 8> op_corners = box.Corners ();

  // Test construction from an iterator pair.
  auto bad_hash = [] (const Vect &x) -> size_t {
    return 0;
  };
  std::unordered_set<Vect, decltype (bad_hash)> spoints (0, bad_hash);
  spoints.emplace (1, 2, 3);
  spoints.emplace (2, 3, 4);
  spoints.emplace (4, 5, 6);
  spoints.emplace (7, 8, 9);
  box = AxisAlignedBox (std::begin (spoints), std::end (spoints));
  EXPECT_TRUE (box.IsValid ());
  const std::array<Vect, 8> sp_corners = box.Corners ();

  const auto default_constructed_box = AxisAlignedBox ();
  EXPECT_TRUE (default_constructed_box.IsValid ());

  ASSERT_EQ (vp_corners, ap_corners);
  ASSERT_EQ (ap_corners, op_corners);
  ASSERT_EQ (op_corners, sp_corners);
  ASSERT_EQ (sp_corners, vp_corners);
}

TEST (AxisAlignedBoxTests, AABB_Invalid)
{
  {
    const AxisAlignedBox invalid = AxisAlignedBox::Invalid ();
    EXPECT_FALSE (invalid.IsValid ());
    // confirms you can ToStr an Invalid aabb
    EXPECT_TRUE (invalid.AsStr ().Length () > 0);
  }
  {
    const std::vector<Vect> emptyvec;
    const AxisAlignedBox empty{emptyvec};
    EXPECT_FALSE (empty.IsValid ());
  }
}
