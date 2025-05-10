#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream>
#include <QRegularExpression> // For parsing
#include <QDebug> // Ensure QDebug is included

// Define system message constants and formats
const QString SYS_MSG_HELLO_FORMAT = QStringLiteral("<SYS_HELLO UUID=\"%1\" NameHint=\"%2\"/>");
const QString SYS_MSG_SESSION_ACCEPTED_FORMAT = QStringLiteral("<SYS_SESSION_ACCEPTED UUID=\"%1\" Name=\"%2\"/>");
const QString SYS_MSG_SESSION_REJECTED_FORMAT = QStringLiteral("<SYS_SESSION_REJECTED Reason=\"%1\"/>"); // New

// Helper function to extract attribute from simple XML-like string
QString extractAttribute(const QString& message, const QString& attributeName) {
    QRegularExpression regex(QStringLiteral("%1=\\\"([^\\\"]*)\\\"").arg(attributeName));
    QRegularExpressionMatch match = regex.match(message);
    if (match.hasMatch() && match.capturedTexts().size() > 1) {
        return match.captured(1);
    }
    return QString();
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
      tcpServer(nullptr),
      udpSocket(nullptr),             // 初始化udpSocket
      defaultPort(60248),
      preferredListenPort(60248),
      autoStartListeningEnabled(true),
      udpDiscoveryEnabled(false),     
      retryListenTimer(nullptr),
      retryListenIntervalMs(15000),
      preferredOutgoingPortNumber(0),
      bindToSpecificOutgoingPort(false),
      localUserUuid(),
      localUserDisplayName()
{
    setupServer(); 
    retryListenTimer = new QTimer(this);
    connect(retryListenTimer, &QTimer::timeout, this, &NetworkManager::attemptToListen);
}

NetworkManager::~NetworkManager()
{
    qDebug() << "NetworkManager::~NetworkManager() - Starting destruction";

    if (retryListenTimer) {
        qDebug() << "NetworkManager::~NetworkManager() - Stopping retryListenTimer";
        retryListenTimer->stop();
        // retryListenTimer is a child of NetworkManager, will be deleted by Qt
    }

    qDebug() << "NetworkManager::~NetworkManager() - Calling stopListening()";
    stopListening(); 
    qDebug() << "NetworkManager::~NetworkManager() - Calling stopUdpDiscovery()";
    stopUdpDiscovery(); 

    // Sockets in pendingIncomingSockets and outgoingSocketsAwaitingSessionAccepted
    // should have been cleaned up (aborted and deleteLater'd) by stopListening()
    // and their respective lists cleared.
    // If they are children of NetworkManager, explicit qDeleteAll here is redundant
    // but harmless if the lists are already empty.
    qDebug() << "NetworkManager::~NetworkManager() - Cleaning up any remaining pendingIncomingSockets (should be empty)";
    qDeleteAll(pendingIncomingSockets);
    pendingIncomingSockets.clear();

    qDebug() << "NetworkManager::~NetworkManager() - Cleaning up any remaining outgoingSocketsAwaitingSessionAccepted (should be empty)";
    qDeleteAll(outgoingSocketsAwaitingSessionAccepted.keys());
    outgoingSocketsAwaitingSessionAccepted.clear();

    if (tcpServer) {
        qDebug() << "NetworkManager::~NetworkManager() - Disconnecting and deleting tcpServer";
        disconnect(tcpServer, nullptr, nullptr, nullptr); // Disconnect signals before deletion
        tcpServer->deleteLater(); 
        tcpServer = nullptr; 
    }
    if (udpSocket) { 
        qDebug() << "NetworkManager::~NetworkManager() - Disconnecting and deleting udpSocket";
        disconnect(udpSocket, nullptr, nullptr, nullptr); // Disconnect signals before deletion
        udpSocket->deleteLater();
        udpSocket = nullptr; 
    }
    qDebug() << "NetworkManager::~NetworkManager() - Destruction finished";
}

void NetworkManager::setupServer()
{
    if (tcpServer) {
        tcpServer->close();
        disconnect(tcpServer, nullptr, nullptr, nullptr);
        tcpServer->deleteLater(); // Schedule for deletion
    }

    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    connect(tcpServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError socketError){
        lastError = tcpServer->errorString();
        emit peerNetworkError("", socketError, lastError);
        emit serverStatusMessage(QString("Server Error: %1").arg(lastError));
    });
}

bool NetworkManager::startListening()
{
    if (!autoStartListeningEnabled) {
        emit serverStatusMessage(tr("Network listening is disabled by user settings."));
        if (tcpServer->isListening()) {
            stopListening();
        } else {
            if (retryListenTimer->isActive()) {
                retryListenTimer->stop();
            }
        }
        return false;
    }

    if (tcpServer->isListening()) {
        emit serverStatusMessage(QString("Server is already listening on port %1.").arg(tcpServer->serverPort()));
        if(retryListenTimer->isActive()) retryListenTimer->stop();
        return true;
    }

    setupServer();

    quint16 portToListen = preferredListenPort > 0 ? preferredListenPort : defaultPort;
    if (portToListen == 0) portToListen = defaultPort;
    if (portToListen == 0) portToListen = 60248;

    if (!tcpServer->listen(QHostAddress::Any, portToListen)) {
        lastError = tcpServer->errorString();
        emit serverStatusMessage(QString("Server could not start on port %1: %2. Will retry automatically if enabled.")
                                     .arg(portToListen).arg(lastError));
        if (autoStartListeningEnabled && !retryListenTimer->isActive()) {
            retryListenTimer->start(retryListenIntervalMs);
            emit serverStatusMessage(tr("Next listen attempt in %1 seconds.").arg(retryListenIntervalMs / 1000));
        }
        return false;
    }

    if(retryListenTimer->isActive()) retryListenTimer->stop();

    emit serverStatusMessage(QString("Server started, listening on port %1.").arg(tcpServer->serverPort()));
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (const QHostAddress &ipAddress : ipAddressesList) {
        if (ipAddress != QHostAddress::LocalHost && ipAddress.toIPv4Address()) {
            emit serverStatusMessage(QString("Accessible IP: %1").arg(ipAddress.toString()));
        }
    }
    return true;
}

void NetworkManager::attemptToListen()
{
    if (autoStartListeningEnabled && !tcpServer->isListening()) {
        emit serverStatusMessage(tr("Retrying to start listener..."));
        startListening();
    } else {
        retryListenTimer->stop();
    }
}

void NetworkManager::stopListening()
{
    if (retryListenTimer && retryListenTimer->isActive()) {
        retryListenTimer->stop();
        emit serverStatusMessage(tr("Automatic listen retry stopped."));
    }

    if (tcpServer && tcpServer->isListening()) {
        tcpServer->close();
        emit serverStatusMessage("Server stopped.");
    }

    QStringList currentPeerUuids = connectedSockets.keys();
    for (const QString& peerUuid : currentPeerUuids) {
        disconnectFromPeer(peerUuid);
    }
    connectedSockets.clear();
    socketToUuidMap.clear();
    peerUuidToNameMap.clear();

    for (QTcpSocket* socket : pendingIncomingSockets) {
        socket->abort();
        socket->deleteLater();
    }
    pendingIncomingSockets.clear();

    for (QTcpSocket* socket : outgoingSocketsAwaitingSessionAccepted.keys()) {
        socket->abort();
        socket->deleteLater();
    }
    outgoingSocketsAwaitingSessionAccepted.clear();
}

QList<QHostAddress> NetworkManager::getLocalIpAddresses() const {
    QList<QHostAddress> ipAddresses;
    const QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : allInterfaces) {
        const QList<QNetworkAddressEntry> allEntries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : allEntries) {
            QHostAddress addr = entry.ip();
            if (!addr.isNull() && (addr.protocol() == QAbstractSocket::IPv4Protocol || addr.protocol() == QAbstractSocket::IPv6Protocol)) {
                ipAddresses.append(addr);
            }
        }
    }
    if (!ipAddresses.contains(QHostAddress(QHostAddress::LocalHost))) {
        ipAddresses.append(QHostAddress(QHostAddress::LocalHost));
    }
    if (!ipAddresses.contains(QHostAddress(QHostAddress::LocalHostIPv6))) {
        ipAddresses.append(QHostAddress(QHostAddress::LocalHostIPv6));
    }
    return ipAddresses;
}

bool NetworkManager::isSelfConnection(const QString& targetHost, quint16 targetPort) const {
    if (!tcpServer || !tcpServer->isListening()) {
        return false;
    }

    quint16 listeningPort = tcpServer->serverPort();
    if (targetPort != listeningPort) {
        return false;
    }

    QHostAddress targetAddress(targetHost);
    if (targetAddress.isNull() && targetHost.compare("localhost", Qt::CaseInsensitive) != 0) {
         qWarning() << "NM::isSelfConnection: Target host" << targetHost << "could not be parsed as a valid IP address or 'localhost'. Assuming not self.";
        return false;
    }

    QList<QHostAddress> localAddresses = getLocalIpAddresses();
    for (const QHostAddress& localAddr : localAddresses) {
        if (targetAddress == localAddr) {
            qDebug() << "NM::isSelfConnection: Target" << targetHost << ":" << targetPort
                     << "matches local listening IP" << localAddr.toString() << ":" << listeningPort;
            return true;
        }
    }
    return false;
}

void NetworkManager::connectToHost(const QString &peerNameToSet, const QString &targetPeerUuidHint, const QString &hostAddress, quint16 port)
{
    Q_UNUSED(targetPeerUuidHint);
    qDebug() << "NM::connectToHost: Attempting to connect to Name:" << peerNameToSet
             << "IP:" << hostAddress << "Port:" << port
             << "My UUID:" << localUserUuid << "My NameHint:" << localUserDisplayName;

    if (isSelfConnection(hostAddress, port)) {
        qWarning() << "NM::connectToHost: Attempt to connect to self (" << hostAddress << ":" << port << ") aborted.";
        emit serverStatusMessage(tr("Attempt to connect to self (%1:%2) was aborted.").arg(hostAddress).arg(port));
        return;
    }

    QTcpSocket *socket = new QTcpSocket(this);

    if (bindToSpecificOutgoingPort && preferredOutgoingPortNumber > 0) {
        if (!socket->bind(QHostAddress::AnyIPv4, preferredOutgoingPortNumber)) {
            emit serverStatusMessage(tr("Warning: Could not bind to outgoing port %1. Error: %2. Proceeding with dynamic port.")
                                     .arg(preferredOutgoingPortNumber).arg(socket->errorString()));
        } else {
            emit serverStatusMessage(tr("Successfully bound to outgoing port %1 for connection to %2.").arg(preferredOutgoingPortNumber).arg(hostAddress));
        }
    }

    outgoingSocketsAwaitingSessionAccepted.insert(socket, peerNameToSet);

    connect(socket, &QTcpSocket::connected, this, &NetworkManager::handleOutgoingSocketConnected);
    connect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleOutgoingSocketDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleOutgoingSocketReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleOutgoingSocketError);

    emit serverStatusMessage(QString("Attempting to connect to %1 (%2:%3)...").arg(peerNameToSet).arg(hostAddress).arg(port));
    socket->connectToHost(hostAddress, port);
}

void NetworkManager::disconnectFromPeer(const QString &peerUuid)
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket) {
        emit serverStatusMessage(tr("Disconnecting from peer %1 (UUID: %2).").arg(peerUuidToNameMap.value(peerUuid, "Unknown")).arg(peerUuid));
        cleanupSocket(socket);
    } else {
        emit serverStatusMessage(tr("Cannot disconnect: Peer UUID %1 not found.").arg(peerUuid));
    }
}

void NetworkManager::sendMessage(const QString &targetPeerUuid, const QString &message)
{
    QTcpSocket *socket = connectedSockets.value(targetPeerUuid, nullptr);
    if (socket && socket->isOpen() && socket->state() == QAbstractSocket::ConnectedState) {
        sendSystemMessage(socket, message);
    } else {
        lastError = tr("Peer %1 not connected or socket invalid.").arg(targetPeerUuid);
        emit serverStatusMessage(tr("Cannot send message to %1: Not connected.").arg(peerUuidToNameMap.value(targetPeerUuid, targetPeerUuid)));
    }
}

QAbstractSocket::SocketState NetworkManager::getPeerSocketState(const QString& peerUuid) const
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket) {
        return socket->state();
    }
    return QAbstractSocket::UnconnectedState;
}

QPair<QString, quint16> NetworkManager::getPeerInfo(const QString& peerUuid) const
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket) {
        return qMakePair(peerUuidToNameMap.value(peerUuid, socket->peerAddress().toString()), socket->peerPort());
    }
    return qMakePair(QString(), 0);
}

QString NetworkManager::getPeerIpAddress(const QString& peerUuid) const
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket) {
        return socket->peerAddress().toString();
    }
    return QString();
}

QStringList NetworkManager::getConnectedPeerUuids() const
{
    return connectedSockets.keys();
}

QString NetworkManager::getLastError() const
{
    return lastError;
}

void NetworkManager::setLocalUserDetails(const QString& uuid, const QString& displayName)
{
    this->localUserUuid = uuid;
    this->localUserDisplayName = displayName;
}

void NetworkManager::setListenPreferences(quint16 port, bool autoStartListen)
{
    preferredListenPort = (port > 0) ? port : defaultPort;
    bool oldAutoStartListeningEnabled = autoStartListeningEnabled;
    autoStartListeningEnabled = autoStartListen;

    if (!autoStartListeningEnabled) {
        if (tcpServer && tcpServer->isListening()) {
            stopListening();
        } else if (retryListenTimer && retryListenTimer->isActive()) {
            retryListenTimer->stop();
            emit serverStatusMessage(tr("Network listening disabled by user. Retry stopped."));
        }
    } else if (autoStartListeningEnabled && !oldAutoStartListeningEnabled && (!tcpServer || !tcpServer->isListening())) {
        emit serverStatusMessage(tr("Network listening enabled. Will attempt to start if not already running."));
    }
}

void NetworkManager::setOutgoingConnectionPreferences(quint16 port, bool useSpecific)
{
    preferredOutgoingPortNumber = port;
    bindToSpecificOutgoingPort = useSpecific;
    qDebug() << "NM::setOutgoingConnectionPreferences: Preferred Outgoing Port:" << preferredOutgoingPortNumber
             << "Use Specific:" << bindToSpecificOutgoingPort;
    emit serverStatusMessage(tr("Outgoing connection port preferences updated. Port: %1, Specific: %2")
                             .arg(port == 0 ? tr("Dynamic") : QString::number(port))
                             .arg(useSpecific ? tr("Yes") : tr("No")));
}

void NetworkManager::setUdpDiscoveryPreferences(bool enabled)
{
    if (udpDiscoveryEnabled == enabled) return;

    udpDiscoveryEnabled = enabled;
    if (udpDiscoveryEnabled) {
        startUdpDiscovery();
    } else {
        stopUdpDiscovery();
    }
}

void NetworkManager::startUdpDiscovery()
{
    if (!udpDiscoveryEnabled) {
        emit serverStatusMessage(tr("UDP discovery is disabled by user settings."));
        return;
    }

    if (udpSocket && udpSocket->state() != QAbstractSocket::UnconnectedState) {
        emit serverStatusMessage(tr("UDP discovery is already active on port %1.").arg(DISCOVERY_UDP_PORT));
        return;
    }

    if (udpSocket) { 
        qDebug() << "NM::startUdpDiscovery: Cleaning up old udpSocket.";
        disconnect(udpSocket, nullptr, nullptr, nullptr);
        udpSocket->close();
        udpSocket->deleteLater();
        udpSocket = nullptr; 
    }

    udpSocket = new QUdpSocket(this);
    connect(udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::processPendingUdpDatagrams);
    connect(udpSocket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError){
        Q_UNUSED(socketError);
        if(udpSocket) {
            emit serverStatusMessage(tr("UDP Socket Error: %1").arg(udpSocket->errorString()));
        } else {
            emit serverStatusMessage(tr("UDP Socket Error on an already deleted socket."));
        }
    });

    if (udpSocket->bind(QHostAddress::AnyIPv4, DISCOVERY_UDP_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit serverStatusMessage(tr("UDP discovery started, listening on port %1.").arg(DISCOVERY_UDP_PORT));
        sendUdpBroadcast();
    } else {
        emit serverStatusMessage(tr("UDP discovery could not start on port %1: %2").arg(DISCOVERY_UDP_PORT).arg(udpSocket->errorString()));
        if (udpSocket) {
            udpSocket->deleteLater();
            udpSocket = nullptr;
        }
    }
}

void NetworkManager::stopUdpDiscovery()
{
    if (udpSocket) {
        qDebug() << "NM::stopUdpDiscovery: Stopping UDP discovery. Current state:" << udpSocket->state();
        if (udpSocket->state() != QAbstractSocket::UnconnectedState) {
            udpSocket->close();
            emit serverStatusMessage(tr("UDP discovery stopped."));
        }
    } else {
        qDebug() << "NM::stopUdpDiscovery: udpSocket is already null.";
    }
}

void NetworkManager::triggerManualUdpBroadcast()
{
    if (!udpDiscoveryEnabled) {
        emit serverStatusMessage(tr("Cannot send manual broadcast: UDP discovery is disabled."));
        return;
    }
    if (!udpSocket || udpSocket->state() == QAbstractSocket::UnconnectedState) {
        emit serverStatusMessage(tr("Cannot send manual broadcast: UDP socket not ready. Try enabling discovery first."));
        return;
    }
    emit serverStatusMessage(tr("Sending manual UDP discovery broadcast..."));
    sendUdpBroadcast();
}

void NetworkManager::sendUdpBroadcast()
{
    if (!udpSocket || !udpDiscoveryEnabled || localUserUuid.isEmpty() || !tcpServer || !tcpServer->isListening()) {
        if (udpDiscoveryEnabled && (!tcpServer || !tcpServer->isListening())) {
             qDebug() << "NM::sendUdpBroadcast: Skipping broadcast because TCP server is not listening.";
        }
        return;
    }

    QString message = QString("%1;UUID=%2;Name=%3;TCPPort=%4;")
                          .arg(UDP_DISCOVERY_MSG_PREFIX)
                          .arg(localUserUuid)
                          .arg(localUserDisplayName)
                          .arg(tcpServer->serverPort());

    QByteArray datagram = message.toUtf8();
    qint64 bytesSent = udpSocket->writeDatagram(datagram, QHostAddress::Broadcast, DISCOVERY_UDP_PORT);

    if (bytesSent == -1) {
        emit serverStatusMessage(tr("UDP broadcast failed: %1").arg(udpSocket->errorString()));
    } else {
        qDebug() << "NM::sendUdpBroadcast: Sent" << bytesSent << "bytes:" << message;
    }
}

void NetworkManager::processPendingUdpDatagrams()
{
    if (!udpSocket) return;

    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        QString message = QString::fromUtf8(datagram);
        qDebug() << "NM::processPendingUdpDatagrams: Received from" << senderAddress.toString() << ":" << senderPort << "Data:" << message;

        if (!message.startsWith(UDP_DISCOVERY_MSG_PREFIX)) {
            qDebug() << "NM::processPendingUdpDatagrams: Ignoring non-discovery message:" << message;
            continue;
        }

        QStringList parts = message.split(';', Qt::SkipEmptyParts);
        if (parts.length() < 4 || parts[0] != UDP_DISCOVERY_MSG_PREFIX) { 
            qWarning() << "NM::processPendingUdpDatagrams: Malformed or non-matching prefix discovery message:" << message;
            continue;
        }

        QString peerUuid;
        QString peerNameHint;
        quint16 peerTcpPort = 0;

        for (const QString& part : parts) {
            if (part.startsWith("UUID=")) {
                peerUuid = part.mid(4);
            } else if (part.startsWith("Name=")) {
                peerNameHint = part.mid(5);
            } else if (part.startsWith("TCPPort=")) {
                bool ok;
                peerTcpPort = part.mid(8).toUShort(&ok);
                if (!ok) {
                    qWarning() << "NM::processPendingUdpDatagrams: Invalid TCPPort in discovery message:" << message;
                    peerTcpPort = 0;
                }
            }
        }

        if (peerUuid.isEmpty() || peerTcpPort == 0) {
            qWarning() << "NM::processPendingUdpDatagrams: Missing UUID or TCPPort in discovery message:" << message;
            continue;
        }

        if (peerUuid == localUserUuid) {
            qDebug() << "NM::processPendingUdpDatagrams: Ignoring own broadcast.";
            continue;
        }

        if (connectedSockets.contains(peerUuid)) {
            qDebug() << "NM::processPendingUdpDatagrams: Peer" << peerUuid << "is already connected. Ignoring discovery.";
            continue;
        }
        
        emit serverStatusMessage(tr("UDP Discovery: Found peer %1 (UUID: %2) at %3, TCP Port: %4. Attempting TCP connection.")
                                 .arg(peerNameHint)
                                 .arg(peerUuid)
                                 .arg(senderAddress.toString())
                                 .arg(peerTcpPort));
        
        connectToHost(peerNameHint.isEmpty() ? peerUuid : peerNameHint, peerUuid, senderAddress.toString(), peerTcpPort);
    }
}

void NetworkManager::onNewConnection()
{
    while (tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = tcpServer->nextPendingConnection();
        if (socket) {
            emit serverStatusMessage(tr("Pending connection from %1:%2. Waiting for HELLO.")
                                     .arg(socket->peerAddress().toString())
                                     .arg(socket->peerPort()));
            pendingIncomingSockets.append(socket);
            connect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
            connect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handlePendingIncomingSocketDisconnected);
            connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handlePendingIncomingSocketError);
        }
    }
}

void NetworkManager::handleClientSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerUuid = socketToUuidMap.value(socket);
    if (!peerUuid.isEmpty()) {
        emit serverStatusMessage(tr("Peer %1 (UUID: %2) disconnected.")
                                 .arg(peerUuidToNameMap.value(peerUuid, "Unknown"))
                                 .arg(peerUuid));
        emit peerDisconnected(peerUuid);
    }
    cleanupSocket(socket);
}

void NetworkManager::handleClientSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !socket->isValid() || socket->bytesAvailable() == 0) return;

    QString peerUuid = socketToUuidMap.value(socket);
    if (peerUuid.isEmpty()) {
        emit serverStatusMessage(tr("Error: Data from unknown connected socket. Closing."));
        cleanupSocket(socket, false);
        return;
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    while(socket->bytesAvailable() > 0) {
        if (socket->bytesAvailable() < (int)sizeof(quint32))
            return;

        in.startTransaction();
        QString message;
        in >> message;

        if (in.commitTransaction()) {
            emit newMessageReceived(peerUuid, message);
        } else {
            break;
        }
    }
}

void NetworkManager::handleClientSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerUuid = socketToUuidMap.value(socket);
    QString errorString = socket->errorString();
    lastError = errorString;

    if (!peerUuid.isEmpty()) {
        emit serverStatusMessage(tr("Network error with peer %1 (UUID: %2): %3")
                                 .arg(peerUuidToNameMap.value(peerUuid, "Unknown"))
                                 .arg(peerUuid)
                                 .arg(errorString));
        emit peerNetworkError(peerUuid, socketError, errorString);
    } else {
        emit serverStatusMessage(tr("Network error on unmapped socket: %1").arg(errorString));
    }
}

void NetworkManager::handlePendingIncomingSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !socket->isValid() || socket->bytesAvailable() == 0) return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    if (socket->bytesAvailable() < (int)sizeof(quint32)) return;

    in.startTransaction();
    QString message;
    in >> message;

    if (in.commitTransaction()) {
        qDebug() << "NM::PendingIncomingSocketReadyRead: Received message:" << message << "from" << socket->peerAddress().toString();
        if (message.startsWith("<SYS_HELLO")) {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerNameHint = extractAttribute(message, "NameHint");
            qDebug() << "NM::PendingIncomingSocketReadyRead: Extracted peerUUID:" << peerUuid << "NameHint:" << peerNameHint;
            qDebug() << "NM::PendingIncomingSocketReadyRead: Local user UUID for comparison:" << localUserUuid;

            if (peerUuid.isEmpty() || peerUuid == localUserUuid) {
                qWarning() << "NM::PendingIncomingSocketReadyRead: Invalid HELLO - peerUUID is empty or matches localUserUuid. PeerUUID:" << peerUuid << "LocalUUID:" << localUserUuid;
                emit serverStatusMessage(tr("Error: Received HELLO from %1 without valid UUID or self-connect. Rejecting.")
                                         .arg(socket->peerAddress().toString()));
                sendSystemMessage(socket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid HELLO"));
                removePendingIncomingSocket(socket);
                socket->abort();
                return;
            }
             if (connectedSockets.contains(peerUuid)) {
                qWarning() << "NM::PendingIncomingSocketReadyRead: Peer" << peerUuid << "is already connected. Rejecting new session.";
                emit serverStatusMessage(tr("Peer %1 (UUID: %2) is already connected. Rejecting new session attempt.")
                                         .arg(peerNameHint).arg(peerUuid));
                sendSystemMessage(socket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Already connected"));
                removePendingIncomingSocket(socket);
                socket->abort();
                return;
            }


            emit serverStatusMessage(tr("Received HELLO from %1 (UUID: %2, Hint: %3).")
                                     .arg(socket->peerAddress().toString())
                                     .arg(peerUuid)
                                     .arg(peerNameHint));
            qDebug() << "NM::PendingIncomingSocketReadyRead: Emitting incomingSessionRequest for UUID:" << peerUuid;

            disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
            emit incomingSessionRequest(socket, socket->peerAddress().toString(), socket->peerPort(), peerUuid, peerNameHint);
        } else {
            qWarning() << "NM::PendingIncomingSocketReadyRead: Expected HELLO, got:" << message.left(50) << "from" << socket->peerAddress().toString();
            emit serverStatusMessage(tr("Error: Expected HELLO from %1, got: %2. Closing.")
                                     .arg(socket->peerAddress().toString()).arg(message.left(50)));
            removePendingIncomingSocket(socket);
            socket->abort();
        }
    }
}

void NetworkManager::handlePendingIncomingSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    emit serverStatusMessage(tr("Pending connection from %1 disconnected before session establishment.")
                             .arg(socket->peerAddress().toString()));
    removePendingIncomingSocket(socket);
    socket->deleteLater();
}

void NetworkManager::handlePendingIncomingSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    Q_UNUSED(socketError);
    emit serverStatusMessage(tr("Error on pending connection from %1: %2")
                             .arg(socket->peerAddress().toString())
                             .arg(socket->errorString()));
    removePendingIncomingSocket(socket);
    socket->deleteLater();
}

void NetworkManager::handleOutgoingSocketConnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerNameToSet = outgoingSocketsAwaitingSessionAccepted.value(socket, "Unknown Peer");
    qDebug() << "NM::OutgoingSocketConnected: TCP link with" << peerNameToSet << "established. Sending HELLO. My UUID:" << localUserUuid << "My Name:" << localUserDisplayName;
    emit serverStatusMessage(tr("TCP link established with %1 (%2:%3). Sending HELLO...")
                             .arg(peerNameToSet)
                             .arg(socket->peerAddress().toString())
                             .arg(socket->peerPort()));

    QString helloMessage = SYS_MSG_HELLO_FORMAT.arg(localUserUuid).arg(localUserDisplayName);
    sendSystemMessage(socket, helloMessage);
}

void NetworkManager::handleOutgoingSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !outgoingSocketsAwaitingSessionAccepted.contains(socket)) return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    if (socket->bytesAvailable() < (int)sizeof(quint32)) return;

    in.startTransaction();
    QString message;
    in >> message;

    if (in.commitTransaction()) {
        QString localNameForPeerAttempt = outgoingSocketsAwaitingSessionAccepted.value(socket, "Unknown");
        qDebug() << "NM::OutgoingSocketReadyRead: Received message:" << message << "from attempted peer:" << localNameForPeerAttempt;
        if (message.startsWith("<SYS_SESSION_ACCEPTED")) {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerReportedName = extractAttribute(message, "Name");
            qDebug() << "NM::OutgoingSocketReadyRead: SESSION_ACCEPTED. PeerUUID:" << peerUuid << "PeerReportedName:" << peerReportedName;
            qDebug() << "NM::OutgoingSocketReadyRead: Local user UUID for comparison:" << localUserUuid;

            if (peerUuid.isEmpty() || peerUuid == localUserUuid) {
                qWarning() << "NM::OutgoingSocketReadyRead: Invalid SESSION_ACCEPTED - peerUUID is empty or matches localUserUuid. PeerUUID:" << peerUuid << "LocalUUID:" << localUserUuid;
                emit serverStatusMessage(tr("Error: Received SESSION_ACCEPTED from %1 without valid UUID or self-connect. Disconnecting.")
                                         .arg(localNameForPeerAttempt));
                emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Invalid SESSION_ACCEPTED (UUID error)"));
                removeOutgoingSocketAwaitingAcceptance(socket);
                socket->abort();
                return;
            }
            if (connectedSockets.contains(peerUuid)) {
                 qWarning() << "NM::OutgoingSocketReadyRead: SESSION_ACCEPTED for already connected peer (race condition?). PeerUUID:" << peerUuid;
                 emit serverStatusMessage(tr("Error: Peer %1 (UUID: %2) is already connected (race condition?). Ignoring new session acceptance.")
                                          .arg(localNameForPeerAttempt).arg(peerUuid));
                 emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Peer already connected (race condition)"));
                 removeOutgoingSocketAwaitingAcceptance(socket);
                 socket->abort();
                 return;
            }

            emit serverStatusMessage(tr("Session with %1 (UUID: %2, Reported Name: %3) accepted by peer.")
                                     .arg(localNameForPeerAttempt).arg(peerUuid).arg(peerReportedName));
            qDebug() << "NM::OutgoingSocketReadyRead: Session accepted. Adding to established connections. PeerUUID:" << peerUuid << "LocalName:" << localNameForPeerAttempt;

            removeOutgoingSocketAwaitingAcceptance(socket);
            addEstablishedConnection(socket, peerUuid, localNameForPeerAttempt, socket->peerAddress().toString(), socket->peerPort());

        } else if (message.startsWith("<SYS_SESSION_REJECTED")) {
            QString reason = extractAttribute(message, "Reason");
            qWarning() << "NM::OutgoingSocketReadyRead: Session REJECTED by peer. Peer:" << localNameForPeerAttempt << "Reason:" << reason;
            emit serverStatusMessage(tr("Session with %1 rejected by peer. Reason: %2")
                                     .arg(localNameForPeerAttempt)
                                     .arg(reason.isEmpty() ? "No reason given" : reason));
            emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Session rejected by peer: %1").arg(reason.isEmpty() ? tr("No reason given") : reason));
            removeOutgoingSocketAwaitingAcceptance(socket);
            socket->abort();
        } else {
            qWarning() << "NM::OutgoingSocketReadyRead: Expected SESSION_ACCEPTED/REJECTED, got:" << message.left(50) << "from" << localNameForPeerAttempt;
            emit serverStatusMessage(tr("Expected SESSION_ACCEPTED/REJECTED from %1, got: %2. Disconnecting.")
                                     .arg(localNameForPeerAttempt)
                                     .arg(message.left(50)));
            emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Invalid response from peer"));
            removeOutgoingSocketAwaitingAcceptance(socket);
            socket->abort();
        }
    }
}

void NetworkManager::handleOutgoingSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    if (outgoingSocketsAwaitingSessionAccepted.contains(socket)) {
        QString peerName = outgoingSocketsAwaitingSessionAccepted.value(socket, "Unknown Peer");
        emit serverStatusMessage(tr("Outgoing connection to %1 failed or disconnected before session established.")
                                 .arg(peerName));
        emit outgoingConnectionFailed(peerName, tr("Disconnected before session established"));
        removeOutgoingSocketAwaitingAcceptance(socket);
    }
    socket->deleteLater();
}

void NetworkManager::handleOutgoingSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    Q_UNUSED(socketError);
    if (outgoingSocketsAwaitingSessionAccepted.contains(socket)) {
        QString peerName = outgoingSocketsAwaitingSessionAccepted.value(socket, "Unknown Peer");
        QString errorString = socket->errorString();
        emit serverStatusMessage(tr("Error on outgoing connection to %1: %2")
                                 .arg(peerName)
                                 .arg(errorString));
        emit outgoingConnectionFailed(peerName, errorString);
    }
}

void NetworkManager::acceptIncomingSession(QTcpSocket* tempSocket, const QString& peerUuid, const QString& localNameForPeer)
{
    qDebug() << "NM::acceptIncomingSession: Attempting to accept session for PeerUUID:" << peerUuid << "LocalName:" << localNameForPeer << "My UUID:" << localUserUuid;
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket)) {
        qWarning() << "NM::acceptIncomingSession: Socket not found or not pending.";
        emit serverStatusMessage(tr("Error: Cannot accept session, socket not found or not pending."));
        if(tempSocket) tempSocket->deleteLater();
        return;
    }
    if (peerUuid.isEmpty() || peerUuid == localUserUuid) {
        qWarning() << "NM::acceptIncomingSession: Invalid peer UUID for acceptance. PeerUUID:" << peerUuid;
        emit serverStatusMessage(tr("Error: Cannot accept session, invalid peer UUID."));
        sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid UUID"));
        removePendingIncomingSocket(tempSocket);
        tempSocket->abort();
        return;
    }
    if (connectedSockets.contains(peerUuid)) {
        qWarning() << "NM::acceptIncomingSession: PeerUUID" << peerUuid << "already connected. Rejecting duplicate.";
        emit serverStatusMessage(tr("Error: Peer with UUID %1 is already connected. Rejecting duplicate session.").arg(peerUuid));
        sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Already connected"));
        removePendingIncomingSocket(tempSocket);
        tempSocket->abort();
        return;
    }


    removePendingIncomingSocket(tempSocket);
    qDebug() << "NM::acceptIncomingSession: Sending SESSION_ACCEPTED. My UUID:" << localUserUuid << "My Name:" << localUserDisplayName;

    QString acceptedMessage = SYS_MSG_SESSION_ACCEPTED_FORMAT.arg(localUserUuid).arg(localUserDisplayName);
    sendSystemMessage(tempSocket, acceptedMessage);

    addEstablishedConnection(tempSocket, peerUuid, localNameForPeer, tempSocket->peerAddress().toString(), tempSocket->peerPort());
    emit serverStatusMessage(tr("Session with %1 (UUID: %2) accepted. Sent session acceptance.")
                             .arg(localNameForPeer).arg(peerUuid));
}

void NetworkManager::rejectIncomingSession(QTcpSocket* tempSocket)
{
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket)) {
        qWarning() << "NM::rejectIncomingSession: Socket not found or not pending.";
        emit serverStatusMessage(tr("Error: Cannot reject session, socket not found or not pending."));
        if(tempSocket) tempSocket->deleteLater();
        return;
    }
    qDebug() << "NM::rejectIncomingSession: Rejecting session from" << tempSocket->peerAddress().toString();
    emit serverStatusMessage(tr("Incoming session from %1 rejected by user.")
                             .arg(tempSocket->peerAddress().toString()));
    sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Rejected by user"));
    removePendingIncomingSocket(tempSocket);
    tempSocket->abort();
}

void NetworkManager::cleanupSocket(QTcpSocket* socket, bool removeFromConnected)
{
    if (!socket) return;
    qDebug() << "NM::cleanupSocket: Cleaning up socket for peer" << socketToUuidMap.value(socket, "N/A")
             << "Addr:" << socket->peerAddress().toString() << ":" << socket->peerPort()
             << "State:" << socket->state();

    // Disconnect all signals from this socket to prevent calls to slots on this NetworkManager
    // instance or other objects that might be in the process of destruction.
    disconnect(socket, nullptr, nullptr, nullptr); // Crucial

    if (socket->isOpen()) {
        socket->abort(); // Forcefully close, don't wait for graceful close
    }

    QString peerUuid = socketToUuidMap.value(socket);
    if (!peerUuid.isEmpty()) {
        if (removeFromConnected) {
            connectedSockets.remove(peerUuid);
        }
        // Only remove from socketToUuidMap if it's the correct UUID for this socket.
        // This check is mostly for sanity, as value() would return the mapped UUID.
        if (socketToUuidMap.value(socket) == peerUuid) {
             socketToUuidMap.remove(socket);
        }
        peerUuidToNameMap.remove(peerUuid);
    }
    
    // Remove from other lists/maps if it could be there.
    // These lists should ideally be managed such that a socket isn't in multiple
    // "pending" type states simultaneously.
    pendingIncomingSockets.removeAll(socket);
    if (outgoingSocketsAwaitingSessionAccepted.contains(socket)) {
        outgoingSocketsAwaitingSessionAccepted.remove(socket);
    }

    socket->deleteLater(); // Schedule for deletion
    qDebug() << "NM::cleanupSocket: Socket scheduled for deletion.";
}

void NetworkManager::sendSystemMessage(QTcpSocket* socket, const QString& sysMessage)
{
    if (socket && socket->isOpen() && socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_5);
        out << sysMessage;
        socket->write(block);
        socket->flush();
    }
}

void NetworkManager::addEstablishedConnection(QTcpSocket* socket, const QString& peerUuid, const QString& peerName, const QString& peerAddress, quint16 peerPort)
{
    if (!socket || peerUuid.isEmpty()) {
        qWarning() << "NM::addEstablishedConnection: Invalid socket or empty peerUUID. Cannot add connection.";
        return;
    }
    qDebug() << "NM::addEstablishedConnection: Establishing connection for PeerUUID:" << peerUuid << "Name:" << peerName << "Addr:" << peerAddress << ":" << peerPort;

    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handlePendingIncomingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handlePendingIncomingSocketError);

    disconnect(socket, &QTcpSocket::connected, this, &NetworkManager::handleOutgoingSocketConnected);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleOutgoingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleOutgoingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleOutgoingSocketError);

    connect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleClientSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleClientSocketDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleClientSocketError);

    connectedSockets.insert(peerUuid, socket);
    socketToUuidMap.insert(socket, peerUuid);
    peerUuidToNameMap.insert(peerUuid, peerName);

    emit peerConnected(peerUuid, peerName, peerAddress, peerPort);
}

void NetworkManager::removePendingIncomingSocket(QTcpSocket* socket)
{
    if (!socket) return;
    pendingIncomingSockets.removeOne(socket);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handlePendingIncomingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handlePendingIncomingSocketError);
}

void NetworkManager::removeOutgoingSocketAwaitingAcceptance(QTcpSocket* socket)
{
    if (!socket) return;
    outgoingSocketsAwaitingSessionAccepted.remove(socket);
    disconnect(socket, &QTcpSocket::connected, this, &NetworkManager::handleOutgoingSocketConnected);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleOutgoingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleOutgoingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleOutgoingSocketError);
}
