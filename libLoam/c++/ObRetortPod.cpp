
/* (c)  oblong industries */

#include <libLoam/c++/ObRetortPod.h>


using namespace oblong::loam;


ObRetortPod::ObRetortPod () : ObPseudopod (), c_ret (0) {}

ObRetortPod::ObRetortPod (ob_retort obr) : ObPseudopod (), c_ret (obr) {}

ObRetortPod::ObRetortPod (ob_retort obr, ObRetortPod *ant)
    : ObPseudopod (), c_ret (obr), antecedent (ant)
{
}


ObRetortPod::~ObRetortPod () {}


ob_retort ObRetortPod::NumericRetort () const
{
  return c_ret;
}

void ObRetortPod::SetNumericRetort (ob_retort num_ret)
{
  c_ret = num_ret;
}


ObRetortPod *ObRetortPod::Antecedent () const
{
  return ~antecedent;
}

void ObRetortPod::SetAntecedent (ObRetortPod *ant)
{
  antecedent = ant;
}


ObRetortPod *ObRetortPod::FirstRetortPodOfClass (const char *pod_cl) const
{
  for (const ObRetortPod *pod = this; pod; pod = pod->Antecedent ())
    if (pod->IsInclusiveDescendentOf (pod_cl))
      return const_cast<ObRetortPod *> (pod);  // lovely. just spectacular.
  return NULL;
}
