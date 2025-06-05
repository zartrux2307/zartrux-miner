#ifndef POOLDISPATCHER_H
#define POOLDISPATCHER_H


#include "MiningModeManager.h"
#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>       // ← IMPORTANTE para json


using json = nlohmann::json;       // Alias directo para comodidad


namespace zartrux::dispatcher {

    enum class Protocol { STRATUM_V1, STRATUM_V2, ETHPROTOCOL_V1 };

    struct DispatchStats {
        uint64_t successCount{0};
        uint64_t failCount{0};
        double avgResponseTimeMs{0.0};
        double successRate{0.0};
    };

    struct PoolConfig {
        std::string url;
        std::string user;
        std::string pass;
        Protocol protocol{Protocol::STRATUM_V2};
    };

    class PoolDispatcher {
    public:
        using DispatchCallback = std::function<void(bool, const std::string&, double)>;

        static PoolDispatcher& instance();

        PoolDispatcher(const PoolDispatcher&) = delete;
        PoolDispatcher& operator=(const PoolDispatcher&) = delete;

        void setMode(::MiningMode mode);
        void setEndpoints(const std::string& iaEndpoint, 
                          const std::string& poolEndpoint,
                          const std::string& poolUser = "",
                          const std::string& poolPass = "");

        void setHybridRatio(double ratio);
        void setRetryPolicy(uint8_t maxRetries, uint16_t retryDelayMs);
        void setTimeout(uint16_t timeoutMs);
        void setSmartThreshold(double threshold);

        void registerDispatchCallback(DispatchCallback callback);
        double getCurrentLatency(const std::string& endpoint) const noexcept;

        bool dispatchValidNonce(const std::string& jobId, uint64_t nonce, 
                                const std::string& resultHash, 
                                const std::string& workerId = "");

        // --- NUEVO: Payload para JSON (útil en protocolos pool/IA) ---
        json createPayload(const std::string& method, const json& params) const {
            return json{
                {"method", method},
                {"params", params},
                {"id", 1}
            };
        }

    private:
        PoolDispatcher();

        bool sendViaHttp(const PoolConfig& endpoint, 
                         const std::string& payload, 
                         double& outLatencyMs);
        std::pair<bool, double> retrySend(const PoolConfig& endpoint, 
                                          const std::string& payload);
        void updateStats(const std::string& endpoint, 
                         bool success, 
                         double latencyMs);
        void notifyCallbacks(bool success, 
                             const std::string& endpoint, 
                             double latencyMs);
        PoolConfig selectTargetEndpoint() const;

        // Para STRATUM/ETH compatibilidad, usa este payload especial
        json createPayload(Protocol protocol,
                           const std::string& jobId, 
                           uint64_t nonce, 
                           const std::string& resultHash, 
                           const std::string& workerId) const;

        std::atomic<::MiningMode> m_currentMode{::MiningMode::POOL};
        PoolConfig m_iaEndpoint;
        PoolConfig m_poolEndpoint;

        mutable std::mutex m_mutex;
        std::vector<DispatchCallback> m_callbacks;

        std::atomic<double> m_hybridRatio{0.5};
        std::atomic<uint8_t> m_maxRetries{3};
        std::atomic<uint16_t> m_retryDelayMs{1000};
        std::atomic<uint16_t> m_timeoutMs{5000};
        std::atomic<double> m_smartThreshold{1.5};

        mutable std::mutex m_statsMutex;
        std::unordered_map<std::string, DispatchStats> m_stats;
    };

} // namespace zartrux::dispatcher

#endif // POOLDISPATCHER_H
