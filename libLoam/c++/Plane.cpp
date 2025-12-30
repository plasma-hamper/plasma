
/* (c)  oblong industries */

#include "Plane.h"
#include "Line.h"


using namespace oblong::loam;


static const Vect zaxs (0.0, 0.0, 1.0);


Plane::Plane () : norm (zaxs) {}

Plane::Plane (const Vect &p, const Vect &normal) : point (p), norm (normal)
{
  norm.NormSelf ();
}



bool Plane::IsValid () const
{
  return (point.IsValid () && (fabs (norm.AutoDot () - 1.0) < 1e-7));
}


Plane Plane::Invalid ()
{
  Vect foul_pnt = Vect ().SetInvalid ();
  Vect foul_nrm;
  return Plane (foul_pnt, foul_nrm);
}


void Plane::Set (const Vect &pnt, const Vect &nrm)
{
  point = pnt;
  norm = nrm.Norm ();
}


float64 Plane::DistFrom (const Vect &p) const
{
  return (p - point).Dot (norm);
}


Vect Plane::Project (const Vect &p) const
{
  return p - norm * DistFrom (p);
}


Vect Plane::Intersect (const Line &line, float64 *pu) const
{
  // zero value implies parallel = no intersection
  float64 den = norm.Dot (line.Dir ());
  if (fabs (den) < 1e-11)
    {
      if (pu)
        *pu = OB_NAN;
      return Vect::Invalid ();
    }

  float64 num = norm.Dot (point - line.Point ());

  float64 u = num / den;
  if (pu)
    *pu = u;
  return line.PointAlong (u);
}


Line Plane::Intersect (const Plane &pln) const
{
  if (!IsValid () || !pln.IsValid ())
    return Line::Invalid ();

  Vect aim (norm.Cross (pln.norm));
  if (aim.AutoDot () < 1.0e-14)  // planes parallel, don't you know.
    return Line::Invalid ();

  aim.NormSelf ();

  Vect slide (norm.Cross (aim));  // in-plane direction toward line

  float64 t = (pln.point - point).Dot (pln.norm);  // numerator for now
  float64 denom = slide.Dot (pln.norm);
  t /= denom;  // now t is amount of slide...

  return Line (point + t * slide, aim);
}
