#pragma once
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include "core/JobManager.h" // Para MiningJob

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class StratumClient : public std::enable_shared_from_this<StratumClient> {
public:
    explicit StratumClient(asio::io_context& io_context);
    ~StratumClient();

    std::function<void()> onConnected;
    std::function<void(const std::string&)> onError;
    std::function<void()> onDisconnected;
    std::function<void(const MiningJob&)> onNewJob;

    void connectToPool(const std::string& host, uint16_t port, const std::string& user, const std::string& pass);
    void disconnect();
    void submit(const std::string& job_id, const std::string& nonce_hex, const std::string& result_hash);

private:
    void resolve();
    void handle_resolve(const boost::system::error_code&, tcp::resolver::results_type);
    void handle_connect(const boost::system::error_code&);
    void read_loop();
    void handle_read(const boost::system::error_code&, size_t);
    void parse_line(const std::string&);
    void write(const std::string&);
    void handle_write(const boost::system::error_code&, size_t);

    asio::io_context& m_io_context;
    tcp::socket m_socket;
    tcp::resolver m_resolver;
    asio::streambuf m_buffer;
    std::string m_host, m_port, m_user, m_pass;
    std::atomic<uint64_t> m_message_id{1};
};