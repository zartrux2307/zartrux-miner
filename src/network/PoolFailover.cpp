#include "PoolFailover.h"
#include "StratumClient.h" 
#include <QDebug>
#include <QTimer>

PoolFailover::PoolFailover(QObject* parent)
    : QObject(parent), m_currentIndex(0), 
      m_client(std::make_unique<StratumClient>(this)),
      m_switching(false), m_handlingFailure(false)
{
    connect(m_client.get(), &StratumClient::connected, 
            this, &PoolFailover::onConnected);
    connect(m_client.get(), &StratumClient::errorOccurred, 
            this, &PoolFailover::onError);
    connect(m_client.get(), &StratumClient::connectionLost, 
            this, &PoolFailover::tryNextPool);
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
    qDebug() << "[PoolFailover] Intentando conectar a pool:" 
             << pool.host << ":" << pool.port;
    m_client->connectToPool(pool.host, pool.port);
}

void PoolFailover::onConnected() {
    const auto& pool = m_pools[m_currentIndex];
    qDebug() << "[PoolFailover] Conectado exitosamente al pool:" 
             << pool.host << ":" << pool.port;
    emit failoverOccurred(pool.host, pool.port);
}

void PoolFailover::onError(const QString& error) {
    if (m_handlingFailure) return;
    m_handlingFailure = true;

    qWarning() << "[PoolFailover] Error en pool actual:" << error;
    auto& pool = m_pools[m_currentIndex];

    if (++pool.retries < 3) {
        qDebug() << "[PoolFailover] Reintentando conexión (" 
                 << pool.retries << "/3)...";
        QTimer::singleShot(2000, [this]() {
            m_handlingFailure = false;
            start();
        });
        return;
    }

    qDebug() << "[PoolFailover] Max reintentos alcanzados";
    pool.retries = 0;
    m_currentIndex = (m_currentIndex + 1) % m_pools.size();

    if (m_currentIndex == 0) {
        emit connectionError("Todos los pools fallaron. Sin conexión.");
        m_handlingFailure = false;
        return;
    }

    tryNextPool();
    m_handlingFailure = false;
}

void PoolFailover::tryNextPool() {
    if (m_switching) return;
    m_switching = true;

    qDebug() << "[PoolFailover] Cambiando a siguiente pool...";
    m_client->disconnect();

    QTimer::singleShot(1000, [this]() {
        if (m_currentIndex >= m_pools.size()) return;
        
        auto& pool = m_pools[m_currentIndex];
        qDebug() << "[PoolFailover] Conectando a nuevo pool:" 
                 << pool.host << ":" << pool.port;
        
        m_client->connectToPool(pool.host, pool.port);
        m_switching = false;
    });
}