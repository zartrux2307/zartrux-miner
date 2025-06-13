#include "PoolFailover.h"
#include "StratumClient.h"
#include "utils/Logger.h"

PoolFailover::PoolFailover(asio::io_context& io_context, 
                          std::vector<PoolInfo> pools)
    : m_io_context(io_context), 
      m_pools(std::move(pools)), 
      m_retryTimer(io_context) {}

PoolFailover::~PoolFailover() {
    stop();
}

void PoolFailover::start() {
    if (m_pools.empty()) {
        Logger::error("PoolFailover", "No pools configured");
        return;
    }
    
    m_active = true;
    tryNextPool();
}

void PoolFailover::stop() {
    m_active = false;
    m_retryTimer.cancel();
    if (m_client) {
        m_client->disconnect();
    }
}

void PoolFailover::submit(const std::string& job_id, 
                        const std::string& nonce_hex, 
                        const std::string& result_hash) {
    if (m_client) {
        m_client->submit(job_id, nonce_hex, result_hash);
    }
}

void PoolFailover::tryNextPool() {
    if (!m_active || m_pools.empty()) return;
    
    m_currentIndex = (m_currentIndex + 1) % m_pools.size();
    const auto& pool = m_pools[m_currentIndex];
    
    Logger::info("PoolFailover", "Connecting to pool #%zu: %s:%d", 
                m_currentIndex, pool.host.c_str(), pool.port);
    
    m_client = std::make_shared<StratumClient>(m_io_context);
    
    // Configure client callbacks
    m_client->onConnected = [this, pool]() {
        Logger::info("PoolFailover", "Connected to %s", pool.host.c_str());
        m_retryCount = 0;
    };
    
    m_client->onNewJob = [this](const MiningJob& job) {
        if (onNewJob) onNewJob(job);
    };
    
    m_client->onShareAccepted = [this](bool accepted, const std::string& reason) {
        if (onShareAccepted) onShareAccepted(accepted, reason);
    };
    
    m_client->onError = [this](const std::string& error) {
        handlePoolError(error);
    };
    
    m_client->onDisconnected = [this]() {
        handlePoolError("Connection lost");
    };
    
    m_client->connectToPool(pool.host, pool.port, pool.user, pool.pass);
}

void PoolFailover::handlePoolError(const std::string& error) {
    if (!m_active) return;
    
    m_retryCount++;
    Logger::warn("PoolFailover", "Pool error (%d/%d): %s", 
                m_retryCount, 5, error.c_str());
    
    if (m_retryCount >= 5) {
        Logger::info("PoolFailover", "Max retries reached, trying next pool");
        tryNextPool();
    } else {
        scheduleNextTry();
    }
}

void PoolFailover::scheduleNextTry() {
    const int delay_seconds = std::min(5 * m_retryCount, 30);
    Logger::info("PoolFailover", "Retrying in %d seconds", delay_seconds);
    
    m_retryTimer.expires_after(std::chrono::seconds(delay_seconds));
    m_retryTimer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec && m_active) {
            tryNextPool();
        }
    });
}