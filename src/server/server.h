#pragma once

#include <glib-object.h>

#define TYPE_SERVER server_get_type()

G_DECLARE_FINAL_TYPE(Server, server, MY, SERVER, GObject)

typedef gpointer ClientId;

Server *server_new();
