#ifndef WS_CLIENT_H
#define WS_CLIENT_H
#pragma once

#include <string>
#include <libsoup/soup.h>
#include <ws_hub.h>

class WsClient {
public:
    WsClient(/*WsHub& hub, */const std::string& url) : /*hub_(&hub), */url_(url) {
        session_ = soup_session_new();
    }
    ~WsClient();
    void setHub(WsHub& hub) {
        this->hub_ = &hub;
    }

    void connect();
    void close(unsigned short code = 1000, const std::string& reason = {});
    void send(const std::string& s);
    void send_with_time(const double& start, const double& viewport_sec);

    void set_reconnect(bool enanle, int base_ms = 1000, int max_ms = 15000);
    void enable_protocol_keepalive(unsigned sec) { proto_keepalive_sec_ = sec; }

private:
    WsHub* hub_{nullptr};
    std::string url_;
    SoupSession* session_{nullptr};
    SoupWebsocketConnection* connection_{nullptr};

    void wire_signals();

    long last_rx_epoch_{0};
    int ping_every_sec_{25};
    int dead_after_sec_{75};

    unsigned proto_keepalive_sec_{25};
};

#endif // WS_CLIENT_H
