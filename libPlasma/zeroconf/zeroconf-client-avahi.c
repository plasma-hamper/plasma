
/* (c)  oblong industries */

#include "zeroconf-services.h"

#include "libLoam/c/ob-pthread.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

typedef struct browser_data
{
  AvahiSimplePoll *poll;
  AvahiClient *client;
  AvahiServiceBrowser *browser;
  add_callback on_add;
  del_callback on_del;
  flushed_callback on_flush;
  int32 unresolved;
  bool flushed;
  pthread_t thread;
} browser_data;

static void quit_browser (browser_data *bd)
{
  avahi_simple_poll_quit (bd->poll);
}

static void resolve_callback (AvahiServiceResolver *r, AvahiIfIndex interface,
                              AvahiProtocol protocol, AvahiResolverEvent event,
                              const char *name, const char *type,
                              const char *domain, const char *host_name,
                              const AvahiAddress *address, uint16_t port,
                              AvahiStringList *txt,
                              AvahiLookupResultFlags flags, void *userdata)
{
  assert (r);

  browser_data *bd = (browser_data *) userdata;

  ZEROCONF_LOG_DEBUG ("Service resolved: %s, %s, %s:%d", name, type, host_name,
                      port);

  switch (event)
    {
      case AVAHI_RESOLVER_FAILURE:
        {
          const char *err = avahi_strerror (
            avahi_client_errno (avahi_service_resolver_get_client (r)));
          ZEROCONF_LOG_DEBUG (
            "Failed to resolve service '%s' of type '%s' in domain '%s': %s\n",
            name, type, domain, err);
          break;
        }
      case AVAHI_RESOLVER_FOUND:
        if (bd->on_add)
          {
            if (!txt)
              bd->on_add (name, NULL, host_name, port);
            while (txt)
              {
                bd->on_add (name,
                            (const char *) avahi_string_list_get_text (txt),
                            host_name, port);
                txt = avahi_string_list_get_next (txt);
              }
          }
        break;
    }

  avahi_service_resolver_free (r);

  if (bd->unresolved-- < 2 && bd->flushed && bd->on_flush)
    {
      bd->on_flush ();
      bd->on_flush = NULL;
    }
}

static void browse_callback (AvahiServiceBrowser *b, AvahiIfIndex interface,
                             AvahiProtocol protocol, AvahiBrowserEvent event,
                             const char *name, const char *type,
                             const char *domain, AvahiLookupResultFlags flags,
                             void *userdata)
{
  assert (b);

  browser_data *bd = (browser_data *) userdata;

  switch (event)
    {
      case AVAHI_BROWSER_FAILURE:
        ZEROCONF_LOG_ERROR ("(Browser) %s",
                            avahi_strerror (avahi_client_errno (
                              avahi_service_browser_get_client (b))));
        quit_browser (bd);
        break;
      case AVAHI_BROWSER_NEW:
        ZEROCONF_LOG_DEBUG ("NEW: service '%s' of type '%s' in domain '%s'",
                            name, type, domain);
        bd->unresolved++;
        if (!(avahi_service_resolver_new (bd->client, interface, protocol, name,
                                          type, domain, AVAHI_PROTO_UNSPEC, 0,
                                          resolve_callback, bd)))
          {
            ZEROCONF_LOG_WARNING (
              "Failed to resolve service '%s': %s", name,
              avahi_strerror (avahi_client_errno (bd->client)));
            bd->unresolved--;
          }
        break;
      case AVAHI_BROWSER_REMOVE:
        ZEROCONF_LOG_DEBUG ("REMOVE: service '%s' of type '%s'", name, type);
        if (bd->on_del)
          bd->on_del (name, NULL);
        break;
      case AVAHI_BROWSER_ALL_FOR_NOW:
      case AVAHI_BROWSER_CACHE_EXHAUSTED:
        bd->flushed = true;
        if (bd->on_flush && bd->unresolved < 1)
          {
            bd->on_flush ();
            bd->on_flush = NULL;
          }
        ZEROCONF_LOG_DEBUG ("(Browser) %s",
                            event == AVAHI_BROWSER_CACHE_EXHAUSTED
                              ? "CACHE_EXHAUSTED"
                              : "ALL_FOR_NOW");
        break;
    }
}

static void client_callback (AvahiClient *c, AvahiClientState state,
                             void *userdata)
{
  assert (c);
  browser_data *bd = (browser_data *) userdata;
  if (state == AVAHI_CLIENT_FAILURE)
    {
      ZEROCONF_LOG_ERROR ("Server connection failure: %s",
                          avahi_strerror (avahi_client_errno (c)));
      quit_browser (bd);
    }
}

static bool init_avahi_data (browser_data *bd)
{
  bd->browser = NULL;
  bd->client = NULL;
  bd->poll = avahi_simple_poll_new ();
  if (!bd->poll)
    {
      ZEROCONF_LOG_ERROR ("Failed to create simple poll object.%s", "");
      return false;
    }
  int err = 0;
  bd->client = avahi_client_new (avahi_simple_poll_get (bd->poll), 0,
                                 client_callback, bd, &err);
  if (!bd->client)
    {
      ZEROCONF_LOG_ERROR ("Failed to create client: %s", avahi_strerror (err));
      return false;
    }

  bd->browser = avahi_service_browser_new (
    bd->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, ZEROCONF_SERVICE_TYPE,
    NULL, 0, browse_callback, bd);
  if (!bd->browser)
    {
      ZEROCONF_LOG_ERROR ("Failed to create service browser: %s",
                          avahi_strerror (avahi_client_errno (bd->client)));
      return false;
    }
  return true;
}

static void delete_browser_data (browser_data *bd)
{
  if (bd->browser)
    avahi_service_browser_free (bd->browser);
  if (bd->client)
    avahi_client_free (bd->client);
  if (bd->poll)
    avahi_simple_poll_free (bd->poll);
  free (bd);
}

static browser_data *make_browser_data (add_callback ac, del_callback dc,
                                        flushed_callback fc)
{
  browser_data *bd = (browser_data *) malloc (sizeof (browser_data));
  if (!init_avahi_data (bd))
    {
      delete_browser_data (bd);
      bd = NULL;
    }
  else
    {
      bd->on_add = ac;
      bd->on_del = dc;
      bd->on_flush = fc;
      bd->flushed = false;
      bd->unresolved = 0;
    }
  return bd;
}

void *browser_main (void *arg)
{
  browser_data *data = (browser_data *) arg;
  while (!avahi_simple_poll_iterate (data->poll, 1000))
    ;
  return NULL;
}

static browser_data *active_data_ = NULL;

bool zeroconf_browser_start (add_callback on_add, del_callback on_del,
                             flushed_callback on_end)
{
  if (active_data_)
    return true;

  active_data_ = make_browser_data (on_add, on_del, on_end);
  return active_data_ != NULL
         && pthread_create (&active_data_->thread, NULL, browser_main,
                            active_data_)
              == 0;
}

void zeroconf_browser_stop (void)
{
  if (active_data_)
    {
      quit_browser (active_data_);
      pthread_join (active_data_->thread, NULL);
      delete_browser_data (active_data_);
      active_data_ = NULL;
    }
}
