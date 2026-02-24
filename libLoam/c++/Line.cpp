
/* (c)  oblong industries */

#include "Line.h"


using namespace oblong::loam;


static const Vect zaxis (0.0, 0.0, 1.0);


Line::Line () : dir (zaxis) {}

Line::Line (const Vect &pointOnLine, const Vect &direction)
    : point (pointOnLine), dir (direction)
{
  dir.NormSelf ();
}


void Line::Set (const Vect &pointOnLine, const Vect &direction)
{
  point = pointOnLine;
  dir = direction.Norm ();
}


bool Line::IsValid () const
{
  return (point.IsValid () && (fabs (dir.AutoDot () - 1.0) < 1.0e-7));
}


Line Line::Invalid ()
{
  Vect foul_pnt = Vect ().SetInvalid ();
  Vect foul_dir;
  return Line (foul_pnt, foul_dir);
}


Vect Line::Project (const Vect &q, float64 *qu) const
{
  Vect pq = q - point;
  float64 len = pq.Dot (dir);
  if (qu)
    *qu = len;
  return (point + dir * len);
}


float64 Line::DistFrom (const Vect &q) const
{
  Vect a = q - point;
  Vect b = a - dir;
  return a.Cross (b).Mag ();
}


Vect Line::Intersect (const Line &line) const
{
  if (!IsValid () || !line.IsValid ())
    return Vect::Invalid ();

  float64 uv = dir.Dot (line.Dir ());
  if (fabs (uv) == 1.0)
    return Vect::Invalid ();  // parallel

  Vect ab = line.Point () - point;
  float64 abu = ab.Dot (dir);
  float64 abv = ab.Dot (line.Dir ());

  float64 norm = 1.0 / (1.0 - uv * uv);
  float64 alpha = (abu - abv * uv) * norm;
  float64 beta = (abu * uv - abv) * norm;

  return (PointAlong (alpha) + line.PointAlong (beta)) / 2.0;
}
