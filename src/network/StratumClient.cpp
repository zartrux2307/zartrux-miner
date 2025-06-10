#include "StratumClient.h"
#include "utils/Logger.h"

// Usamos el espacio de nombres para Boost.Asio
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace std::placeholders;

StratumClient::StratumClient(asio::io_context& io_context)
    : m_io_context(io_context), m_socket(io_context), m_resolver(io_context) {
    // El constructor ahora es más simple
}

StratumClient::~StratumClient() {
    disconnect();
}

void StratumClient::connectToPool(const std::string& host, uint16_t port) {
    m_host = host;
    m_port = std::to_string(port);

    // Iniciar la resolución de DNS de forma asíncrona
    m_resolver.async_resolve(m_host, m_port,
        std::bind(&StratumClient::handle_resolve, this, _1, _2));
}

void StratumClient::disconnect() {
    boost::system::error_code ec;
    m_socket.shutdown(tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
    if (onDisconnected) {
        onDisconnected();
    }
}

void StratumClient::handle_resolve(const boost::system::error_code& ec,
                                  const tcp::resolver::results_type& endpoints) {
    if (!ec) {
        // Intentar conectar con las direcciones encontradas
        asio::async_connect(m_socket, endpoints,
            std::bind(&StratumClient::handle_connect, this, _1, _2));
    } else {
        if (onError) {
            onError("Error de resolución DNS: " + ec.message());
        }
    }
}

void StratumClient::handle_connect(const boost::system::error_code& ec,
                                 const tcp::endpoint& endpoint) {
    if (!ec) {
        if (onConnected) {
            onConnected();
        }
        // Empezar a leer datos del pool
        read_response();
    } else {
        if (onError) {
            onError("Error de conexión: " + ec.message());
        }
    }
}

void StratumClient::read_response() {
    // Leer hasta encontrar un salto de línea (típico en Stratum)
    asio::async_read_until(m_socket, m_buffer, '\n',
        std::bind(&StratumClient::handle_read, this, _1, _2));
}

void StratumClient::handle_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        // Procesar los datos recibidos
        std::istream response_stream(&m_buffer);
        std::string response_line;
        std::getline(response_stream, response_line);

        // Aquí iría la lógica para procesar el JSON de la respuesta del pool
        Logger::debug("StratumClient", "Recibido: %s", response_line.c_str());

        // Continuar leyendo
        read_response();
    } else if (ec != asio::error::eof) {
        if (onError) {
            onError("Error de lectura: " + ec.message());
        }
    } else {
        // Se cerró la conexión
        if (onDisconnected) {
            onDisconnected();
        }
    }
}