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

void StratumClient::connectToPool(const std::string& host, uint16_t port, const std::string& user, const std::string& pass) {
    m_host = host;
    m_port = std::to_string(port);
    m_user = user;
    m_pass = pass;
    
    m_resolver.async_resolve(m_host, m_port,
        std::bind(&StratumClient::handle_resolve, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void StratumClient::disconnect() {
    boost::system::error_code ec;
    m_socket.shutdown(tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

void StratumClient::handle_resolve(const boost::system::error_code& ec, tcp::resolver::results_type results) {
    if (!ec) {
        asio::async_connect(m_socket, results,
            std::bind(&StratumClient::handle_connect, shared_from_this(), std::placeholders::_1));
    } else {
        if (onError) onError("DNS resolve error: " + ec.message());
    }
}

void StratumClient::handle_connect(const boost::system::error_code& ec) {
    if (!ec) {
        if (onConnected) onConnected();
        read_loop();
        json login_params = { {"login", m_user}, {"pass", m_pass}, {"agent", "zartrux-miner/1.0"} };
        json request = { {"id", m_message_id++}, {"method", "login"}, {"params", login_params} };
        write(request.dump() + "\n");
    } else {
        if (onError) onError("Connect error: " + ec.message());
    }
}

void StratumClient::submit(const std::string& job_id, const std::string& nonce_hex, const std::string& result_hash) {
    json params = { {"id", "1"}, {"job_id", job_id}, {"nonce", nonce_hex}, {"result", result_hash} };
    json request = { {"id", m_message_id++}, {"method", "submit"}, {"params", params} };
    write(request.dump() + "\n");
}

void StratumClient::read_loop() {
    asio::async_read_until(m_socket, m_buffer, '\n',
        std::bind(&StratumClient::handle_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void StratumClient::handle_read(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (!ec && bytes_transferred > 0) {
        std::istream response_stream(&m_buffer);
        std::string line;
        std::getline(response_stream, line);
        parse_line(line);
        read_loop();
    } else {
        if (ec != asio::error::eof && onError) onError("Read error: " + ec.message());
        if (onDisconnected) onDisconnected();
    }
}

void StratumClient::parse_line(const std::string& line) {
    try {
        json rpc = json::parse(line);
        if (rpc.contains("method") && rpc["method"] == "job") {
            const auto& params = rpc["params"];
            MiningJob job;
            job.id = params.value("job_id", "");
            job.blob = params.value("blob", "");
            job.target = params.value("target", "");
            if (onNewJob) onNewJob(job);
        } else if (rpc.contains("result") && rpc["result"].contains("status") && rpc["result"]["status"] == "OK") {
            if (onShareAccepted) onShareAccepted(true);
        }
    } catch (const json::parse_error& e) {
        Logger::warn("StratumClient", "JSON parse error: %s", e.what());
    }
}

void StratumClient::write(const std::string& message) {
    asio::async_write(m_socket, asio::buffer(message),
        std::bind(&StratumClient::handle_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void StratumClient::handle_write(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (ec) {
        if (onError) onError("Write error: " + ec.message());
    }
}