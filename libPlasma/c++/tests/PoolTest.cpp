
/* (c)  oblong industries */

#include "PoolTestBase.h"

#include "libLoam/c++/LoamStreams.h"
#include <libPlasma/c++/Hose.h>

namespace {

class PoolTest : public PoolTestBase
{};

TEST_F (PoolTest, Creation)
{
  // suppress message "Pool type ceci n'est pas a type not implemented"
  EXPECT_EQ (OB_OK, ob_suppress_message (OBLV_ERROR, 0x20101010));
  EXPECT_EQ (OB_OK, ob_suppress_message (OBLV_ERROR, 0x20104022));

  EXPECT_FALSE (Pool::Create (PNAME, Pool::MMAP_MEDIUM, true).IsSplend ());
  EXPECT_FALSE (Pool::Create (PNAME, Pool::MMAP_MEDIUM, false).IsSplend ());
  EXPECT_FALSE (Pool::Create (ENAME, Pool::MMAP_MEDIUM, true).IsSplend ());
  EXPECT_FALSE (Pool::Create (ENAME, Pool::MMAP_MEDIUM, false).IsSplend ());

  ::std::string name (PoolName ("foo"));
  const char *nc = name.c_str ();

  EXPECT_FALSE (
    Pool::Create (nc, "ceci n'est pas a type", true, Slaw ()).IsSplend ());
  EXPECT_FALSE (
    Pool::Create (nc, "ceci n'est pas a type", false, Slaw ()).IsSplend ());
}

TEST_F (PoolTest, Participate)
{
  ObRetort ret = OB_UNKNOWN_ERR;
  typedef ObRef<Hose *> HRef;
  HRef h = Pool::Participate (ENAME, &ret);
  EXPECT_TRUE ((~h)->LastRetort ().IsSplend ());
  EXPECT_TRUE (ret == (~h)->LastRetort ());
  ret = OB_UNKNOWN_ERR;
  h = Pool::Participate (PNAME, &ret);
  EXPECT_TRUE ((~h)->LastRetort ().IsSplend ());
  EXPECT_TRUE (ret == (~h)->LastRetort ());

  ::std::string newp (PoolName ("oh-my-pool!"));
  EXPECT_TRUE (Pool::Participate (newp.c_str ()) == NULL);

  ret = OB_UNKNOWN_ERR;
  h = Pool::Participate (newp.c_str (), Pool::MMAP_SMALL, &ret);
  EXPECT_TRUE ((~h)->LastRetort () == OB_OK);
  EXPECT_TRUE (ret == POOL_CREATED);

  h->Withdraw ();
  pool_dispose (newp.c_str ());
}

}  // namespace
