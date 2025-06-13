#pragma once
#include <string>

class WebsocketBackend {
public:
    static WebsocketBackend& instance();
    void broadcast(const std::string& eventType, const std::string& payload);

private:
    WebsocketBackend() = default;
};