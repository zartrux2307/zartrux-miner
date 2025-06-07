#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVector>
#include <memory>
#include "StratumClient.h"
#include <nlohmann/json.hpp>
//! Mecanismo de failover para pools secundarios de minería XMR.
//! Garantiza conexión continua y tolerante a fallos, reintentos configurables.
//! 100% seguro para entornos de minería críticos (uso real, no de adorno).

class PoolFailover : public QObject {
    Q_OBJECT

public:
    struct PoolEntry {
        QString host;
        quint16 port;
        int retries{0};    // Reintentos antes de saltar al siguiente pool.
    };

    explicit PoolFailover(QObject* parent = nullptr);
    void setPools(const QVector<QPair<QString, quint16>>& pools);
    void start();

signals:
    //! Emitido cuando ocurre failover exitoso (pool activo cambia).
    void failoverOccurred(const QString& host, quint16 port);

    //! Emitido cuando todos los pools han fallado.
    void connectionError(const QString& error);

    //! Opcional: Número de ciclos de failover completados.
    void failoverAttemptsCompleted(int attempts);

private slots:
    void onConnected();
    void onError(const QString& error);

private:
    void tryNextPool();

    QVector<PoolEntry> m_pools;
    int m_currentIndex{0};
    std::unique_ptr<StratumClient> m_client;
};

