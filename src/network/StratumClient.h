#ifndef STRATUMCLIENT_H
#define STRATUMCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>  // ¡IMPORTANTE! Añadido para QJsonArray
#include <QTimer>
#include <QByteArray>

class StratumClient : public QObject {
 Q_OBJECT
public:
    explicit StratumClient(QObject* parent = nullptr);
    ~StratumClient();
    
    void connectToPool(const QString& host, quint16 port);
    void disconnect();
    bool isConnected() const;
    bool hasError() const;
    
    // Métodos específicos de Monero
    void submitShare(const QString& jobId, const QString& nonce, const QString& result);
    QString currentJobId() const;
    
signals:
    void connected();
    void errorOccurred(const QString& error);
    void connectionLost();
    void newJobReceived(const QJsonObject& job);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onTimeout();
    
private:
    void send(const QJsonObject& json);
    void handleResponse(const QJsonObject& response);
    void handleNotification(const QJsonObject& notification);
    void sendSubscribe();
    void sendAuthorize();
    
    QTcpSocket* m_socket;
    QTimer* m_timeoutTimer;
    bool m_connected = false;
    bool m_subscribed = false;
    bool m_authorized = false;
    QString m_currentJobId;
    QString m_extranonce;
    int m_extranonce2Size = 0;
    QString m_user;  // Dirección de monedero XMR
};

#endif // STRATUMCLIENT_H