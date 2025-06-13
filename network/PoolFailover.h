#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
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

    explicit PoolFailover(asio::io_context& io_context, 
                         std::vector<PoolInfo> pools);
    ~PoolFailover();

    void start();
    void stop();
    void submit(const std::string& job_id, 
               const std::string& nonce_hex, 
               const std::string& result_hash);

    std::function<void(const MiningJob&)> onNewJob;
    std::function<void(bool, const std::string&)> onShareAccepted;

private:
    void tryNextPool();
    void handlePoolError(const std::string& error);
    void scheduleNextTry();

    asio::io_context& m_io_context;
    std::vector<PoolInfo> m_pools;
    size_t m_currentIndex = 0;
    std::shared_ptr<StratumClient> m_client;
    asio::steady_timer m_retryTimer;
    std::atomic<bool> m_active{false};
    std::atomic<int> m_retryCount{0};
};