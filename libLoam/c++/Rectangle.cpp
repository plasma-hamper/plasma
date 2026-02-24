
/* (c)  oblong industries */

#include "Rectangle.h"
using namespace oblong::loam;

Rectangle::Rectangle (Vect corner_, Vect leg1_, Vect leg2_)
    : corner (corner_), leg1 (leg1_), leg2 (leg2_)
{
}

Str Rectangle::AsStr () const
{
  return "Rectangle[c=" + corner.AsStr () + ", l1=" + leg1.AsStr ()
         + ", l2=" + leg2.AsStr () + "]";
}

bool Rectangle::IsValid () const
{
  return corner.IsValid () && leg1.IsValid () && leg2.IsValid ();
}

Rectangle Rectangle::Invalid ()
{
  return Rectangle (Vect::Invalid (), Vect::Invalid (), Vect::Invalid ());
}
