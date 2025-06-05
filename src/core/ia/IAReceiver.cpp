#include "IAReceiver.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <zmq.h>

using json = nlohmann::json;

// ---- Constructor/Destructor
IAReceiver::IAReceiver(const Config& config)
    : m_config(config)
{
    if (m_config.protocol == IAProtocol::ZMQ) {
        m_zmqContext = zmq_ctx_new();
        if (!m_zmqContext) throw std::runtime_error("ZMQ context creation failed");

        m_zmqSocket = zmq_socket(m_zmqContext, ZMQ_REP);
        if (!m_zmqSocket) {
            zmq_ctx_destroy(m_zmqContext);
            throw std::runtime_error("ZMQ socket creation failed");
        }
        std::string endpoint = "tcp://" + config.ip + ":" + std::to_string(config.port);
        if (zmq_bind(m_zmqSocket, endpoint.c_str()) != 0) {
            zmq_close(m_zmqSocket); zmq_ctx_destroy(m_zmqContext);
            throw std::runtime_error("ZMQ bind failed at " + endpoint);
        }
        zmq_setsockopt(m_zmqSocket, ZMQ_RCVTIMEO, &config.recvTimeoutMs, sizeof(config.recvTimeoutMs));
    } else {
        m_httpSession.SetUrl(cpr::Url{m_config.serverUrl});
        m_httpSession.SetTimeout(cpr::Timeout{m_config.requestTimeoutMs});
    }
}

IAReceiver::~IAReceiver() {
    stop();
    cleanup();
}

// --- ZMQ PUSH MODE: threading/callback
void IAReceiver::start() {
    if (m_config.protocol != IAProtocol::ZMQ) return;
    if (m_running.exchange(true)) return;
    m_thread = std::thread(&IAReceiver::zmqListenLoop, this);
}

void IAReceiver::stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();
}

void IAReceiver::cleanup() {
    if (m_zmqSocket) zmq_close(m_zmqSocket);
    if (m_zmqContext) zmq_ctx_destroy(m_zmqContext);
    m_zmqSocket = nullptr; m_zmqContext = nullptr;
}

void IAReceiver::setNonceCallback(NonceCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(cb);
}

// ---- HTTP Polling (síncrono: fetch/report)
std::vector<uint64_t> IAReceiver::fetchNonces(int count) {
    std::vector<uint64_t> nonces;
    m_stats.fetchAttempts++;
    try {
        json req = {{"count", count}};
        if (!m_config.authToken.empty()) req["auth"] = m_config.authToken;
        auto resp = m_httpSession.Post(cpr::Body{req.dump()}, cpr::Header{{"Content-Type", "application/json"}});
        if (resp.status_code == 200) {
            auto j = json::parse(resp.text);
            if (j.contains("nonces") && j["nonces"].is_array()) {
                nonces = j["nonces"].get<std::vector<uint64_t>>();
                m_stats.fetchSuccesses++;
                m_stats.zmqNoncesRecv += nonces.size();
                if (m_config.verbose) logInfo("Fetched " + std::to_string(nonces.size()) + " nonces (HTTP)");
            }
        }
    } catch (...) { logError("HTTP fetchNonces failed"); }
    return nonces;
}

void IAReceiver::reportResult(uint64_t nonce, bool accepted, const std::string& hash) {
    m_stats.reportAttempts++;
    try {
        json req = {{"nonce", nonce}, {"accepted", accepted}};
        if (!hash.empty()) req["hash"] = hash;
        if (!m_config.authToken.empty()) req["auth"] = m_config.authToken;
        auto resp = m_httpSession.Post(cpr::Body{req.dump()}, cpr::Header{{"Content-Type", "application/json"}});
        if (resp.status_code == 200) {
            m_stats.reportSuccesses++;
            if (m_config.verbose) logInfo("Reported nonce " + std::to_string(nonce) + " result");
        }
    } catch (...) { logError("HTTP reportResult failed"); }
}

IAReceiver::Stats IAReceiver::getStats() const { return m_stats; }

// ---- ZMQ listener loop (Push)
void IAReceiver::zmqListenLoop() {
    while (m_running) {
        char buffer[4096] = {0};
        int received = zmq_recv(m_zmqSocket, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            std::string msg(buffer, received);
            handleJsonMessage(msg);
            zmq_send(m_zmqSocket, "ok", 2, 0);
            m_stats.zmqMessages++;
        } else if (received == -1) {
            m_stats.zmqErrors++;
        }
    }
}

void IAReceiver::handleJsonMessage(const std::string& msg) {
    try {
        auto j = json::parse(msg);
        if (!m_config.authToken.empty()) {
            std::string auth = j.value("auth", "");
            if (auth != m_config.authToken) {
                m_stats.zmqAuthFails++;
                logError("Auth token mismatch");
                return;
            }
        }
        if (!j.contains("nonces") || !j["nonces"].is_array()) {
            logError("Invalid nonce format");
            m_stats.zmqParseErr++;
            return;
        }
        std::vector<uint64_t> nonces = j["nonces"].get<std::vector<uint64_t>>();
        if (m_callback) m_callback(nonces);
        m_stats.zmqNoncesRecv += nonces.size();
        if (m_config.verbose) logInfo("Received " + std::to_string(nonces.size()) + " nonces (ZMQ)");
    } catch (...) {
        m_stats.zmqParseErr++;
        logError("JSON parse error");
    }
}

// ---- Logging
void IAReceiver::logError(const std::string& msg) const {
    if (m_config.verbose) std::cerr << "[IAReceiver][ERROR] " << msg << std::endl;
}
void IAReceiver::logInfo(const std::string& msg) const {
    if (m_config.verbose) std::cout << "[IAReceiver][INFO] " << msg << std::endl;
}

// ---- Singleton
IAReceiver& IAReceiver::instance(const Config& config) {
    static IAReceiver inst(config);
    return inst;
}
