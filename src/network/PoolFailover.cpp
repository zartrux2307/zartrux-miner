#include "PoolFailover.h"
#include <QDebug>
#include <QTimer>

PoolFailover::PoolFailover(QObject* parent)
    : QObject(parent), m_currentIndex(0), m_client(std::make_unique<StratumClient>(this))
{
    // Conecta señales críticas del cliente Stratum a los slots de PoolFailover
    connect(m_client.get(), &StratumClient::connected, this, &PoolFailover::onConnected);
    connect(m_client.get(), &StratumClient::errorOccurred, this, &PoolFailover::onError);
    connect(m_client.get(), &StratumClient::connectionLost, this, &PoolFailover::tryNextPool);
}

void PoolFailover::setPools(const QVector<QPair<QString, quint16>>& pools) {
    m_pools.clear();
    for (const auto& pool : pools) {
        m_pools.push_back(PoolEntry{pool.first, pool.second, 0});
    }
    m_currentIndex = 0;
}

void PoolFailover::start() {
    if (m_pools.isEmpty()) {
        emit connectionError("Lista de pools vacía.");
        return;
    }
    auto& pool = m_pools[m_currentIndex];
    qDebug() << "[PoolFailover] Intentando conectar a pool:" << pool.host << ":" << pool.port;
    m_client->connectToPool(pool.host, pool.port);
}

void PoolFailover::onConnected() {
    const auto& pool = m_pools[m_currentIndex];
    qDebug() << "[PoolFailover] Conectado exitosamente al pool:" << pool.host << ":" << pool.port;
    emit failoverOccurred(pool.host, pool.port);
}

void PoolFailover::onError(const QString& error) {
    qWarning() << "[PoolFailover] Error en pool actual:" << error;
    auto& pool = m_pools[m_currentIndex];

    if (++pool.retries < 3) { // 3 reintentos por pool antes de saltar
        qDebug() << "[PoolFailover] Reintentando conexión con el mismo pool (" << pool.retries << "/3 ) ...";
        QTimer::singleShot(1000, [this]() { start(); });
        return;
    }

    qDebug() << "[PoolFailover] Max reintentos alcanzados, probando siguiente pool...";
    pool.retries = 0; // Reset reintentos cuando vuelva a este pool
    m_currentIndex = (m_currentIndex + 1) % m_pools.size();

    // Si damos la vuelta completa → todos los pools fallaron
    if (m_currentIndex == 0) {
        emit connectionError("Todos los pools fallaron. El minero está sin conexión.");
        return;
    }

    tryNextPool();
}

void PoolFailover::tryNextPool() {
    auto& pool = m_pools[m_currentIndex];
    qDebug() << "[PoolFailover] Conectando con siguiente pool:" << pool.host << ":" << pool.port;
    m_client->disconnect();

    // Espera breve antes de intentar el siguiente (anti-flood)
    QTimer::singleShot(500, [this, pool]() {
        m_client->connectToPool(pool.host, pool.port);
    });
}
