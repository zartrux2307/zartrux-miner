#include "WebsocketBackend.h"

WebsocketBackend& WebsocketBackend::instance() {
    static WebsocketBackend backend;
    return backend;
}

void WebsocketBackend::broadcast(const std::string&, const std::string&) {
    // No-op stub
}