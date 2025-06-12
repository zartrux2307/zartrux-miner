#include "PoolFailover.h"
#include "StratumClient.h"
#include "utils/Logger.h"

PoolFailover::PoolFailover(asio::io_context& io_context, std::vector<PoolInfo> pools)
    : m_io_context(io_context), m_pools(std::move(pools)), m_retryTimer(io_context) {}

PoolFailover::~PoolFailover() {
    stop();
}

void PoolFailover::start() {
    if (m_pools.empty()) {
        Logger::error("PoolFailover", "La lista de pools está vacía. No se puede iniciar.");
        return;
    }
    tryNextPool();
}

void PoolFailover::stop() {
    m_is_stopped = true;
    m_retryTimer.cancel();
    if (m_client) {
        m_client->disconnect();
    }
}

void PoolFailover::tryNextPool() {
    if (m_is_stopped || m_pools.empty()) return;

    m_client = std::make_shared<StratumClient>(m_io_context);
    const auto& currentPool = m_pools[m_currentIndex];

    Logger::info("PoolFailover", "Conectando al pool: %s:%d", currentPool.host.c_str(), currentPool.port);

    m_client->onConnected = [this, host = currentPool.host]() {
        Logger::info("PoolFailover", "Conectado exitosamente a %s", host.c_str());
    };
    
    m_client->onNewJob = [this](const MiningJob& job) {
        if (onNewJob) onNewJob(job);
    };

    m_client->onError = [this](const std::string& error) { this->onClientError(error); };
    m_client->onDisconnected = [this](const std::string& error) { this->onClientError(error); };

    m_client->connectToPool(currentPool.host, currentPool.port, currentPool.user, currentPool.pass);
}

void PoolFailover::onClientError(const std::string& error) {
    Logger::warn("PoolFailover", "Error o desconexión del pool actual: %s. Cambiando al siguiente en 5 segundos.", error.c_str());
    m_client->disconnect();
    
    m_currentIndex = (m_currentIndex + 1) % m_pools.size();

    m_retryTimer.expires_after(std::chrono::seconds(5));
    m_retryTimer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) tryNextPool();
    });
}