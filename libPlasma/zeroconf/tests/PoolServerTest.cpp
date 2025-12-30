
/* (c)  oblong industries */

#include <libPlasma/zeroconf/PoolServer.h>

#include <gtest/gtest.h>

using namespace oblong::plasma;

namespace {

class PoolServerTest : public ::testing::Test
{};

TEST_F (PoolServerTest, Defaults)
{
  PoolServer server ("", 0, NULL, NULL);
  EXPECT_STREQ ("localhost", server.Host ());
  EXPECT_STREQ ("", server.Name ());
  EXPECT_STREQ ("", server.Type ());
  EXPECT_EQ (0, server.Port ());
  EXPECT_STREQ ("tcp://localhost:0/", server.Address ());

  EXPECT_EQ (PoolServer (), PoolServer (NULL, 65535, NULL, NULL));
  EXPECT_EQ (PoolServer (), PoolServer ("", 65535, "", ""));
}

TEST_F (PoolServerTest, Constructors)
{
  PoolServer defaultServer;
  EXPECT_STREQ ("tcp://localhost:65535/", defaultServer.Address ());
  PoolServer server ("host", 69, "name", "type");
  EXPECT_NE (defaultServer, server);
  PoolServer server_copy (server);
  EXPECT_EQ (server, server_copy);
  EXPECT_STREQ ("host", server.Host ());
  EXPECT_STREQ ("name", server.Name ());
  EXPECT_STREQ ("type", server.Type ());
  EXPECT_EQ (69, server.Port ());
  EXPECT_STREQ ("tcp://host:69/", server.Address ());
  EXPECT_STREQ ("host", server_copy.Host ());
  EXPECT_STREQ ("name", server_copy.Name ());
  EXPECT_STREQ ("type", server_copy.Type ());
  EXPECT_EQ (69, server_copy.Port ());
}

}  // namespace
