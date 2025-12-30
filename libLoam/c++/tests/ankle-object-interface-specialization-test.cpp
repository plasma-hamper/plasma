
/* (c)  oblong industries */

#include <gtest/gtest.h>

#include <stdio.h>

#include "libLoam/c/ob-retorts.h"

#include <libLoam/c++/ankle-object-interface-specialization.h>

#include <vector>


using namespace oblong::loam;


static std::vector<const char *> global_scratch;
static size_t expected_entries;

void init_scratch ()
{
  global_scratch.clear ();
  expected_entries = 0;
}
void ready_scratch (int expected = 1)
{
  global_scratch.clear ();
  expected_entries = expected;
}
void update_scratch (const char *note)
{
  if (global_scratch.size () == expected_entries)
    {
      fprintf (stderr,
               "calling our scratchpad function unexpectedly. "
               "(%s)\n",
               note);
      abort ();
    }

  global_scratch.push_back (note);
}

class intrfaithA
{
 public:
  virtual ob_retort MethA ()
  {
    update_scratch ("base-a");
    return OB_OK;
  }
  // The destructor below as added to avoid a warning on Red Hat's gcc
  // (4.1.2) of the form : class [C] has virtual functions but
  // non-virtual destructor.  With -Werror, this warning became an
  // error.
  virtual ~intrfaithA () {}
};

DECLARE_INTERFACE_AS_ANKLE_OBJECT_SPECIALIZED (intrfaithA);

class intrfaithB
{
 public:
  virtual ob_retort MethB ()
  {
    update_scratch ("base-b");
    return OB_OK;
  }
  // The destructor below as added to avoid a warning on Red Hat's gcc
  // (4.1.2) of the form : class [C] has virtual functions but
  // non-virtual destructor.  With -Werror, this warning became an
  // error.
  virtual ~intrfaithB () {}
};

DECLARE_INTERFACE_AS_ANKLE_OBJECT_SPECIALIZED (intrfaithB);


class HappilyA : public AnkleObject, public intrfaithA
{
 public:
  ~HappilyA () override { update_scratch ("~HappilyA"); }
  ob_retort MethA () override
  {
    update_scratch ("a-only");
    return OB_OK;
  }
};

class HappilyAandB : public AnkleObject, public intrfaithA, public intrfaithB
{
 public:
  ~HappilyAandB () override { update_scratch ("~HappilyAandB"); }
  ob_retort MethA () override
  {
    update_scratch ("anb--a");
    return OB_OK;
  }
  ob_retort MethB () override
  {
    update_scratch ("anb--b");
    return OB_OK;
  }
};

//

TEST (CtInterfaceSpecializations, BasicTest)
{
  init_scratch ();
  {
    ObRef<HappilyA *> a (new HappilyA ());
    ObRef<HappilyAandB *> anb (new HappilyAandB ());
    {
      std::vector<intrfaithA *> atro;
      atro.push_back (~a);
      atro.push_back (~anb);
      intrfaithA *a_mine = atro[0];
      ready_scratch (2);
      a_mine->MethA ();
      a_mine = atro[1];
      a_mine->MethA ();
      ASSERT_STREQ ("a-only", global_scratch[0]);
      ASSERT_STREQ ("anb--a", global_scratch[1]);
    }
    ready_scratch (2);
  }
  ASSERT_STREQ ("~HappilyAandB", global_scratch[0]);
  ASSERT_STREQ ("~HappilyA", global_scratch[1]);
}
