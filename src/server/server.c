#include "server.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-version.h>

#if SOUP_CHECK_VERSION(3, 0, 0)

    #include <libsoup/soup-server-message.h>

#endif

#include "../utils/logger.h"
#include "../utils/audio_loader.h"

#define DEFAULT_PORT 8080

struct _Server {
    GObject parent;

    SoupServer *soup_server;

    GSList *websocket_connections;

    char *audio_buffer;
    int audio_buffer_size;
};

G_DEFINE_TYPE(Server, server, G_TYPE_OBJECT)

enum {
    SIGNAL_WS_CLIENT_CONNECTED,
    SIGNAL_WS_CLIENT_DISCONNECTED,
    SIGNAL_DATA_CHUNK_DESCRIPTOR,
    SIGNAL_DATA_CHUNK,
    N_SIGNALS
};

/// Custom signals
static guint signals[N_SIGNALS];

Server *server_new() {
    Server *server = MY_SERVER(g_object_new(TYPE_SERVER, NULL));

    guint8 channels;
    gint32 sampleRate;
    guint8 bitsPerSample;
    server->audio_buffer = load_wav_c("test_audio.wav", &channels, &sampleRate, &bitsPerSample, &server->audio_buffer_size);
    g_assert(server->audio_buffer);

    return server;
}

#if !SOUP_CHECK_VERSION(3, 0, 0)
static void http_cb(SoupServer *server,
                    SoupMessage *msg,
                    const char *path,
                    GHashTable *query,
                    SoupClientContext *client,
                    gpointer user_data) {
    // We're not serving any HTTP traffic - if somebody (erroneously) submits an HTTP request, tell them to get
    // lost.
    ALOGE("Got an erroneous HTTP request from %s", soup_client_context_get_host(client));
    soup_message_set_status(msg, SOUP_STATUS_NOT_FOUND);
}
#else

static void http_cb(SoupServer *server,     //
                    SoupServerMessage *msg, //
                    const char *path,       //
                    GHashTable *query,      //
                    gpointer user_data) {
    // We're not serving any HTTP traffic - if somebody (erroneously) submits an HTTP request, tell them to get
    // lost.
    ALOGE("Got an erroneous HTTP request from %s", soup_server_message_get_remote_host(msg));
    soup_server_message_set_status(msg, SOUP_STATUS_NOT_FOUND, NULL);
}

#endif

static void server_handle_json_message(Server *server, SoupWebsocketConnection *connection, GBytes *message) {
    gsize length = 0;
    const gchar *msg_data = g_bytes_get_data(message, &length);

    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    if (json_parser_load_from_data(parser, msg_data, length, &error)) {
        JsonObject *msg = json_node_get_object(json_parser_get_root(parser));

        if (!json_object_has_member(msg, "msg")) {
            // Invalid message
            goto out;
        }

        const gchar *msg_type = json_object_get_string_member(msg, "msg");
        if (g_str_equal(msg_type, "answer")) {
            const gchar *answer_sdp = json_object_get_string_member(msg, "sdp");
            // ALOGD("Received answer:\n %s", answer_sdp);

            g_signal_emit(server, signals[SIGNAL_DATA_CHUNK_DESCRIPTOR], 0, connection, answer_sdp);
        }
    } else {
        ALOGD("Error parsing message: %s", error->message);
        g_clear_error(&error);
    }

out:
    g_object_unref(parser);
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void message_cb(SoupWebsocketConnection *connection, gint type, GBytes *message, gpointer user_data) {
    switch (type) {
        case SOUP_WEBSOCKET_DATA_BINARY: {
            ALOGD("Received unknown binary message from client %p, ignoring", connection);
            break;
        }
        case SOUP_WEBSOCKET_DATA_TEXT: {
            gsize length = 0;
            const gchar *msg_str = g_bytes_get_data(message, &length);
            ALOGD("Received text message from client %p: %s", connection, msg_str);

            const gchar *reply_str = "OK, prepare to receive the binary data.";
            soup_websocket_connection_send_text(connection, reply_str);

            char test_data_buf[] = "This is some test binary data";
            soup_websocket_connection_send_binary(connection, test_data_buf, ARRAY_SIZE(test_data_buf));

            // server_handle_json_message(MY_SERVER(user_data), connection, message);
        } break;
        default:
            g_assert_not_reached();
    }
}

static void server_remove_websocket_connection(Server *server, SoupWebsocketConnection *connection) {
    ALOGD("Removed websocket connection: %p", connection);

    // Currently, client_id is the same as the connection's pointer
    ClientId client_id = g_object_get_data(G_OBJECT(connection), "client_id");

    server->websocket_connections = g_slist_remove(server->websocket_connections, client_id);

    g_signal_emit(server, signals[SIGNAL_WS_CLIENT_DISCONNECTED], 0, client_id);
}

static void closed_cb(SoupWebsocketConnection *connection, gpointer user_data) {
    ALOGD("Connection closed: %p", connection);

    server_remove_websocket_connection(MY_SERVER(user_data), connection);
}

static void server_add_websocket_connection(Server *server, SoupWebsocketConnection *connection) {
    ALOGD("Added websocket connection: %p", connection);

    g_object_ref(connection);
    server->websocket_connections = g_slist_append(server->websocket_connections, connection);
    g_object_set_data(G_OBJECT(connection), "client_id", connection);

    g_signal_connect(connection, "message", message_cb, server);
    g_signal_connect(connection, "closed", closed_cb, server);

    g_signal_emit(server, signals[SIGNAL_WS_CLIENT_CONNECTED], 0, connection);
}

#if !SOUP_CHECK_VERSION(3, 0, 0)
static void websocket_cb(SoupServer *server,
                         SoupWebsocketConnection *connection,
                         const char *path,
                         SoupClientContext *client,
                         gpointer user_data) {
    ALOGD("New websocket connection from %s", soup_client_context_get_host(client));

    server_add_websocket_connection(MY_SERVER(user_data), connection);
}
#else

static void websocket_cb(SoupServer *server,
                         SoupServerMessage *msg,
                         const char *path,
                         SoupWebsocketConnection *connection,
                         gpointer user_data) {
    ALOGD("New connection from somewhere");

    server_add_websocket_connection(MY_SERVER(user_data), connection);
}

#endif

static void server_init(Server *server) {
    GError *error = NULL;

    server->soup_server = soup_server_new(NULL, NULL);
    g_assert_no_error(error);

    soup_server_add_handler(server->soup_server, NULL, http_cb, server, NULL);
    soup_server_add_websocket_handler(server->soup_server, "/ws", NULL, NULL, websocket_cb, server, NULL);

    soup_server_listen_all(server->soup_server, DEFAULT_PORT, 0, &error);
    g_assert_no_error(error);

    ALOGI("Server initialized, listening on: %u", DEFAULT_PORT);
}

static void server_send_msg_to_client(Server *server, ClientId client_id, gchar *msg_str) {
    SoupWebsocketConnection *connection = client_id;
    g_info("%s", __func__);

    if (!g_slist_find(server->websocket_connections, connection)) {
        g_warning("Unknown websocket connection.");
        return;
    }

    SoupWebsocketState socket_state = soup_websocket_connection_get_state(connection);

    if (socket_state == SOUP_WEBSOCKET_STATE_OPEN) {
        soup_websocket_connection_send_text(connection, msg_str);
    } else {
        g_warning("Trying to send message using websocket that isn't open.");
    }
}

static void server_send_json_to_client(Server *server, ClientId client_id, JsonNode *msg) {
    gchar *msg_str = json_to_string(msg, TRUE);

    server_send_msg_to_client(server, client_id, msg_str);

    g_free(msg_str);
}

static void server_dispose(GObject *object) {
    Server *self = MY_SERVER(object);

    soup_server_disconnect(self->soup_server);
    g_clear_object(&self->soup_server);

    ALOGD("Server disconnected");
}

static void server_class_init(ServerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = server_dispose;

    signals[SIGNAL_WS_CLIENT_CONNECTED] = g_signal_new("ws-client-connected",
                                                       G_OBJECT_CLASS_TYPE(klass),
                                                       G_SIGNAL_RUN_LAST,
                                                       0,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       G_TYPE_NONE,
                                                       1,
                                                       G_TYPE_POINTER);

    signals[SIGNAL_WS_CLIENT_DISCONNECTED] = g_signal_new("ws-client-disconnected",
                                                          G_OBJECT_CLASS_TYPE(klass),
                                                          G_SIGNAL_RUN_LAST,
                                                          0,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          G_TYPE_NONE,
                                                          1,
                                                          G_TYPE_POINTER);

    signals[SIGNAL_DATA_CHUNK_DESCRIPTOR] = g_signal_new("data-chunk-descriptor",
                                                         G_OBJECT_CLASS_TYPE(klass),
                                                         G_SIGNAL_RUN_LAST,
                                                         0,
                                                         NULL,
                                                         NULL,
                                                         NULL,
                                                         G_TYPE_NONE,
                                                         2,
                                                         G_TYPE_POINTER,
                                                         G_TYPE_STRING);
}
