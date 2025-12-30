
/* (c)  oblong industries */

#include "zeroconf-services.h"

#include <signal.h>

#include <libLoam/c++/Str.h>
#include <libLoam/c++/ArgParse.h>
#include <libPlasma/c++/Plasma.h>

using namespace oblong::plasma;

const int32 Default_Port = 65456;
const Str Default_Pool_Server =
  Str::Format ("tcp://localhost:%d/", Default_Port);
const int64 Default_Poll_Interval = 5;


// Global flag.  This is set ONLY In the signal handlers below
static volatile sig_atomic_t continueMonitoring = 1;

// Signal handling (for clean shutdown)
//
void HandleSigquit (int sig)
{
  continueMonitoring = 0;
}


void HandleSigintOrTerm (int sig)
{
  continueMonitoring = 0;
}


#ifdef _MSC_VER
BOOL HandleSigwin (DWORD fdwCtrlType)
{
  continueMonitoring = 0;
  return TRUE;
}
#endif


void InstallCiaoSignalHandlers ()
{
#ifdef _MSC_VER
  SetConsoleCtrlHandler ((PHANDLER_ROUTINE) HandleSigwin, TRUE);
#else
  // Signal handlers for SIGTERM, SIGINT
  struct sigaction action;
  OB_CLEAR (action);
  action.sa_handler = HandleSigintOrTerm;
  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  sigaction (SIGHUP, &action, NULL);
  OB_CLEAR (action);
  action.sa_handler = HandleSigquit;
  sigaction (SIGQUIT, &action, NULL);
#endif
}


//  Turns foo.oblong.com into foo
static Str MinimalHostName ()
{
  char tmp[512];
  gethostname (tmp, 512);
  Str fullName (tmp);
  ObTrove<Str> parts = fullName.Split ("\\.");
  if (parts.Count () > 1)
    return parts.Nth (0);
  else
    return fullName;
}


//  Grabs port, but does not verify its validity.
//  Ports *should* be between 0 and 65536 (noninclusive)
static int32 ExtractPort (const Str &addr)
{
  Str a (addr);
  if (a.Match ("\\w+:(\\d+)/?\\z"))
    {
      Str portStr = a.MatchNthCaptureSlice (1);
      int32 port = atoi (portStr);
      return port;
    }
  return -1;
}


//  Check and normalize address of a pool server.
//  Returns empty string if address is not valid.
static Str CheckAndNormalizeAddress (const Str &addr)
{
  Str a (addr);
  a.Strip ();
  a.Chomp ("/");

  //  Must start with tcp://, have at least 3 characters' worth of hostname,
  //  some set of word chars and dots, and maybe a port at the end
  if (!a.Match ("^tcp://\\w{3,}[\\.\\w]*(:\\d+)?\\z"))
    return "";

  int32 port = ExtractPort (a);

  //  No port: we'll assign the default
  if (port == -1)
    port = Default_Port;
  //  Invalid port
  else if (port <= 0 || port >= 65536)
    return "";

  //  Get rid of the existing port part of the string so we can rewrite it all
  a.ReplaceAll (":\\d+\\z", "");

  return Str::Format ("%s:%d/", a.utf8 (), port);
}


//  Assumes that the host name supplied is valid and wellformed!
//  To check this, call CheckAndNormalizeAddress
static bool ConnectToPoolServer (const Str &host, bool report = false)
{
  slaw listOfPools = NULL;
  ob_retort ret = pool_list_remote (host, &listOfPools);
  if (ret == OB_OK)
    {
      Slaw names (listOfPools);  // takes responsibility for freeing little slaw
      if (report)
        {
          ZEROCONF_LOG_DEBUG ("Found %d pools at poolserver %s",
                              (int) names.Count (), host.utf8 ());
        }
    }
  else if (report)
    ZEROCONF_LOG_DEBUG ("Could not connect to pool server %s:\n   %s",
                        host.utf8 (), ob_error_string_literal (ret));
  return (ret == OB_OK);
}


//  Expects a pre-validated & checked service name
//
static zc_server_data *AnnounceServer (const Str &service_name,
                                       const Str &subtypes, int32 port)
{
  return zeroconf_server_announce (service_name, ZEROCONF_SERVICE_TYPE,
                                   subtypes, port, NULL, NULL);
}


//  Runs forever until the continueMonitoring flag is set to 0.
//  This only happens in signal handlers up top.
static void MonitoringLoop (const Str &poolserver, int32 seconds)
{
  while (continueMonitoring)  // this is a global
    {
      if (!ConnectToPoolServer (poolserver, false))
        ZEROCONF_LOG_INFO ("No poolserver detected at %s", poolserver.utf8 ());
      else
        ZEROCONF_LOG_DEBUG ("Poolserver found at %s :: Announcing.",
                            poolserver.utf8 ());

      sleep (seconds / 2);
      //  Check for termination signals, halfway thru our sleep (to be responsive)
      if (!continueMonitoring)
        break;
      sleep (seconds / 2);
    }
}


//  Spawn a child process (or don't)
//
static void ForkOff (bool do_fork)
{
  int pid = do_fork ? fork () : 0;
  if (pid < 0)
    {
      ZEROCONF_LOG_WARNING ("ERROR: Could not fork process (%d)", pid);
      exit (EXIT_FAILURE);
    }
  if (pid > 0)
    exit (EXIT_SUCCESS);

  //  If pid == 0, we're the child and we keep on trucking
}


int main (int argc, char **argv)
{
#ifdef _MSC_VER
  //initialize windows sockets
  winsock_init ();
#endif

  Str poolServerAddr (Default_Pool_Server);
  int64 pollInterval = Default_Poll_Interval;
  Str nameOfService = MinimalHostName ();
  Str subTypes;
  bool noFork = false;
  bool verbose = false;
  bool printHelp = false;

  ArgParse ap;
  ap.DontAllowUnrecognizedOptions ();
  ap.AllowOneCharOptionsToBeCombined ();
  ap.UsageHeader ("pool-server-zeroconf-adapter:  For announcing an instance "
                  "of pool_tcp_server ");
  ap.UsageHeader (
    "                               using the Zeroconf (Bonjour) protocol\n");
  ap.UsageHeader (
    "Usage: pool-server-zeroconf-adapter [options] [pool server]");
  ap.UsageHeader ("\n  [pool server]          Pool server to announce (default "
                  + Default_Pool_Server + ")");
  ap.ArgInt ("interval", " <secs>\aSeconds between liveness checks ",
             &pollInterval, true);
  ap.ArgString ("zname", " <name>\aName to announce for Zeroconf service ",
                &nameOfService, true);
  ap.ArgString ("subtypes",
                " <foo,bar,...>\aList of Zeroconf subtypes, "
                "comma-separated only ",
                &subTypes);
#ifndef _MSC_VER
  ap.ArgFlag ("nofork",
              "\aRun this process in the foreground (daemon by default)",
              &noFork, false);
  ap.Alias ("nofork", "n");
#else
  noFork =
    true;  // The Windows poolserver-zeroconf-adapter does not fork-off child processes.
#endif
  ap.ArgFlag ("verbose", "\aVerbose output", &verbose);
  ap.ArgFlag ("help", "\aPrint this help, then exit", &printHelp);

  ap.Alias ("interval", "u");
  ap.Alias ("zname", "z");
  ap.Alias ("subtypes", "t");
  ap.Alias ("verbose", "v");
  ap.Alias ("help", "h");
  bool parsed = ap.Parse (argc - 1, argv + 1);

  if (printHelp || !parsed)
    ap.ErrorExit ("");

  if (verbose)
    zeroconf_enable_debug_messages ();  // todo: seems backward or broken

  //  Validate user input
  //
  if (pollInterval <= 0)
    pollInterval = Default_Poll_Interval;

  if (nameOfService.Length () >= ZEROCONF_MAX_SERVICE_LEN)
    nameOfService = nameOfService.Slice (0, ZEROCONF_MAX_SERVICE_LEN - 1);

  int numLeftovers = ap.Leftovers ().Count ();
  if (numLeftovers > 0)
    {
      Str addressGiven = ap.Leftovers ().Nth (0);
      poolServerAddr = CheckAndNormalizeAddress (addressGiven);
      if (poolServerAddr.IsEmpty ())
        {
          ZEROCONF_LOG_WARNING ("ERROR: Invalid server address: %s",
                                addressGiven.utf8 ());
          exit (EXIT_FAILURE);
        }

      ZEROCONF_LOG_DEBUG ("User selected a server to announce: %s",
                          poolServerAddr.utf8 ());
    }


  //  Fork a child process, and start announcing
  //
  ForkOff (!noFork);
  InstallCiaoSignalHandlers ();

  if (ConnectToPoolServer (poolServerAddr, true))
    ZEROCONF_LOG_INFO ("Poolserver found at %s :: Announcing.  (my pid: %d)",
                       poolServerAddr.utf8 (), getpid ());
  else
    ZEROCONF_LOG_INFO ("No poolserver found at %s :: Monitoring.  (my pid: %d)",
                       poolServerAddr.utf8 (), getpid ());

  if (zc_server_data *serverdata =
        AnnounceServer (nameOfService, subTypes, ExtractPort (poolServerAddr)))
    {
      MonitoringLoop (poolServerAddr, pollInterval);
      zeroconf_server_shutdown (serverdata);
      ZEROCONF_LOG_INFO (
        "pool-server-zeroconf-adapter finished announcing poolserver %s",
        poolServerAddr.utf8 ());
    }
  else
    ZEROCONF_LOG_WARNING ("ERROR: Could not start the Zeroconf announcer.%s",
                          "");

#ifdef _MSC_VER
  //uninit windows sockets
  winsock_shutdown ();
#endif

  return EXIT_SUCCESS;
}
