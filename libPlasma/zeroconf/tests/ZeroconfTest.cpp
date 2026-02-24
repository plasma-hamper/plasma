
/* (c)  oblong industries */

#ifndef _MSC_VER

#include <libPlasma/zeroconf/Zeroconf.h>
#include <libPlasma/zeroconf/PoolServer.h>
#include <libPlasma/zeroconf/zeroconf-services.h>

#include <gtest/gtest.h>
#include <iostream>

#include <signal.h>
#include <string.h>
#include <stdlib.h>

// #define ZEROCONF_TEST_TRACE

namespace {

using namespace oblong::plasma;
using namespace oblong::loam;

// As suggested in bug 2019 comment 5 and bug 2020 comment 2, disable
// this test because it is unreliable, and no one with the knowledge,
// time, and/or desire to investigate it is currently available.
#define ZeroconfTest DISABLED_ZeroconfTest

class ZeroconfTest;

class MockServer
{
 public:
  explicit MockServer (int i) : server_ (MakeServer (i)), pid_ (-1) {}

  bool Matches (const PoolServer &ps) const
  {
    return server_ == PoolServer (NULL, ps.Port (), ps.Name (), ps.Type ());
  }

  bool Run ()
  {
    Exit ();
    pid_ = fork ();
    if (pid_ < 0)
      return false;
    if (pid_ > 0)
      return true;
    run (this);
    exit (0);
  }

  void Exit ()
  {
    if (pid_ > 0 && !kill (pid_, SIGKILL))
      {
        int status = 0;
        waitpid (pid_, &status, 0);
        pid_ = -1;
      }
  }

  void Announce ()
  {
    if (pid_ > 0)
      kill (pid_, SIGUSR1);
  }

  const char *Name () const { return server_.Name ().utf8 (); }
  const char *Type () const { return server_.Type ().utf8 (); }

  static void InstallHandlers ()
  {
    static struct sigaction action;
    action.sa_handler = &on_signal;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGUSR1, &action, NULL);
  }

 private:
  static void run (MockServer *me)
  {
    while (!announce_)
      usleep (10000);
    ZEROCONF_LOG_DEBUG ("MOCK: Announcing %s", me->Name ());
    zc_server_data *sd = zeroconf_server_announce (
      me->server_.Name (), ZEROCONF_SERVICE_TYPE, me->server_.Type (),
      me->server_.Port (), NULL, NULL);
    if (!sd)
      {
        ZEROCONF_LOG_ERROR ("Announcement failed for %s", me->Name ());
        abort ();
      }
    ZEROCONF_LOG_DEBUG ("MOCK: Announced %s", me->Name ());

    while (true)
      sleep (1000);
  }

  static void on_signal (int s) { announce_ = true; }

  static PoolServer MakeServer (int i)
  {
    static char host[256] = {0};
    if (!host[0])
      gethostname (host, 256);
    const int p = getpid ();
    char name[512];
    int namelen = snprintf (name, sizeof (name), "%d=)%d(>>=%s", p, i, host);
    if (namelen >= ZEROCONF_MAX_SERVICE_LEN)
      {
        ZEROCONF_LOG_ERROR ("service name %s too long", name);
        abort ();
      }
    char type[ZEROCONF_MAX_SUBTYPE_LEN + 1];
    snprintf (type, ZEROCONF_MAX_SUBTYPE_LEN, "%d_%d", p, i);
    return PoolServer (NULL, i + unt16 (p), name, type);
  }

  PoolServer server_;
  pid_t pid_;
  static bool announce_;
};

bool MockServer::announce_ = false;

class ZeroconfTest : public ::testing::Test
{
 public:
  enum
  {
    NO = 2
  };
  static bool check_server (const PoolServer &server, int no)
  {
    return servers[no]->Matches (server);
  }

 protected:
  class Tracer : public ZeroconfHandler
  {
   public:
    Tracer (const char *pref) : prefix_ (pref) {}
    void Add (const PoolServer &server) override
    {
      ZEROCONF_LOG_DEBUG ("%s%s", prefix_, server.Name ().utf8 ());
    }
    void Remove (const PoolServer &server) override
    {
      ZEROCONF_LOG_DEBUG ("%s%s", prefix_, server.Name ().utf8 ());
    }

   private:
    const char *const prefix_;
  };

  class CallCounter : public ZeroconfHandler
  {
   public:
    CallCounter (volatile int *flags, bool inc) : flags_ (flags), inc_ (inc) {}
    void Add (const PoolServer &server) override
    {
      ZEROCONF_LOG_DEBUG ("CALL_COUNTER: %s called with %s", inc_ ? "+" : "-",
                          server.Name ().utf8 ());
      for (int i = 0; i < ZeroconfTest::NO; ++i)
        if (ZeroconfTest::check_server (server, i))
          {
            if (inc_)
              flags_[i]++;
          }
    }
    void Remove (const PoolServer &server) override
    {
      ZEROCONF_LOG_DEBUG ("CALL_COUNTER: %s called with %s", inc_ ? "+" : "-",
                          server.Name ().utf8 ());
      for (int i = 0; i < ZeroconfTest::NO; ++i)
        if (ZeroconfTest::check_server (server, i))
          {
            if (!inc_)
              flags_[i]--;
          }
    }

   private:
    volatile int *flags_;
    const bool inc_;
  };

  static void update_added (const PoolServer &sp)
  {
    static CallCounter counter (added_flags, true);
    counter.Add (sp);
  }

  static void SetUpTestCase ()
  {
#ifdef ZEROCONF_TEST_TRACE
    zeroconf_enable_debug_messages ();
#endif
    MockServer::InstallHandlers ();
    for (int i = 0; i < NO; ++i)
      servers[i] = new MockServer (i);
    start_servers ();
#ifdef ZEROCONF_TEST_TRACE
    RegisterServerHandler (new Tracer ("(TRACER) ADD: "));
    RegisterServerHandler (new Tracer ("(TRACER) REMOVE"));
#endif
    RegisterServerHandler (MakeZeroconfHandler (update_added));
    zeroconf_enabled = ZeroconfStart ();
    if (!zeroconf_enabled)
      ZEROCONF_LOG_INFO ("Could not init browser: is daemon running%s", "?");
  }

  static void TearDownTestCase ()
  {
    ZeroconfStop ();
    kill_servers ();
    for (int i = 0; i < NO; ++i)
      delete servers[i];
  }

  static void check_type (int n)
  {
    PoolServers list = ZeroconfServers (servers[n]->Type ());
    ASSERT_LT (0, list.Count ()) << servers[n]->Type ();
    for (int32 i = 0, c = list.Count (); i < c; ++i)
      if (check_server (list.Nth (i), n))
        return;
    FAIL () << n << "th server not found";
  }

  static void start_servers ()
  {
    for (int i = 0; i < NO; ++i)
      EXPECT_TRUE (servers[i]->Run ());
    sleep (NO);
  }

  static void kill_servers ()
  {
    for (int i = 0; i < NO; ++i)
      servers[i]->Exit ();
  }

  static void make_announcements ()
  {
    for (int i = 0; i < NO; ++i)
      servers[i]->Announce ();
  }

  static bool zeroconf_enabled;
  static MockServer *servers[NO];
  static int added_flags[NO];
};

bool ZeroconfTest::zeroconf_enabled = true;
MockServer *ZeroconfTest::servers[NO] = {NULL, NULL};
int ZeroconfTest::added_flags[NO] = {0, 0};

TEST_F (ZeroconfTest, ServerList)
{
  if (!zeroconf_enabled)
    return;
  make_announcements ();
  sleep (1);
  int found[NO];
  int retries (100);
  while (retries--)
    {
      sleep (NO);
      PoolServers list = ZeroconfServers ();
      const int32 lc = list.Count ();
      for (int32 i = 0; i < NO; ++i)
        found[i] = 0;
      for (int32 n = 0; n < lc; ++n)
        {
          const PoolServer &s = list.Nth (n);
          for (int32 i = 0; i < NO; ++i)
            if (check_server (s, i))
              found[i]++;
        }
      bool good = true;
      for (int32 i = 0; good && i < NO; ++i)
        good = found[i] == 1;
      if (good)
        break;
    }
  for (unt32 i = 0; i < NO; ++i)
    EXPECT_EQ (1, found[i]) << i << "th";
}

TEST_F (ZeroconfTest, FilteredServerList)
{
  if (!zeroconf_enabled)
    return;
  for (unt32 i = 0; i < NO; ++i)
    {
      SCOPED_TRACE (servers[i]->Name ());
      check_type (i);
    }
}

TEST_F (ZeroconfTest, PoolServerADT)
{
  if (!zeroconf_enabled)
    return;
  PoolServers list = ZeroconfServers ();
  const int32 lc = list.Count ();
  ASSERT_TRUE (lc > 0);
  for (int32 i = 0; i < lc - 1; ++i)
    EXPECT_FALSE (list.Nth (i) == list.Nth (i + 1));
}

TEST_F (ZeroconfTest, Callbacks)
{
  if (!zeroconf_enabled)
    return;

  EXPECT_EQ (1, added_flags[0]);
  EXPECT_EQ (1, added_flags[1]);

  volatile int flags[NO] = {1, 1};

  ZeroconfHandler *on_remove = new CallCounter (flags, false);

  RegisterServerHandler (on_remove);

  servers[0]->Exit ();
  int retries (2000);
  while (retries-- && flags[0])
    usleep (10000);
  EXPECT_EQ (0, flags[0]);
  EXPECT_EQ (1, flags[1]);

  servers[1]->Exit ();
  retries = 2000;
  while (retries-- && flags[1])
    usleep (10000);
  EXPECT_EQ (0, flags[0]);
  EXPECT_EQ (0, flags[1]);
}

}  // namespace

#endif  // !_MSC_VER
