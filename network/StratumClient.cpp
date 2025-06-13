#include "StratumClient.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <functional>

using json = nlohmann::json;

StratumClient::StratumClient(asio::io_context& io_context)
    : m_io_context(io_context), m_socket(io_context), m_resolver(io_context) {}

StratumClient::~StratumClient() {
    disconnect();
}

void StratumClient::connectToPool(const std::string& host, uint16_t port, 
                                const std::string& user, const std::string& pass) {
    if (m_connected) disconnect();
    
    m_host = host;
    m_port = port;
    m_user = user;
    m_pass = pass;
    
    Logger::info("StratumClient", "Resolviendo DNS: " + host);
    m_resolver.async_resolve(m_host, std::to_string(m_port),
        [self = shared_from_this()](auto ec, auto results) {
            self->handle_resolve(ec, results);
        });
}

void StratumClient::disconnect() {
    if (!m_connected) return;
    
    boost::system::error_code ec;
    m_socket.shutdown(tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
    m_connected = false;
    
    if (onDisconnected) onDisconnected();
}

void StratumClient::handle_resolve(const boost::system::error_code& ec, 
                                 tcp::resolver::results_type results) {
    if (ec) {
        if (onError) onError("DNS resolution failed: " + ec.message());
        return;
    }
    
    Logger::info("StratumClient", "Conectando a " + m_host);
    asio::async_connect(m_socket, results,
        [self = shared_from_this()](auto ec, auto) {
            self->handle_connect(ec);
        });
}

void StratumClient::handle_connect(const boost::system::error_code& ec) {
    if (ec) {
        if (onError) onError("Connection failed: " + ec.message());
        return;
    }
    
    m_connected = true;
    Logger::info("StratumClient", "Conexión establecida con " + m_host);
    
    if (onConnected) onConnected();
    read_loop();

    json login_params = {
        {"login", m_user},
        {"pass", m_pass},
        {"agent", "zartrux-miner/1.0"}
    };
    json request = {
        {"id", m_message_id++},
        {"method", "login"},
        {"params", login_params}
    };
    write(request.dump() + "\n");
}

void StratumClient::submit(const std::string& job_id, const std::string& nonce_hex, 
                         const std::string& result_hash) {
    if (!m_connected) {
        Logger::warn("StratumClient", "Intento de submit sin conexión");
        return;
    }
    
    json params = {
        {"id", "1"},
        {"job_id", job_id},
        {"nonce", nonce_hex},
        {"result", result_hash}
    };
    json request = {
        {"id", m_message_id++},
        {"method", "submit"},
        {"params", params}
    };
    write(request.dump() + "\n");
}

void StratumClient::read_loop() {
    asio::async_read_until(m_socket, m_buffer, '\n',
        [self = shared_from_this()](auto ec, auto size) {
            self->handle_read(ec, size);
        });
}

void StratumClient::handle_read(const boost::system::error_code& ec, size_t bytes) {
    if (ec || bytes == 0) {
        if (ec != asio::error::eof) {
            Logger::error("StratumClient", "Read error: " + ec.message());
        }
        disconnect();
        return;
    }
    
    std::istream is(&m_buffer);
    std::string line;
    std::getline(is, line);
    
    if (!line.empty()) {
        Logger::debug("StratumClient", "Recibido: " + line);
        parse_line(line);
    }
    
    read_loop();
}

void StratumClient::parse_line(const std::string& line) {
    try {
        json rpc = json::parse(line);
        
        // Handle new jobs
        if (rpc.contains("method") && rpc["method"] == "job") {
            MiningJob job;
            job.id = rpc["params"].value("job_id", "");
            job.blob = rpc["params"].value("blob", "");
            job.target = rpc["params"].value("target", "");
            
            if (onNewJob) onNewJob(job);
        } 
        // Handle share responses
        else if (rpc.contains("id")) {
            bool accepted = false;
            std::string reason = "Unknown response";
            
            if (rpc.contains("result")) {
                const auto& result = rpc["result"];
                if (result.is_boolean()) {
                    accepted = result.get<bool>();
                } else if (result.is_object()) {
                    accepted = result.value("status", "") == "OK";
                    reason = result.value("reason", "");
                }
            }
            
            if (onShareAccepted) onShareAccepted(accepted, reason);
        }
    } catch(const json::parse_error& e) {
        Logger::warn("StratumClient", "JSON parse error: " + std::string(e.what()));
    }
}

void StratumClient::write(const std::string& message) {
    asio::async_write(m_socket, asio::buffer(message),
        [self = shared_from_this()](auto ec, auto size) {
            self->handle_write(ec, size);
        });
}

void StratumClient::handle_write(const boost::system::error_code& ec, size_t) {
    if (ec) {
        Logger::error("StratumClient", "Write error: " + ec.message());
        disconnect();
    }
}