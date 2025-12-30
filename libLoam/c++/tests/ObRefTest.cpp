
/* (c)  oblong industries */

#include <gtest/gtest.h>
#include "libLoam/c++/ObRef.h"
#include "libLoam/c++/AnkleObject.h"

using namespace oblong::loam;

TEST (EmptyObRefAssignmentTest, ObRef)
{
  ObRef<AnkleObject *> strongling, another_strongling;
  strongling = another_strongling;
  EXPECT_EQ (~strongling, nullptr);
}

TEST (EmptyWeakRefAssignmentTest, ObRef)  // bug4288
{
  ObWeakRef<AnkleObject *> weakling, another_weakling;
  weakling = another_weakling;
  EXPECT_EQ (~weakling, nullptr);
}

TEST (EmptyStrongRefAssignmentToWeakTest, ObRef)
{
  ObRef<AnkleObject *> ref;
  ObWeakRef<AnkleObject *> weak_ref{ref};
  EXPECT_EQ (~weak_ref, nullptr);
}

TEST (NoArgCtorTest, ObRef)
{
  ObRef<AnkleObject *> ao_ref;
  EXPECT_EQ (~ao_ref, nullptr);
}

TEST (PointerAnkleArgCtorTest, ObRef)
{
  AnkleObject *ao = new AnkleObject;
  ASSERT_TRUE (ao);

  ObRef<AnkleObject *> ao_ref{ao};
  ASSERT_TRUE (~ao_ref);
  EXPECT_EQ (ao, ~ao_ref);
}

static unt32 count_von_ankle = 0u;

struct CountVonAnkle : public AnkleObject
{
  int32 id;

  CountVonAnkle () : AnkleObject (), id (++count_von_ankle) {}

  ~CountVonAnkle () override { --count_von_ankle; }

  static void ResetCount () { count_von_ankle = 0u; }
};

// CountVonAnkle tests make sure the ObRef with AO-subclasses exercise
// all the various constructors and assignments.
TEST (PointerToSubAnkleArgCtorTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  CountVonAnkle *von_ankle = new CountVonAnkle;
  ASSERT_TRUE (von_ankle);
  ASSERT_EQ (count_von_ankle, 1);

  ObRef<AnkleObject *> ao_ref{von_ankle};
  ASSERT_EQ (von_ankle, ~ao_ref);
  EXPECT_EQ (von_ankle->id, count_von_ankle);
  ao_ref = nullptr;
  EXPECT_EQ (0, count_von_ankle);
}

TEST (PointerToSubAnkleCopyishCtorTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  ObRef<CountVonAnkle *> von_ankle_ref = new CountVonAnkle;
  ASSERT_TRUE (~von_ankle_ref);
  ASSERT_EQ (count_von_ankle, 1);

  ObRef<AnkleObject *> ao_ref{von_ankle_ref};
  ASSERT_EQ (~von_ankle_ref, ~ao_ref);
  ASSERT_EQ (count_von_ankle, 1);
  EXPECT_EQ (von_ankle_ref->id, count_von_ankle);
  von_ankle_ref = nullptr;
  EXPECT_EQ (1, count_von_ankle);
}

TEST (PointerToSubAnkleCopyCtorTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  ObRef<CountVonAnkle *> von_ankle_ref = new CountVonAnkle;
  ASSERT_TRUE (~von_ankle_ref);
  ASSERT_EQ (count_von_ankle, 1);

  ObRef<CountVonAnkle *> von_ankle_copy_ref{von_ankle_ref};
  ASSERT_EQ (~von_ankle_ref, ~von_ankle_copy_ref);
  ASSERT_EQ (count_von_ankle, 1);
  EXPECT_EQ (von_ankle_ref->id, count_von_ankle);
  von_ankle_ref = nullptr;
  EXPECT_EQ (1, count_von_ankle);
}

TEST (PointerToSubAnkleMoveArgCtorTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  ObRef<CountVonAnkle *> von_ankle_ref = new CountVonAnkle;
  ASSERT_TRUE (~von_ankle_ref);
  ASSERT_EQ (count_von_ankle, 1);

  ObRef<CountVonAnkle *> von_ankle_move_ref{std::move (von_ankle_ref)};
  ASSERT_EQ (~von_ankle_ref, nullptr);
  ASSERT_NE (nullptr, ~von_ankle_move_ref);
  ASSERT_EQ (count_von_ankle, 1);

  EXPECT_EQ (von_ankle_move_ref->id, count_von_ankle);
  von_ankle_move_ref = nullptr;
  EXPECT_EQ (0, count_von_ankle);
  von_ankle_ref = nullptr;
  EXPECT_EQ (0, count_von_ankle);
}

TEST (PointerToSubAnkleWeakRefShillTest, ObWeakRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  CountVonAnkle *von_ankle = new CountVonAnkle;
  ObWeakRef<CountVonAnkle *> von_ankle_weak_ref{von_ankle};
  ASSERT_TRUE (~von_ankle_weak_ref);
  ASSERT_EQ (count_von_ankle, 1);

  ObRef<CountVonAnkle *> von_ankle_ref{von_ankle};
  EXPECT_TRUE (~von_ankle_ref);

  von_ankle_ref.Nullify ();
  EXPECT_EQ (~von_ankle_ref, nullptr);
  EXPECT_EQ (count_von_ankle, 0);

  EXPECT_EQ (~von_ankle_weak_ref, nullptr);
}

TEST (PointerToSubAnkleAssignTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  ObRef<CountVonAnkle *> von_ankle_ref1{new CountVonAnkle};
  ASSERT_TRUE (~von_ankle_ref1);
  EXPECT_EQ (count_von_ankle, 1);
  ObRef<CountVonAnkle *> von_ankle_ref2{new CountVonAnkle};
  ASSERT_TRUE (~von_ankle_ref2);
  EXPECT_EQ (count_von_ankle, 2);

  von_ankle_ref2 = von_ankle_ref1;
  EXPECT_EQ (~von_ankle_ref1, ~von_ankle_ref2);
  EXPECT_EQ (1, count_von_ankle);
}

TEST (PointerToSubAnkleAssignSubTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  ObRef<CountVonAnkle *> von_ankle_ref1{new CountVonAnkle};
  ASSERT_TRUE (~von_ankle_ref1);
  EXPECT_EQ (count_von_ankle, 1);
  ObRef<AnkleObject *> von_ankle_ref2{new CountVonAnkle};
  ASSERT_TRUE (~von_ankle_ref2);
  EXPECT_EQ (count_von_ankle, 2);

  von_ankle_ref2 = von_ankle_ref1;
  EXPECT_EQ (~von_ankle_ref1, ~von_ankle_ref2);
  EXPECT_EQ (1, count_von_ankle);
}

TEST (PointerToSubAnkleMoveAssignTest, ObRef)
{
  CountVonAnkle::ResetCount ();
  ASSERT_EQ (count_von_ankle, 0);

  ObRef<CountVonAnkle *> von_ankle_ref1{new CountVonAnkle};
  ASSERT_TRUE (~von_ankle_ref1);
  EXPECT_EQ (count_von_ankle, 1);
  ObRef<CountVonAnkle *> von_ankle_ref2{new CountVonAnkle};
  ASSERT_TRUE (~von_ankle_ref2);
  EXPECT_EQ (count_von_ankle, 2);

  CountVonAnkle *von_ankle_1 = ~von_ankle_ref1;
  von_ankle_ref2 = std::move (von_ankle_ref1);
  EXPECT_EQ (~von_ankle_ref1, nullptr);
  EXPECT_EQ (von_ankle_1, ~von_ankle_ref2);
  EXPECT_EQ (1, count_von_ankle);
}


// This doesn't compile. Should I add a ctor to obref or a method to
// ObWeakRef, so that a programmer can create an ObRef from a WeakRef?
// From the looks of it, I'm not sure that ObWeakRef has enough
// information to create an ObRef. WeakRef currently holds
// GripMinder::WeakGripMinder ptr and T ptr (the thing itself). This
// is enough to recreate an ObRef for AnkleObject -- AO holds its
// GripMinder internally -- but the non-AO case doesn't have access to
// its GripMinder. The simplest thing to do in this case is to modify
// ObWeakRef so AO-subclasses can do this easily without bare
// pointers, then consider changing the API of
// GripMinder::WeakGripMinder so that it can get a hold of the
// (strong)GripMinder.

// TEST (PointerToSubAnkleWeakRefShillTestDos, ObWeakRef)
// {
//   CountVonAnkle::ResetCount();
//   ASSERT_EQ (count_von_ankle, 0);

//   CountVonAnkle *von_ankle = new CountVonAnkle;
//   ObWeakRef<CountVonAnkle *> von_ankle_weak_ref {von_ankle};
//   ASSERT_TRUE (~von_ankle_weak_ref);
//   ASSERT_EQ (count_von_ankle, 1);

//   ObRef<CountVonAnkle *> von_ankle_ref {von_ankle_weak_ref};
//   EXPECT_TRUE (~von_ankle_ref);

//   von_ankle_ref.Nullify();
//   EXPECT_EQ (~von_ankle_ref, nullptr);
//   EXPECT_EQ (count_von_ankle, 0);

//   EXPECT_EQ (~von_ankle_weak_ref, nullptr);
// }

static unt32 count_von_count = 0u;

struct CountVonCount
{
  int32 id;

  CountVonCount () : id (++count_von_count) {}

  ~CountVonCount () { --count_von_count; }

  static void ResetCount () { count_von_count = 0; }
};

// The story of Count von Count and Count von Ankle is an acrimonious
// one. Count von Ankle, the eldest, inherited their father's title,
// but Count von Count married into title and land. The brothers had
// always been competitive in sport, but getting shafted on the
// inheritance was the last straw for von Count. The brothers have not
// spoken since the day their father's will was executed in the
// solicitor's office.


// CountVonCount tests make sure to exercise all the various ObRef
// ctors and assignments for non-AnkleObjects.
TEST (PointerToNonAnkleArgCtorTest, ObRef)
{
  CountVonCount::ResetCount ();
  ASSERT_EQ (count_von_count, 0);

  CountVonCount *von_count = new CountVonCount;
  ASSERT_TRUE (von_count);
  ASSERT_EQ (count_von_count, 1);

  ObRef<CountVonCount *> von_count_ref{von_count};
  ASSERT_EQ (von_count, ~von_count_ref);
  EXPECT_EQ (von_count->id, count_von_count);
  von_count_ref = nullptr;
  EXPECT_EQ (0, count_von_count);
}

TEST (PointerToNonAnkleCopyCtorTest, ObRef)
{
  CountVonCount::ResetCount ();
  ASSERT_EQ (count_von_count, 0);

  ObRef<CountVonCount *> von_count_ref = new CountVonCount;
  ASSERT_TRUE (~von_count_ref);
  ASSERT_EQ (count_von_count, 1);

  ObRef<CountVonCount *> von_count_copy_ref{von_count_ref};
  ASSERT_EQ (~von_count_ref, ~von_count_copy_ref);
  ASSERT_EQ (count_von_count, 1);
  EXPECT_EQ (von_count_ref->id, count_von_count);
  von_count_ref = nullptr;
  EXPECT_EQ (1, count_von_count);
}

TEST (PointerToNonAnkleMoveArgCtorTest, ObRef)
{
  CountVonCount::ResetCount ();
  ASSERT_EQ (count_von_count, 0);

  ObRef<CountVonCount *> von_count_ref = new CountVonCount;
  ASSERT_TRUE (~von_count_ref);
  ASSERT_EQ (count_von_count, 1);

  ObRef<CountVonCount *> von_count_move_ref{std::move (von_count_ref)};
  ASSERT_EQ (~von_count_ref, nullptr);
  ASSERT_NE (nullptr, ~von_count_move_ref);
  ASSERT_EQ (count_von_count, 1);

  EXPECT_EQ (von_count_move_ref->id, count_von_count);
  von_count_move_ref = nullptr;
  EXPECT_EQ (0, count_von_count);
  von_count_ref = nullptr;
  EXPECT_EQ (0, count_von_count);
}

TEST (PointerToNonAnkleAssignTest, ObRef)
{
  CountVonCount::ResetCount ();
  ObRef<CountVonCount *> von_count_ref1{new CountVonCount};
  ASSERT_TRUE (~von_count_ref1);
  EXPECT_EQ (count_von_count, 1);
  ObRef<CountVonCount *> von_count_ref2{new CountVonCount};
  ASSERT_TRUE (~von_count_ref2);
  EXPECT_EQ (count_von_count, 2);

  von_count_ref2 = von_count_ref1;
  EXPECT_EQ (~von_count_ref1, ~von_count_ref2);
  EXPECT_EQ (1, count_von_count);
}

struct CountVonCountVonCount : public CountVonCount
{};

TEST (PointerToNonAnkleAssignSubTest, ObRef)
{
  CountVonCount::ResetCount ();
  ObRef<CountVonCountVonCount *> von_count_ref1{new CountVonCountVonCount};
  ASSERT_TRUE (~von_count_ref1);
  EXPECT_EQ (count_von_count, 1);
  ObRef<CountVonCount *> von_count_ref2{new CountVonCount};
  ASSERT_TRUE (~von_count_ref2);
  EXPECT_EQ (count_von_count, 2);

  von_count_ref2 = von_count_ref1;
  EXPECT_EQ (~von_count_ref1, ~von_count_ref2);
  EXPECT_EQ (1, count_von_count);
}

TEST (PointerToNonAnkleMoveAssignTest, ObRef)
{
  CountVonCount::ResetCount ();
  ObRef<CountVonCount *> von_count_ref1{new CountVonCount};
  ASSERT_TRUE (~von_count_ref1);
  EXPECT_EQ (count_von_count, 1);
  ObRef<CountVonCount *> von_count_ref2{new CountVonCount};
  ASSERT_TRUE (~von_count_ref2);
  EXPECT_EQ (count_von_count, 2);

  CountVonCount *von_count_1 = ~von_count_ref1;
  von_count_ref2 = std::move (von_count_ref1);
  EXPECT_EQ (~von_count_ref1, nullptr);
  EXPECT_EQ (von_count_1, ~von_count_ref2);
  EXPECT_EQ (1, count_von_count);
}


struct Subtalar : public AnkleObject
{};

TEST (EmptyObRefSubTypeAssignmentTest, ObRef)
{
  ObRef<AnkleObject *> strongling;
  ObRef<Subtalar *> another_strongling;
  strongling = another_strongling;
  EXPECT_EQ (~strongling, nullptr);
}

TEST (EmptyWeakRefSubTypeAssignmentTest, ObRef)
{
  ObWeakRef<AnkleObject *> weakling;
  ObWeakRef<Subtalar *> another_weakling;
  weakling = another_weakling;
  EXPECT_EQ (~weakling, nullptr);
}

TEST (EmptyStrongRefSubTypeWeakAssignmentTest, ObRef)
{
  ObWeakRef<AnkleObject *> weakling;
  ObRef<Subtalar *> strongling;
  weakling = strongling;
  EXPECT_EQ (~weakling, nullptr);
}


TEST (NonEmptyWeakRefAssignmentTest, ObRef)
{
  AnkleObject *da_obj = new AnkleObject ();
  ObWeakRef<AnkleObject *> weakling;
  weakling = da_obj;
  EXPECT_EQ (~weakling, da_obj);
  ObWeakRef<AnkleObject *> another_weakling;
  weakling = another_weakling;
  EXPECT_EQ (~weakling, nullptr);
  da_obj->Delete ();
}

TEST (NonEmptyWeakRefAssignmentTestForSubType, ObRef)
{
  AnkleObject *da_obj = new AnkleObject ();
  ObWeakRef<AnkleObject *> weakling;
  weakling = da_obj;
  EXPECT_EQ (~weakling, da_obj);
  ObWeakRef<Subtalar *> another_weakling;
  weakling = another_weakling;
  EXPECT_EQ (~weakling, nullptr);
  da_obj->Delete ();
}

TEST (NonEmptyObRefAssignmentTest, ObRef)
{
  AnkleObject *da_obj = new AnkleObject ();
  ObRef<AnkleObject *> strongling;
  strongling = da_obj;
  EXPECT_EQ (~strongling, da_obj);
  ObRef<AnkleObject *> another_strongling;
  strongling = another_strongling;
  EXPECT_EQ (~strongling, nullptr);
}

TEST (ObRefCopyConstructorTest, ObRef)
{
  AnkleObject *da_obj = new AnkleObject ();
  ObRef<AnkleObject *> strongling = da_obj;
  ObRef<AnkleObject *> another_strongling (strongling);
  EXPECT_EQ (~strongling, da_obj);
  EXPECT_EQ (~another_strongling, da_obj);
}

TEST (ObRefNullifyTest, ObRef)
{
  Subtalar *ess = new Subtalar ();
  ObRef<Subtalar *> s1 (ess);
  ObRef<Subtalar *> s2 (ess);
  ObWeakRef<Subtalar *> s3 (ess);
  s1.Nullify ();
  s2.Nullify ();
  EXPECT_EQ (nullptr, (~s3));
}

TEST (ObWeakRefCopyConstructorTest, ObRef)
{
  AnkleObject *da_obj = new AnkleObject ();
  ObWeakRef<AnkleObject *> weakling = da_obj;
  ObWeakRef<AnkleObject *> another_weakling (weakling);
  EXPECT_EQ (~weakling, da_obj);
  EXPECT_EQ (~another_weakling, da_obj);
  da_obj->Delete ();
}


TEST (ObWeakRefNullAssignmentOughtNotToCrashTest, ObRef)
{
  //ObWeakRef <float *> fl_weaky (nullptr);
  ObWeakRef<AnkleObject *> ao_weaky (nullptr);
}

TEST (ObWeakRefRawPointerAccessTest, ObRef)
{
  AnkleObject *anko = new AnkleObject;
  ObWeakRef<AnkleObject *> weakling (anko);
  EXPECT_EQ (~weakling, anko);
  EXPECT_EQ (weakling.RawPointer (), anko);
  {
    ObRef<AnkleObject *> strongling (anko);
    EXPECT_EQ (~strongling, anko);
    EXPECT_EQ (strongling.RawPointer (), anko);
    EXPECT_EQ (~weakling, anko);
    EXPECT_EQ (weakling.RawPointer (), anko);
  }
  EXPECT_EQ (~weakling, nullptr);
  EXPECT_EQ (weakling.RawPointer (), anko);
}

struct NonAnkleThing
{
  int foo () { return 42; }
};

TEST (ArrowOperatorTest, ObRef)
{
  AnkleObject *ao = new AnkleObject ();
  ObRef<AnkleObject *> ref (ao);
  EXPECT_EQ (ao->ClassName (), ref->ClassName ());

  NonAnkleThing *na = new NonAnkleThing ();
  ObRef<NonAnkleThing *> ref2 (na);
  EXPECT_EQ (na->foo (), ref2->foo ());
}

TEST (ObRefNullifyTestNonAnkle, ObRef)
{
  NonAnkleThing *en = new NonAnkleThing ();
  ObRef<NonAnkleThing *> n1 (en);
  ObRef<NonAnkleThing *> n2 (n1);
  ObWeakRef<NonAnkleThing *> n3 (n2);
  n1.Nullify ();
  n2.Nullify ();
  EXPECT_EQ (nullptr, (~n3));
}

class AnkleForwardedTestOne;

struct NonAnkleObjectTestOne
{
  // NonAnkleObject ();
  NonAnkleObjectTestOne (AnkleForwardedTestOne *ank) : obj (ank) {}

  ObRef<AnkleForwardedTestOne *> obj;
};

//Full definition of AnkleForwardedTestOne.
class AnkleForwardedTestOne : public AnkleObject
{
  PATELLA_SUBCLASS (AnkleForwardedTestOne, AnkleObject);
};

TEST (ObRefForwardTestOne, ObRef)
{
  // note from bmt: commenting out the above AnkleForwardedTestOne
  // definition should cause compilation to fail.
  NonAnkleObjectTestOne obj{nullptr};
  (void) obj;
}

class AnkleForwardedTestTwo;

struct NonAnkleObjectTestTwo
{
  NonAnkleObjectTestTwo ();
  NonAnkleObjectTestTwo (AnkleForwardedTestTwo *ank) : obj (ank) {}

  ObRef<AnkleForwardedTestTwo *> obj;
};


//Full definition of AnkleForwarded.
class AnkleForwardedTestTwo : public AnkleObject
{
  PATELLA_SUBCLASS (AnkleForwardedTestTwo, AnkleObject);

 public:
  AnkleForwardedTestTwo ();
  ~AnkleForwardedTestTwo ();
};

//Instantiate AnkleForwarded.
NonAnkleObjectTestTwo::NonAnkleObjectTestTwo ()
    : obj (new AnkleForwardedTestTwo ())
{
}

TEST (ObRefForwardTestTwo, ObRef)
{
  NonAnkleObjectTestTwo obj;
  // const bool is_base = std::is_base_of<AnkleObject, AnkleForwardedTestTwo>::value;
  // EXPECT_TRUE (is_base);

  GripMinder *obj_minder = obj.obj->grip_mnd;
  ASSERT_NE (obj_minder, nullptr);
  {
    NonAnkleObjectTestTwo second (~obj.obj);
    GripMinder *second_minder = second.obj->grip_mnd;
    EXPECT_EQ (obj_minder, second_minder);
  }

  EXPECT_EQ (obj.obj->grip_mnd, obj_minder);
}

AnkleForwardedTestTwo::AnkleForwardedTestTwo () {}

AnkleForwardedTestTwo::~AnkleForwardedTestTwo () {}


//struct AnkleSubclass : public virtual AnkleObject
//{ AnkleSubclass () { }
//  virtual ~AnkleSubclass () { }
//};

//TEST(Bug4969, ObRef)
//{ AnkleSubclass *rawptr = new AnkleSubclass ();
//  { ObRef <AnkleObject *> obj = rawptr; }
//}
