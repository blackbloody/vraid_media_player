#include "ws_hub.h"

void WsHub::register_sink(WsSink* sink) {
    sinks.insert(sink);
}

void WsHub::unregister_sink(WsSink *sink) {
    sinks.erase(sink);
}

void WsHub::publish(const std::string& command) {
    sig_message.emit(command);
    for (auto sink : sinks) {
        sink->on_ws_message(command);
    }
}
