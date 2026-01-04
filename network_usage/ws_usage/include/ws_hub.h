#ifndef WS_HUB_H
#define WS_HUB_H
#pragma once
#include <sigc++/sigc++.h>
#include <unordered_set>
#include "ws_sink.h"

class WsHub {
private:
    std::unordered_set<WsSink*> sinks;
    sigc::signal<void, const std::string&> sig_message;

public:
    void register_sink(WsSink* sink);
    void unregister_sink(WsSink* sink);

    void publish(const std::string& command);

    sigc::connection on_any(const sigc::slot<void, const std::string&>& slot) {
        return sig_message.connect(slot);
    }
};

#endif //WS_HUB_H
