#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include "core/JobManager.h"

class StratumClient;

class PoolFailover {
public:
    struct PoolInfo {
        std::string host;
        uint16_t port;
        std::string user;
        std::string pass;
    };

    explicit PoolFailover(asio::io_context& io_context, std::vector<PoolInfo> pools);
    ~PoolFailover();

    void start();
    void stop();

    std::function<void(const MiningJob&)> onNewJob;

private:
    void tryNextPool();
    void onClientError(const std::string& error);

    asio::io_context& m_io_context;
    std::vector<PoolInfo> m_pools;
    size_t m_currentIndex = 0;
    std::shared_ptr<StratumClient> m_client;
    
    asio::steady_timer m_retryTimer;
    std::atomic<bool> m_is_stopped{false};
};