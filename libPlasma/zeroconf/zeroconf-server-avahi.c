
/* (c)  oblong industries */

#include "zeroconf-services.h"
#include "libLoam/c/ob-pthread.h"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include <unistd.h>
#include <stdio.h>

struct zc_server_data
{
  AvahiEntryGroup *group;
  AvahiThreadedPoll *poll;
  char *name;
  char *type;
  AvahiStringList *subtypes;
  int port;
  pthread_t thread;
  reg_callback on_reg;
  void *reg_data;
};

static void zc_server_data_free (zc_server_data *data)
{
  if (data->group)
    avahi_entry_group_reset (data->group);
  if (data->poll)
    avahi_threaded_poll_free (data->poll);
  if (data->name)
    avahi_free (data->name);
  if (data->type)
    avahi_free (data->type);
  if (data->subtypes)
    avahi_string_list_free (data->subtypes);
  free (data);
}

static zc_server_data *zc_server_data_new (reg_callback rc, void *rd)
{
  zc_server_data *sd = (zc_server_data *) malloc (sizeof (zc_server_data));
  sd->group = NULL;
  sd->poll = NULL;
  sd->name = NULL;
  sd->type = NULL;
  sd->subtypes = NULL;
  sd->port = 0;
  sd->on_reg = rc;
  sd->reg_data = rd;
  return sd;
}

static void quit_poll (zc_server_data *sd)
{
  avahi_threaded_poll_stop (sd->poll);
  zc_server_data_free (sd);
}

static void create_services (AvahiClient *c, zc_server_data *d);
static void solve_collision (AvahiClient *c, zc_server_data *d, bool reset);

static char *format_subtype (const char *type, const char *subtype)
{
  if (!subtype || strlen (subtype) == 0)
    return NULL;
  const int len = strlen (type) + strlen (subtype) + 8;
  char *sub = (char *) avahi_malloc (len);
  snprintf (sub, len, "_%s._sub.%s", subtype, type);
  return sub;
}

static void entry_group_callback (AvahiEntryGroup *g,
                                  AvahiEntryGroupState state, void *data)
{
  zc_server_data *sd = (zc_server_data *) data;

  sd->group = g;

  switch (state)
    {
      case AVAHI_ENTRY_GROUP_ESTABLISHED:
        ZEROCONF_LOG_DEBUG ("service '%s' established.\n", sd->name);
        if (sd->on_reg)
          {
            sd->on_reg (sd->reg_data);
            sd->on_reg = NULL;
          }
        break;
      case AVAHI_ENTRY_GROUP_COLLISION:
        solve_collision (avahi_entry_group_get_client (g), sd, false);
        break;
      case AVAHI_ENTRY_GROUP_FAILURE:
        {
          const char *err = avahi_strerror (
            avahi_client_errno (avahi_entry_group_get_client (g)));
          ZEROCONF_LOG_ERROR ("entry group failure: %s\n", err);
          quit_poll (sd);
          break;
        }
      case AVAHI_ENTRY_GROUP_UNCOMMITED:
      case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

static void solve_collision (AvahiClient *c, zc_server_data *sd, bool reset)
{
  char *n = avahi_alternative_service_name (sd->name);
  avahi_free (sd->name);
  sd->name = n;

  ZEROCONF_LOG_WARNING ("service name collision, renaming service to '%s'\n",
                        sd->name);

  if (reset)
    avahi_entry_group_reset (sd->group);

  create_services (c, sd);
}

static bool add_subtype (zc_server_data *sd, const char *subtype)
{
  char *st = format_subtype (sd->type, subtype);
  int ret = avahi_entry_group_add_service_subtype (
    sd->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, sd->name, sd->type, NULL,
    st);

  if (ret < 0)
    ZEROCONF_LOG_ERROR ("failed to add service subtype %s: %s\n", st,
                        avahi_strerror (ret));

  avahi_free (st);

  return ret >= 0;
}

static bool add_subtypes (zc_server_data *sd)
{
  AvahiStringList *lst = sd->subtypes;
  while (lst)
    {
      const char *txt = (const char *) avahi_string_list_get_text (lst);
      if (txt && !add_subtype (sd, txt))
        return false;
      lst = avahi_string_list_get_next (lst);
    }
  return true;
}

static void add_service (AvahiClient *c, zc_server_data *sd)
{
  int ret = avahi_entry_group_add_service_strlst (
    sd->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, sd->name, sd->type, NULL,
    NULL, sd->port, sd->subtypes);
  if (ret == AVAHI_ERR_COLLISION)
    {
      solve_collision (c, sd, true);
      return;
    }

  if (ret < 0)
    {
      ZEROCONF_LOG_ERROR ("failed to add service %s: %s\n", sd->name,
                          avahi_strerror (ret));
    }
  else if (add_subtypes (sd))
    {
      if ((ret = avahi_entry_group_commit (sd->group)) >= 0)
        return;  // utter success

      ZEROCONF_LOG_ERROR ("failed to commit entry group for %s: %s\n", sd->name,
                          avahi_strerror (ret));
    }

  quit_poll (sd);
  return;
}

static void create_services (AvahiClient *c, zc_server_data *sd)
{
  if (!sd->group)
    {
      sd->group = avahi_entry_group_new (c, entry_group_callback, sd);
      if (!sd->group)
        {
          ZEROCONF_LOG_ERROR ("avahi_entry_group_new() failed: %s\n",
                              avahi_strerror (avahi_client_errno (c)));
          quit_poll (sd);
          return;
        }
    }

  if (avahi_entry_group_is_empty (sd->group))
    {
      ZEROCONF_LOG_DEBUG ("adding service '%s'\n", sd->name);
      add_service (c, sd);
    }
}

static void client_callback (AvahiClient *c, AvahiClientState state, void *data)
{
  zc_server_data *sd = (zc_server_data *) data;
  switch (state)
    {
      case AVAHI_CLIENT_S_RUNNING:
        /* The server has startup successfully and registered its host
     * name on the network, so it's time to create our services */
        create_services (c, sd);
        break;
      case AVAHI_CLIENT_FAILURE:
        ZEROCONF_LOG_ERROR ("Client failure: %s\n",
                            avahi_strerror (avahi_client_errno (c)));
        quit_poll (sd);
        break;
      case AVAHI_CLIENT_S_COLLISION:
      /* Let's drop our registered services. When the server is back
     * in AVAHI_SERVER_RUNNING state we will register them
     * again with the new host name. */
      case AVAHI_CLIENT_S_REGISTERING:
        /* The server records are now being established. This
     * might be caused by a host name change. We need to wait
     * for our own records to register until the host name is
     * properly esatblished. */
        if (sd->group)
          avahi_entry_group_reset (sd->group);
        break;
      case AVAHI_CLIENT_CONNECTING:
        break;
    }
}

static AvahiStringList *subtypes_to_list (const char *subtypes)
{
  if (!subtypes)
    return NULL;
  AvahiStringList *result = NULL;
  char *sts = strdup (subtypes);
  char *str = sts;
  while (true)
    {
      char *sub = strtok (str, ", ");
      if (!sub)
        break;
      result = avahi_string_list_add (result, sub);
      str = NULL;
    }
  free (sts);
  return result;
}

void zeroconf_server_shutdown (zc_server_data *data)
{
  if (data)
    quit_poll (data);
}

zc_server_data *zeroconf_server_announce (const char *name, const char *type,
                                          const char *subtypes, int port,
                                          reg_callback on_reg, void *reg_data)
{
  if (!type)
    {
      ZEROCONF_LOG_ERROR ("Null service type for '%s'\n", name);
      return NULL;
    }

  ZEROCONF_LOG_DEBUG ("Announcing '%s' with type %s%s%s\n", name, type,
                      subtypes ? " and subtypes " : "",
                      subtypes ? subtypes : "");

  zc_server_data *data = zc_server_data_new (on_reg, reg_data);
  data->poll = avahi_threaded_poll_new ();


  if (!data->poll)
    {
      ZEROCONF_LOG_ERROR ("Failed to create simple poll object for '%s'.\n",
                          name);
      zc_server_data_free (data);
      return NULL;
    }

  data->port = port;
  data->name = avahi_strdup (name);
  data->type = avahi_strdup (type);
  data->subtypes = subtypes_to_list (subtypes);
  data->group = NULL;

  int error = 0;
  AvahiClient *client = avahi_client_new (avahi_threaded_poll_get (data->poll),
                                          0, client_callback, data, &error);

  if (!client)
    {
      ZEROCONF_LOG_ERROR ("Failed to create Avahi server: %s\n",
                          avahi_strerror (error));
      zc_server_data_free (data);
      return NULL;
    }

  if (avahi_threaded_poll_start (data->poll))
    {
      ZEROCONF_LOG_ERROR ("Failed to start Avahi server %s\n", "");
      zc_server_data_free (data);
      return NULL;
    }

  return data;
}
