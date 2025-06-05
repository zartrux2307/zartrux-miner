#include "StratumClient.h"
#include <QDebug>
#include <QHostAddress>
#include <QThread>
#include <QCoreApplication>
#include <QJsonArray>
#include <thread>
#include <chrono>

StratumClient::StratumClient(QObject* parent)
    : QObject(parent), m_socket(new QTcpSocket(this)),
      m_timeoutTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &StratumClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &StratumClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &StratumClient::onReadyRead);

    // Compatibilidad con todas las versiones de Qt
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onError(QAbstractSocket::SocketError)));
#else
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &StratumClient::onError);
#endif

    m_timeoutTimer->setInterval(10000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &StratumClient::onTimeout);
}

StratumClient::~StratumClient() {
    disconnect();
}

void StratumClient::connectToPool(const QString& host, quint16 port) {
    if (m_connected) return;

    m_socket->abort();
    m_socket->connectToHost(host, port);
    m_timeoutTimer->start();
    qDebug() << "Conectando a pool XMR:" << host << ":" << port;
}

void StratumClient::disconnect() {
    m_socket->disconnectFromHost();
    m_connected = false;
    m_subscribed = false;
    m_authorized = false;
    m_timeoutTimer->stop();
}

bool StratumClient::isConnected() const {
    return m_connected;
}

bool StratumClient::hasError() const {
    return m_socket->error() != QAbstractSocket::UnknownSocketError;
}

QString StratumClient::currentJobId() const {
    return m_currentJobId;
}

void StratumClient::submitShare(const QString& jobId, const QString& nonce, const QString& result) {
    QJsonObject params;
    params["id"] = jobId;
    params["job_id"] = jobId;
    params["nonce"] = nonce;
    params["result"] = result;

    QJsonObject json;
    json["id"] = 4;
    json["method"] = "submit";
    json["params"] = params;

    send(json);
}

// --- Slots privados ---
void StratumClient::onConnected() {
    m_connected = true;
    m_timeoutTimer->stop();
    qDebug() << "Conexión establecida con pool XMR";

    // Protocolo inicial para Monero
    sendSubscribe();
    sendAuthorize();

    emit connected();
}

void StratumClient::onDisconnected() {
    m_connected = false;
    m_subscribed = false;
    m_authorized = false;
    qDebug() << "Desconectado del pool XMR";
    emit connectionLost();
}

void StratumClient::onReadyRead() {
    while (m_socket->canReadLine()) {
        QByteArray data = m_socket->readLine();
        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (!doc.isObject()) {
            qWarning() << "Respuesta JSON inválida:" << data;
            continue;
        }

        QJsonObject json = doc.object();

        if (json.contains("result") || json.contains("error")) {
            handleResponse(json);
        } else if (json.contains("method")) {
            handleNotification(json);
        } else {
            qWarning() << "Mensaje desconocido:" << json;
        }
    }
}

void StratumClient::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    QString errorMsg = m_socket->errorString();
    qWarning() << "Error de conexión XMR:" << errorMsg;
    emit errorOccurred(errorMsg);
}

void StratumClient::onTimeout() {
    qWarning() << "Timeout de conexión con pool XMR";
    m_socket->abort();
    emit errorOccurred("Timeout de conexión");
}

// --- Métodos privados ---
void StratumClient::send(const QJsonObject& json) {
    QJsonDocument doc(json);
    m_socket->write(doc.toJson(QJsonDocument::Compact) + '\n');
}

void StratumClient::sendSubscribe() {
    QJsonObject json;
    json["id"] = 1;
    json["method"] = "mining.subscribe";
    json["params"] = QJsonArray();

    send(json);
}

void StratumClient::sendAuthorize() {
    QJsonObject json;
    json["id"] = 2;
    json["method"] = "mining.authorize";

    QJsonArray params;
    params.append(m_user);
    params.append("x");

    json["params"] = params;
    send(json);
}

void StratumClient::handleResponse(const QJsonObject& response) {
    int id = response["id"].toInt();

    if (id == 1) {
        if (!response["result"].isNull()) {
            m_subscribed = true;
            qDebug() << "Subscripción exitosa a pool XMR";
        } else {
            qWarning() << "Error en subscripción:" << response["error"].toObject();
        }
    }
    else if (id == 2) {
        if (response["result"].toBool()) {
            m_authorized = true;
            qDebug() << "Autorización exitosa en pool XMR";
        } else {
            qWarning() << "Error en autorización:" << response["error"].toObject();
        }
    }
    else if (id == 4) {
        if (response["result"].toBool()) {
            qDebug() << "Share aceptado por el pool";
        } else {
            qWarning() << "Share rechazado:" << response["error"].toObject();
        }
    }
}

void StratumClient::handleNotification(const QJsonObject& notification) {
    QString method = notification["method"].toString();

    if (method == "job") {
        QJsonObject params = notification["params"].toObject();
        m_currentJobId = params["job_id"].toString();

        QString blob = params["blob"].toString();
        QString target = params["target"].toString();
        QString algo = params["algo"].toString();

        emit newJobReceived(params);
        qDebug() << "Nuevo trabajo recibido para XMR:" << m_currentJobId;
    }
}
