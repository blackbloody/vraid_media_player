#ifndef WS_SINK_H
#define WS_SINK_H
#pragma once
#include <string>

class WsHub;
class WsClient;

struct WsSink {
    virtual ~WsSink() = default;
    virtual void on_ws_message(const std::string& m) = 0;
};

#endif // WS_SINK_H
