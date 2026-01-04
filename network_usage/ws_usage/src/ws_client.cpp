#include "ws_client.h"
#include <nlohmann/json.hpp>
#include <iostream>

WsClient::~WsClient() {
    close();
    if (session_) {
        g_object_unref(session_);
        session_ = nullptr;
    }
}

void WsClient::connect() {

    if (connection_)
        return;

    SoupMessage* msg = soup_message_new("GET", url_.c_str());
    soup_session_websocket_connect_async(
        session_, msg, nullptr, nullptr, nullptr,
        +[](GObject* source, GAsyncResult* res, gpointer user) {
            auto* self = static_cast<WsClient*>(user);
            GError* error = nullptr;

            self->connection_ = soup_session_websocket_connect_finish(SOUP_SESSION(source), res, &error);

            if (!self->connection_) {
                if (error) {
                    // g_warning("Failed to connect to websocket: %s", error->message);
                    // g_error_free(error);
                }
                self->connection_ = nullptr;
                return;
            }
            soup_websocket_connection_set_keepalive_interval(self->connection_, self->proto_keepalive_sec_);
            self->wire_signals();
        },
        this
        );
    g_object_unref(msg);

}

void WsClient::wire_signals() {
    if (!connection_) return;
    ///*
    g_signal_handlers_disconnect_by_data(connection_, this);

    g_signal_connect(connection_, "message",
                     G_CALLBACK(+[] (SoupWebsocketConnection*, SoupWebsocketDataType type, GBytes* b, gpointer u) {
                         auto* self = static_cast<WsClient*>(u);
                         if (type != SOUP_WEBSOCKET_DATA_TEXT) return;
                         gsize n = 0;
                         const char* p = (const char*)g_bytes_get_data(b, &n);
                         try {
                             auto j = nlohmann::json::parse(std::string(p, p+n));
                             std::string payload = j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

                             if (self->hub_) {
                                 // std::cout << payload << std::endl;
                                 self->hub_->publish(payload);
                             } else {
                                 // std::cerr << "Null hub" << std::endl;
                             }
                             // self->hub_.publish(payload);

                         } catch (const std::exception& e) {
                             // g_warning("WS error received: %s", e.what());
                         }
                     }), this);

    g_signal_connect(connection_, "pong",
                     G_CALLBACK(+[] (SoupWebsocketConnection*, GBytes* payload, gpointer){
                         // g_message("WS PONG (%zu bytes)", g_bytes_get_size(payload));
                     }), nullptr);

    g_signal_connect(connection_, "error",
                     G_CALLBACK(+[](SoupWebsocketConnection*, GError* err, gpointer){
                         // g_warning("WS error: %s", err ? err->message : "unknown");
                     }), nullptr);

    g_signal_connect(connection_, "closed",
                     G_CALLBACK(+[](SoupWebsocketConnection* conn, gpointer u) {
                         auto* self = static_cast<WsClient*>(u);

                         guint16 code = soup_websocket_connection_get_close_code(conn);
                         const char* reason = soup_websocket_connection_get_close_data(conn);
                         // g_message("WS closed (code=u%, reason=%s)", code, reason ? reason : "");
                         // g_message("WS closed (code=%u, reason=%s)", (unsigned)code, reason ? reason : "");

                         // Clean Up
                         g_signal_handlers_disconnect_by_data(conn, self);
                         g_clear_object(&self->connection_);
                         // self->connection_ = nullptr;
                     }), this);
    //*/
}

void WsClient::close(unsigned short code, const std::string& reason) {
    if (!connection_) return;

    // 1) Detach our handlers so no callbacks hit 'this' after this point
    g_signal_handlers_disconnect_by_data(connection_, this);

    // 2) Initiate close; a "closed" will usually fire, but we won't rely on it
    soup_websocket_connection_close(connection_, code, reason.c_str());

    // 3) Drop our ref regardless; we already detached signals
    g_clear_object(&connection_);
}

void WsClient::send(const std::string& s) {
    nlohmann::json command_job = { {"v",1}, {"op",s}, {"server","ws-ml"} };
    std::string payload = command_job.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    if (!connection_) return;
    if (payload.empty()) return;
    if (payload.size() > (1<<20)) return; // WS send too large
    soup_websocket_connection_send_text(connection_, payload.c_str());
}

void WsClient::send_with_time(const double& start, const double& viewport_sec) {
    nlohmann::json command_job =
    {
        {"v",1},
        {"op","reader_time"},
        {"start",start},
        {"duration",viewport_sec},
        {"server","ws-ml"}
    };
    std::string payload = command_job.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    if (!connection_) return;
    if (payload.empty()) return;
    if (payload.size() > (1<<20)) return; // WS send too large
    soup_websocket_connection_send_text(connection_, payload.c_str());
}

void WsClient::set_reconnect(bool enanle, int base_ms, int max_ms) { }
