#include "PoolDispatcher.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <random>
#include <thread>
#include <fmt/format.h>

namespace zartrux::dispatcher {
    using json = nlohmann::json;
    
    PoolDispatcher& PoolDispatcher::instance() {
        static PoolDispatcher instance;
        return instance;
    }
    
    PoolDispatcher::PoolDispatcher() {
        // Valores por defecto
        m_iaEndpoint = {"http://localhost:8000/ia/submit", "", "", Protocol::STRATUM_V2};
        m_poolEndpoint = {"http://localhost:3333/submit", "", "", Protocol::STRATUM_V2};
    }
    
    void PoolDispatcher::setMode(::MiningMode mode) {
        m_currentMode = mode;
         Logger::info("PoolDispatcher", "mode set to: {}",
                     MiningModeManager::modeToString(mode))
    }
    
    void PoolDispatcher::setEndpoints(const std::string& iaEndpoint, 
                                     const std::string& poolEndpoint,
                                     const std::string& poolUser,
                                     const std::string& poolPass) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_iaEndpoint.url = iaEndpoint;
        m_poolEndpoint.url = poolEndpoint;
        m_poolEndpoint.user = poolUser;
        m_poolEndpoint.pass = poolPass;
        
          Logger::info("PoolDispatcher", "Endpoints configured - IA: {}, Pool: {}", iaEndpoint, poolEndpoint);
    }
    
    void PoolDispatcher::setHybridRatio(double ratio) {
        if (ratio < 0.0 || ratio > 1.0) {
            throw std::invalid_argument("Hybrid ratio must be between 0.0 and 1.0");
        }
        m_hybridRatio = ratio;
        Logger::info("PoolDispatcher", "Hybrid ratio set to: {:.2f}", ratio);
    }
    
    void PoolDispatcher::setRetryPolicy(uint8_t maxRetries, uint16_t retryDelayMs) {
        if (maxRetries > 10) maxRetries = 10;
        if (retryDelayMs < 100) retryDelayMs = 100;
        m_maxRetries = maxRetries;
        m_retryDelayMs = retryDelayMs;
        Logger::info("PoolDispatcher", "Retry policy: {} attempts, {}ms delay", maxRetries, retryDelayMs);
    }
    
    void PoolDispatcher::setTimeout(uint16_t timeoutMs) {
        if (timeoutMs < 100) timeoutMs = 100;
        m_timeoutMs = timeoutMs;
        Logger::info("PoolDispatcher", "HTTP timeout set to: {}ms", timeoutMs);
    }
    
    void PoolDispatcher::setSmartThreshold(double threshold) {
        if (threshold <= 0.0) {
            throw std::invalid_argument("Smart threshold must be > 0.0");
        }
        m_smartThreshold = threshold;
       Logger::info("PoolDispatcher", "Smart threshold set to: {:.2f}", threshold);
    }
    
    void PoolDispatcher::registerDispatchCallback(DispatchCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.push_back(callback);
        Logger::info("PoolDispatcher", "Dispatch callback registered");
    }
    
    bool PoolDispatcher::sendViaHttp(const PoolConfig& endpoint, 
                                    const std::string& payload, 
                                    double& outLatencyMs) {
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            cpr::Session session;
            session.SetUrl(cpr::Url{endpoint.url});
            session.SetHeader({{"Content-Type", "application/json"}});
            session.SetBody(cpr::Body{payload});
            session.SetTimeout(std::chrono::milliseconds(m_timeoutMs));
            
            if (!endpoint.user.empty()) {
                session.SetAuth(cpr::Authentication{endpoint.user, endpoint.pass});
            }
            
            auto response = session.Post();
            outLatencyMs = std::chrono::duration<double, std::milli>(
                std::chrono::high_resolution_clock::now() - start).count();
            
            bool success = (response.status_code >= 200 && response.status_code < 300);
            if (!success) {
                Logger::warn("PoolDispatcher", "HTTP error {} for endpoint: {}", response.status_code, endpoint.url);
            }
            return success;
            
        } catch (const std::exception& ex) {
              Logger::error("PoolDispatcher", "HTTP exception for {}: {}", endpoint.url, ex.what());
            outLatencyMs = 0;
            return false;
        }
    }
    
    std::pair<bool, double> PoolDispatcher::retrySend(const PoolConfig& endpoint, 
                                                     const std::string& payload) {
        for (uint8_t attempt = 0; attempt < m_maxRetries; ++attempt) {
            double latencyMs = 0.0;
            bool success = sendViaHttp(endpoint, payload, latencyMs);
            
            if (success) {
                return {true, latencyMs};
            }
            
            if (attempt < m_maxRetries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(m_retryDelayMs));
                 Logger::debug("PoolDispatcher", "Retry {} for endpoint {}", attempt + 1, endpoint.url);
            }
        }
        return {false, 0};
    }
    
    bool PoolDispatcher::dispatchValidNonce(const std::string& jobId, uint64_t nonce,
                                           const std::string& resultHash, 
                                           const std::string& workerId) {
        const auto target = selectTargetEndpoint();
        json payload = createPayload(target.protocol, jobId, nonce, resultHash, workerId);
        
        auto [success, latencyMs] = retrySend(target, payload.dump());
        updateStats(target.url, success, latencyMs);
        
        // Notificar callbacks
        std::vector<DispatchCallback> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbacksCopy = m_callbacks;
        }
        for (const auto& callback : callbacksCopy) {
            callback(success, target.url, latencyMs);
        }
        
        return success;
    }
    
    PoolDispatcher::PoolConfig PoolDispatcher::selectTargetEndpoint() const {
        switch (m_currentMode.load()) {
            case MiningMode::IA:
                return m_iaEndpoint;
                
            case MiningMode::SOLO:
                return m_iaEndpoint; // Modo solo usa IA
                
            case MiningMode::HYBRID: {
                static thread_local std::mt19937 gen(std::random_device{}());
                static std::uniform_real_distribution<> dis(0.0, 1.0);
                return (dis(gen) < m_hybridRatio ? m_iaEndpoint : m_poolEndpoint;
            }
                
            case MiningMode::SMART: {
                double iaLatency = getCurrentLatency(m_iaEndpoint.url);
                double poolLatency = getCurrentLatency(m_poolEndpoint.url);
                
                // Fallback si no hay datos
                if (iaLatency == 0.0 && poolLatency == 0.0) {
                    return m_iaEndpoint;
                } else if (iaLatency == 0.0) {
                    return m_poolEndpoint;
                } else if (poolLatency == 0.0) {
                    return m_iaEndpoint;
                }
                
                return (iaLatency < poolLatency * m_smartThreshold) ? m_iaEndpoint : m_poolEndpoint;
            }
                
            default: // POOL y otros
                return m_poolEndpoint;
        }
    }
    
    nlohmann::json PoolDispatcher::createPayload(Protocol protocol,
                                               const std::string& jobId, 
                                               uint64_t nonce, 
                                               const std::string& resultHash, 
                                               const std::string& workerId) const {
        switch (protocol) {
            case Protocol::STRATUM_V1:
                return {
                    {"method", "mining.submit"},
                    {"params", {
                        workerId.empty() ? "zartrux_miner" : workerId,
                        jobId,
                        fmt::format("{:016x}", nonce),
                        resultHash
                    }},
                    {"id", 1}
                };
                
            case Protocol::STRATUM_V2:
                return {
                    {"job_id", jobId},
                    {"nonce", nonce},
                    {"result", resultHash},
                    {"worker_id", workerId.empty() ? "zartrux_miner" : workerId}
                };
                
            case Protocol::ETHPROTOCOL_V1:
                return {
                    {"jsonrpc", "2.0"},
                    {"method", "eth_submitWork"},
                    {"params", {
                        fmt::format("0x{:016x}", nonce),
                        resultHash,
                        jobId
                    }},
                    {"id", 1}
                };
                
            default:
                return {
                    {"job_id", jobId},
                    {"nonce", nonce},
                    {"result_hash", resultHash},
                    {"worker_id", workerId}
                };
        }
    }
    
    void PoolDispatcher::updateStats(const std::string& endpoint, 
                                   bool success, 
                                   double latencyMs) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        DispatchStats& stats = m_stats[endpoint];
        
        if (success) {
            stats.successCount++;
            stats.avgResponseTimeMs = 
                (stats.avgResponseTimeMs * (stats.successCount - 1) + latencyMs) / stats.successCount;
        } else {
            stats.failCount++;
        }
        
        stats.successRate = static_cast<double>(stats.successCount) / 
                          (stats.successCount + stats.failCount);
        
        Logger::debug("PoolDispatcher", "Endpoint {} stats: Success={}, Fail={}, AvgLatency={:.2f}ms",
                     endpoint, stats.successCount, stats.failCount, stats.avgResponseTimeMs);
    }
    
    double PoolDispatcher::getCurrentLatency(const std::string& endpoint) const noexcept {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        if (auto it = m_stats.find(endpoint); it != m_stats.end()) {
            return it->second.avgResponseTimeMs;
        }
        return 0.0;
    }
} // namespace zartrux::dispatcher