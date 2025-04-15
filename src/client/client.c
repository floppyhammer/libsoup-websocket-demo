#ifdef __linux__
    #include <glib-unix.h>
#endif
#include <json-glib/json-glib.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-session.h>
#include <stdint.h>

#include "../utils/logger.h"
#include "stdio.h"

static gchar *websocket_uri = NULL;

#define WEBSOCKET_URI_DEFAULT "ws://127.0.0.1:8080/ws"

static GOptionEntry options[] = {{
                                     "websocket-uri",
                                     'u',
                                     0,
                                     G_OPTION_ARG_STRING,
                                     &websocket_uri,
                                     "Websocket URI of webrtc signaling connection",
                                     "URI",
                                 },
                                 {NULL}};

struct MyState {
    SoupWebsocketConnection *connection;
    guint timeout_id;
};

struct MyState ws_state = {};

/*
 *
 * Websocket connection.
 *
 */

// Main loop breaker
static gboolean sigint_handler(gpointer user_data) {
    g_main_loop_quit(user_data);
    return G_SOURCE_REMOVE;
}

static void websocket_message_cb(SoupWebsocketConnection *connection, gint type, GBytes *message, gpointer user_data) {
    switch (type) {
        case SOUP_WEBSOCKET_DATA_BINARY: {
            gsize data_size = g_bytes_get_size(message);
            ALOGE("Received binary message, size: %lu\n", data_size);
            break;
        }
        case SOUP_WEBSOCKET_DATA_TEXT: {
            gsize length = 0;
            const gchar *msg_str = g_bytes_get_data(message, &length);
            ALOGE("Received text message: %s", msg_str);

            // server_handle_message(MY_SERVER(user_data), connection, message);
        } break;
        default:
            g_assert_not_reached();
    }
    //
    //     gsize length = 0;
    //     const gchar *msg_data = g_bytes_get_data(message, &length);
    //
    //     JsonParser *parser = json_parser_new();
    //     GError *error = NULL;
    //
    //     if (json_parser_load_from_data(parser, msg_data, length, &error)) {
    //         JsonObject *msg = json_node_get_object(json_parser_get_root(parser));
    //
    //         if (!json_object_has_member(msg, "msg")) {
    //             // Invalid message
    //             goto out;
    //         }
    //
    //         const gchar *msg_type = json_object_get_string_member(msg, "msg");
    //         g_print("Websocket message received: %s\n", msg_type);
    //
    //         if (g_str_equal(msg_type, "offer")) {
    //             const gchar *offer_sdp = json_object_get_string_member(msg, "sdp");
    //             // process_sdp_offer(offer_sdp);
    //         } else if (g_str_equal(msg_type, "candidate")) {
    //             JsonObject *candidate = json_object_get_object_member(msg, "candidate");
    //
    //             // process_candidate(json_object_get_int_member(candidate, "sdpMLineIndex"),
    //             //                   json_object_get_string_member(candidate, "candidate"));
    //         }
    //     } else {
    //         g_debug("Error parsing message: %s", error->message);
    //         g_clear_error(&error);
    //     }
    //
    // out:
    //     g_object_unref(parser);
}

static void websocket_closed_cb(SoupWebsocketConnection *connection, gint type, GBytes *message, gpointer user_data) {
    g_clear_handle_id(&ws_state.timeout_id, g_source_remove);
}

gboolean send_test_message(SoupWebsocketConnection *connection) {
    SoupWebsocketState socket_state = soup_websocket_connection_get_state(connection);

    if (socket_state == SOUP_WEBSOCKET_STATE_OPEN) {
        // gchar *msg_str = json_to_string(msg, TRUE);

        soup_websocket_connection_send_text(connection, "Hi! from client.");

        // g_free(msg_str);
    } else {
        g_warning("Trying to send message using websocket that isn't open!");
    }

    return G_SOURCE_CONTINUE;
}

static void websocket_connected_cb(GObject *session, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;

    g_assert(!ws_state.connection);

    ws_state.connection = soup_session_websocket_connect_finish(SOUP_SESSION(session), res, &error);

    if (error) {
        g_print("Error creating websocket: %s\n", error->message);
        g_clear_error(&error);
    } else {
        g_print("Websocket connected\n");

        g_signal_connect(ws_state.connection, "message", G_CALLBACK(websocket_message_cb), NULL);
        g_signal_connect(ws_state.connection, "closed", G_CALLBACK(websocket_closed_cb), NULL);

        ws_state.timeout_id = g_timeout_add_seconds(3, G_SOURCE_FUNC(send_test_message), ws_state.connection);
    }
}

int create_client(int argc, char *argv[]) {
    GError *error = NULL;

    GOptionContext *option_context = g_option_context_new(NULL);
    g_option_context_add_main_entries(option_context, options, NULL);

    if (!g_option_context_parse(option_context, &argc, &argv, &error)) {
        g_print("Option context parsing failed: %s\n", error->message);
        exit(1);
    }

    if (!websocket_uri) {
        websocket_uri = g_strdup(WEBSOCKET_URI_DEFAULT);
    }

    SoupSession *soup_session = soup_session_new();

#if !SOUP_CHECK_VERSION(3, 0, 0)
    soup_session_websocket_connect_async(soup_session,                                     // session
                                         soup_message_new(SOUP_METHOD_GET, websocket_uri), // message
                                         NULL,                                             // origin
                                         NULL,                                             // protocols
                                         NULL,                                             // cancellable
                                         websocket_connected_cb,                           // callback
                                         NULL);                                            // user_data
#else
    soup_session_websocket_connect_async(soup_session,                                     // session
                                         soup_message_new(SOUP_METHOD_GET, websocket_uri), // message
                                         NULL,                                             // origin
                                         NULL,                                             // protocols
                                         0,                                                // io_priority
                                         NULL,                                             // cancellable
                                         websocket_connected_cb,                           // callback
                                         NULL);                                            // user_data
#endif

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
#ifdef __linux__
    g_unix_signal_add(SIGINT, sigint_handler, loop);
#endif

    g_main_loop_run(loop);

    // Cleanup
    g_main_loop_unref(loop);
    g_clear_pointer(&websocket_uri, g_free);

    return 0;
}
