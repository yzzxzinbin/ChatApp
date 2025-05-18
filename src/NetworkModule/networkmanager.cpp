#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream>
#include <QRegularExpression> // For parsing
#include <QDebug>             // Ensure QDebug is included

// Helper function to extract attribute from simple XML-like string
QString extractAttribute(const QString &message, const QString &attributeName)
{
    QRegularExpression regex(QStringLiteral("%1=\\\"([^\\\"]*)\\\"").arg(attributeName));
    QRegularExpressionMatch match = regex.match(message);
    if (match.hasMatch() && match.capturedTexts().size() > 1)
    {
        return match.captured(1);
    }
    return QString();
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
      tcpServer(nullptr),
      udpDiscoveryListenerSocket(nullptr),
      udpBroadcastSenderSocket(nullptr),
      udpTemporaryResponseListenerSocket(nullptr),
      udpResponseListenerTimer(nullptr),
      defaultPort(60248),
      preferredListenPort(60248),
      autoStartListeningEnabled(true),
      udpDiscoveryEnabled(false),
      currentUdpDiscoveryPort(60249),                                      // Default UDP port
      udpContinuousBroadcastEnabled(true),                                 // Added: Default to true
      udpBroadcastIntervalSeconds(DEFAULT_UDP_BROADCAST_INTERVAL_SECONDS), // Added: Use default
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

    udpBroadcastTimer = new QTimer(this); // Initialize broadcast timer
    connect(udpBroadcastTimer, &QTimer::timeout, this, &NetworkManager::sendUdpBroadcast);

    udpResponseListenerTimer = new QTimer(this);
    udpResponseListenerTimer->setSingleShot(true);                                                                // New init
    connect(udpResponseListenerTimer, &QTimer::timeout, this, &NetworkManager::handleUdpResponseListenerTimeout); // New init
}

NetworkManager::~NetworkManager()
{
    qDebug() << "NetworkManager::~NetworkManager() - Starting destruction";

    if (retryListenTimer)
    {
        qDebug() << "NetworkManager::~NetworkManager() - Stopping retryListenTimer";
        retryListenTimer->stop();
    }
    if (udpBroadcastTimer)
    { // Stop broadcast timer
        qDebug() << "NetworkManager::~NetworkManager() - Stopping udpBroadcastTimer";
        udpBroadcastTimer->stop();
    }
    if (udpResponseListenerTimer)
    { // Stop response listener timer
        qDebug() << "NetworkManager::~NetworkManager() - Stopping udpResponseListenerTimer";
        udpResponseListenerTimer->stop();
    }

    qDebug() << "NetworkManager::~NetworkManager() - Calling stopListening()";
    stopListening();
    qDebug() << "NetworkManager::~NetworkManager() - Calling stopUdpDiscovery()";
    stopUdpDiscovery();

    qDebug() << "NetworkManager::~NetworkManager() - Cleaning up any remaining pendingIncomingSockets (should be empty)";
    qDeleteAll(pendingIncomingSockets);
    pendingIncomingSockets.clear();

    qDebug() << "NetworkManager::~NetworkManager() - Cleaning up any remaining outgoingSocketsAwaitingSessionAccepted (should be empty)";
    qDeleteAll(outgoingSocketsAwaitingSessionAccepted.keys());
    outgoingSocketsAwaitingSessionAccepted.clear();

    if (tcpServer)
    {
        qDebug() << "NetworkManager::~NetworkManager() - Disconnecting and deleting tcpServer";
        disconnect(tcpServer, nullptr, nullptr, nullptr); // Disconnect signals before deletion
        tcpServer->deleteLater();
        tcpServer = nullptr;
    }
    // Clean up UDP sockets
    if (udpDiscoveryListenerSocket)
    {
        qDebug() << "NetworkManager::~NetworkManager() - Disconnecting and deleting udpDiscoveryListenerSocket";
        disconnect(udpDiscoveryListenerSocket, nullptr, nullptr, nullptr);
        udpDiscoveryListenerSocket->deleteLater();
        udpDiscoveryListenerSocket = nullptr;
    }
    if (udpBroadcastSenderSocket)
    {
        qDebug() << "NetworkManager::~NetworkManager() - Disconnecting and deleting udpBroadcastSenderSocket";
        disconnect(udpBroadcastSenderSocket, nullptr, nullptr, nullptr);
        udpBroadcastSenderSocket->deleteLater();
        udpBroadcastSenderSocket = nullptr;
    }

    cleanupTemporaryUdpResponseListener(); // New: Cleanup temporary UDP listener

    qDebug() << "NetworkManager::~NetworkManager() - Destruction finished";
}

void NetworkManager::setupServer()
{
    if (tcpServer)
    {
        tcpServer->close();
        disconnect(tcpServer, nullptr, nullptr, nullptr);
        tcpServer->deleteLater(); // Schedule for deletion
    }

    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    connect(tcpServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError socketError)
            {
        lastError = tcpServer->errorString();
        emit peerNetworkError("", socketError, lastError);
        emit serverStatusMessage(QString("Server Error: %1").arg(lastError)); });
}

bool NetworkManager::startListening()
{
    if (!autoStartListeningEnabled)
    {
        emit serverStatusMessage(tr("Network listening is disabled by user settings."));
        if (tcpServer->isListening())
        {
            stopListening();
        }
        else
        {
            if (retryListenTimer->isActive())
            {
                retryListenTimer->stop();
            }
        }
        return false;
    }

    if (tcpServer->isListening())
    {
        emit serverStatusMessage(QString("Server is already listening on port %1.").arg(tcpServer->serverPort()));
        if (retryListenTimer->isActive())
            retryListenTimer->stop();
        return true;
    }

    setupServer();

    quint16 portToListen = preferredListenPort > 0 ? preferredListenPort : defaultPort;
    if (portToListen == 0)
        portToListen = defaultPort;
    if (portToListen == 0)
        portToListen = 60248;

    if (!tcpServer->listen(QHostAddress::Any, portToListen))
    {
        lastError = tcpServer->errorString();
        emit serverStatusMessage(QString("Server could not start on port %1: %2. Will retry automatically if enabled.")
                                     .arg(portToListen)
                                     .arg(lastError));
        if (autoStartListeningEnabled && !retryListenTimer->isActive())
        {
            retryListenTimer->start(retryListenIntervalMs);
            emit serverStatusMessage(tr("Next listen attempt in %1 seconds.").arg(retryListenIntervalMs / 1000));
        }
        return false;
    }

    if (retryListenTimer->isActive())
        retryListenTimer->stop();

    emit serverStatusMessage(QString("Server started, listening on port %1.").arg(tcpServer->serverPort()));
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (const QHostAddress &ipAddress : ipAddressesList)
    {
        if (ipAddress != QHostAddress::LocalHost && ipAddress.toIPv4Address())
        {
            emit serverStatusMessage(QString("Accessible IP: %1").arg(ipAddress.toString()));
        }
    }
    return true;
}

void NetworkManager::attemptToListen()
{
    if (autoStartListeningEnabled && !tcpServer->isListening())
    {
        emit serverStatusMessage(tr("Retrying to start listener..."));
        startListening();
    }
    else
    {
        retryListenTimer->stop();
    }
}

void NetworkManager::stopListening()
{
    if (retryListenTimer && retryListenTimer->isActive())
    {
        retryListenTimer->stop();
        emit serverStatusMessage(tr("Automatic listen retry stopped."));
    }

    if (tcpServer && tcpServer->isListening())
    {
        tcpServer->close();
        emit serverStatusMessage("Server stopped.");
    }

    QStringList currentPeerUuids = connectedSockets.keys();
    for (const QString &peerUuid : currentPeerUuids)
    {
        disconnectFromPeer(peerUuid);
    }
    connectedSockets.clear();
    socketToUuidMap.clear();
    peerUuidToNameMap.clear();

    for (QTcpSocket *socket : pendingIncomingSockets)
    {
        socket->abort();
        socket->deleteLater();
    }
    pendingIncomingSockets.clear();

    for (QTcpSocket *socket : outgoingSocketsAwaitingSessionAccepted.keys())
    {
        socket->abort();
        socket->deleteLater();
    }
    outgoingSocketsAwaitingSessionAccepted.clear();
}

QList<QHostAddress> NetworkManager::getLocalIpAddresses() const
{
    QList<QHostAddress> ipAddresses;
    const QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : allInterfaces)
    {
        const QList<QNetworkAddressEntry> allEntries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : allEntries)
        {
            QHostAddress addr = entry.ip();
            if (!addr.isNull() && (addr.protocol() == QAbstractSocket::IPv4Protocol || addr.protocol() == QAbstractSocket::IPv6Protocol))
            {
                ipAddresses.append(addr);
            }
        }
    }
    if (!ipAddresses.contains(QHostAddress(QHostAddress::LocalHost)))
    {
        ipAddresses.append(QHostAddress(QHostAddress::LocalHost));
    }
    if (!ipAddresses.contains(QHostAddress(QHostAddress::LocalHostIPv6)))
    {
        ipAddresses.append(QHostAddress(QHostAddress::LocalHostIPv6));
    }
    return ipAddresses;
}

bool NetworkManager::isSelfConnection(const QString &targetHost, quint16 targetPort) const
{
    if (!tcpServer || !tcpServer->isListening())
    {
        return false;
    }

    quint16 listeningPort = tcpServer->serverPort();
    if (targetPort != listeningPort)
    {
        return false;
    }

    QHostAddress targetAddress(targetHost);
    if (targetAddress.isNull() && targetHost.compare("localhost", Qt::CaseInsensitive) != 0)
    {
        qWarning() << "NM::isSelfConnection: Target host" << targetHost << "could not be parsed as a valid IP address or 'localhost'. Assuming not self.";
        return false;
    }

    QList<QHostAddress> localAddresses = getLocalIpAddresses();
    for (const QHostAddress &localAddr : localAddresses)
    {
        if (targetAddress == localAddr)
        {
            qDebug() << "NM::isSelfConnection: Target" << targetHost << ":" << targetPort
                     << "matches local listening IP" << localAddr.toString() << ":" << listeningPort;
            return true;
        }
    }
    return false;
}

void NetworkManager::connectToHost(const QString &peerNameToSet, const QString &targetPeerUuidHint, const QString &hostAddress, quint16 port)
{
    qDebug() << "NM::connectToHost: Attempting to connect to Name:" << peerNameToSet
             << "IP:" << hostAddress << "Port:" << port
             << "My UUID:" << localUserUuid << "My NameHint:" << localUserDisplayName
             << "Target UUID Hint:" << targetPeerUuidHint;

    if (isSelfConnection(hostAddress, port))
    {
        qWarning() << "NM::connectToHost: Attempt to connect to self (" << hostAddress << ":" << port << ") aborted.";
        emit serverStatusMessage(tr("Attempt to connect to self (%1:%2) was aborted.").arg(hostAddress).arg(port));
        return;
    }

    if (!targetPeerUuidHint.isEmpty() && connectedSockets.contains(targetPeerUuidHint))
    {
        qWarning() << "NM::connectToHost: Attempt to connect to already connected peer UUID" << targetPeerUuidHint << ". Aborting.";
        emit serverStatusMessage(tr("Peer %1 (UUID: %2) is already connected. Connection attempt aborted.").arg(peerNameToSet).arg(targetPeerUuidHint));
        return;
    }

    if (!targetPeerUuidHint.isEmpty())
    {
        for (const QPair<QString, QString> &attemptDetails : outgoingSocketsAwaitingSessionAccepted.values())
        {
            if (attemptDetails.second == targetPeerUuidHint)
            {
                qWarning() << "NM::connectToHost: Outgoing connection attempt already in progress for UUID" << targetPeerUuidHint << ". Aborting new attempt.";
                emit serverStatusMessage(tr("Outgoing connection to %1 (UUID: %2) already in progress. New attempt aborted.").arg(peerNameToSet).arg(targetPeerUuidHint));
                return;
            }
        }
    }

    QTcpSocket *socket = new QTcpSocket(this);

    if (bindToSpecificOutgoingPort && preferredOutgoingPortNumber > 0)
    {
        if (!socket->bind(QHostAddress::AnyIPv4, preferredOutgoingPortNumber))
        {
            emit serverStatusMessage(tr("Warning: Could not bind to outgoing port %1. Error: %2. Proceeding with dynamic port.")
                                         .arg(preferredOutgoingPortNumber)
                                         .arg(socket->errorString()));
        }
        else
        {
            emit serverStatusMessage(tr("Successfully bound to outgoing port %1 for connection to %2.").arg(preferredOutgoingPortNumber).arg(hostAddress));
        }
    }

    outgoingSocketsAwaitingSessionAccepted.insert(socket, qMakePair(peerNameToSet, targetPeerUuidHint));

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
    if (socket)
    {
        emit serverStatusMessage(tr("Disconnecting from peer %1 (UUID: %2).").arg(peerUuidToNameMap.value(peerUuid, "Unknown")).arg(peerUuid));
        cleanupSocket(socket);
    }
    else
    {
        emit serverStatusMessage(tr("Cannot disconnect: Peer UUID %1 not found.").arg(peerUuid));
    }
}

void NetworkManager::sendMessage(const QString &targetPeerUuid, const QString &message)
{
    QTcpSocket *socket = connectedSockets.value(targetPeerUuid, nullptr);
    if (socket && socket->isOpen() && socket->state() == QAbstractSocket::ConnectedState)
    {
        sendSystemMessage(socket, message);
    }
    else
    {
        lastError = tr("Peer %1 not connected or socket invalid.").arg(targetPeerUuid);
        emit serverStatusMessage(tr("Cannot send message to %1: Not connected.").arg(peerUuidToNameMap.value(targetPeerUuid, targetPeerUuid)));
    }
}

QAbstractSocket::SocketState NetworkManager::getPeerSocketState(const QString &peerUuid) const
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket)
    {
        return socket->state();
    }
    return QAbstractSocket::UnconnectedState;
}

QPair<QString, quint16> NetworkManager::getPeerInfo(const QString &peerUuid) const
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket)
    {
        return qMakePair(peerUuidToNameMap.value(peerUuid, socket->peerAddress().toString()), socket->peerPort());
    }
    return qMakePair(QString(), 0);
}

QString NetworkManager::getPeerIpAddress(const QString &peerUuid) const
{
    QTcpSocket *socket = connectedSockets.value(peerUuid, nullptr);
    if (socket)
    {
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

void NetworkManager::setLocalUserDetails(const QString &uuid, const QString &displayName)
{
    this->localUserUuid = uuid;
    this->localUserDisplayName = displayName;
}

void NetworkManager::setListenPreferences(quint16 port, bool autoStartListen)
{
    bool portActuallyChanged = (preferredListenPort != port && port > 0);
    quint16 oldPreferredListenPort = preferredListenPort;
    preferredListenPort = (port > 0) ? port : defaultPort;
    // Ensure preferredListenPort is not 0 if it's supposed to be a valid port.
    // If defaultPort itself could be 0, assign a fallback.
    if (preferredListenPort == 0 && defaultPort == 0)
        preferredListenPort = 60248;
    else if (preferredListenPort == 0)
        preferredListenPort = defaultPort;

    bool oldAutoStartListeningEnabled = autoStartListeningEnabled;
    autoStartListeningEnabled = autoStartListen;

    qDebug() << "NM::setListenPreferences: New Port:" << preferredListenPort << "(Old:" << oldPreferredListenPort << ", Changed:" << portActuallyChanged << ")"
             << "New AutoStart:" << autoStartListeningEnabled << "(Old:" << oldAutoStartListeningEnabled << ")";

    if (!autoStartListeningEnabled)
    { // If listening is being disabled or kept disabled
        if (tcpServer && tcpServer->isListening())
        {
            qDebug() << "NM::setListenPreferences: AutoStart disabled, server is listening. Stopping server.";
            stopListening(); // This also stops the retry timer
        }
        else if (retryListenTimer && retryListenTimer->isActive())
        {
            qDebug() << "NM::setListenPreferences: AutoStart disabled, retry timer active. Stopping timer.";
            retryListenTimer->stop();
            emit serverStatusMessage(tr("Network listening disabled. Retry timer stopped."));
        }
        else
        {
            qDebug() << "NM::setListenPreferences: AutoStart remains disabled. No server/timer action.";
            emit serverStatusMessage(tr("Network listening is disabled in settings."));
        }
    }
    else
    { // autoStartListeningEnabled is true (listening is being enabled or kept enabled)
        bool serverIsCurrentlyListening = (tcpServer && tcpServer->isListening());
        quint16 currentListeningPort = serverIsCurrentlyListening ? tcpServer->serverPort() : 0;

        if (!oldAutoStartListeningEnabled)
        { // Auto-start was just turned ON
            if (!serverIsCurrentlyListening)
            {
                qDebug() << "NM::setListenPreferences: AutoStart just enabled, server not running. Attempting to start.";
                startListening(); // This will use the new preferredListenPort
            }
            else
            { // Server is already running, but auto-start was off. Now it's on.
                if (portActuallyChanged && currentListeningPort != preferredListenPort)
                {
                    qDebug() << "NM::setListenPreferences: AutoStart just enabled, server running on old port, port changed. Restarting.";
                    emit serverStatusMessage(tr("Port changed from %1 to %2. Restarting listener...").arg(currentListeningPort).arg(preferredListenPort));
                    stopListening();
                    startListening();
                }
                else
                {
                    qDebug() << "NM::setListenPreferences: AutoStart just enabled, server already running on correct port %1.", currentListeningPort;
                    emit serverStatusMessage(tr("Network listening enabled. Server already running on port %1.").arg(currentListeningPort));
                }
            }
        }
        else
        { // Auto-start was already ON and remains ON
            if (serverIsCurrentlyListening)
            {
                if (portActuallyChanged && currentListeningPort != preferredListenPort)
                {
                    qDebug() << "NM::setListenPreferences: AutoStart ON, server running, port changed. Restarting.";
                    emit serverStatusMessage(tr("Port changed from %1 to %2. Restarting listener...").arg(currentListeningPort).arg(preferredListenPort));
                    stopListening();
                    startListening();
                }
                else
                {
                    qDebug() << "NM::setListenPreferences: AutoStart ON, server running on correct port %1. No port change.", currentListeningPort;
                }
            }
            else
            { // Auto-start is ON, but server is not running (e.g., previous attempts failed or was stopped manually)
                qDebug() << "NM::setListenPreferences: AutoStart ON, server not running. Attempting to start.";
                startListening();
            }
        }
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

void NetworkManager::onNewConnection()
{
    while (tcpServer->hasPendingConnections())
    {
        QTcpSocket *socket = tcpServer->nextPendingConnection();
        if (socket)
        {
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
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QString peerUuid = socketToUuidMap.value(socket);
    if (!peerUuid.isEmpty())
    {
        emit serverStatusMessage(tr("Peer %1 (UUID: %2) disconnected.")
                                     .arg(peerUuidToNameMap.value(peerUuid, "Unknown"))
                                     .arg(peerUuid));
        emit peerDisconnected(peerUuid);
    }
    cleanupSocket(socket);
}

void NetworkManager::handleClientSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !socket->isValid() || socket->bytesAvailable() == 0)
        return;
    qInfo() << "[NEW RECEIVE] TCP data received from peer" << socketToUuidMap.value(socket, "Unknown")
            << "IP:" << socket->peerAddress().toString() << "Port:" << socket->peerPort()
            << "Bytes available:" << socket->bytesAvailable();

    QString peerUuid = socketToUuidMap.value(socket);
    if (peerUuid.isEmpty())
    {
        emit serverStatusMessage(tr("Error: Data from unknown connected socket. Closing."));
        cleanupSocket(socket, false);
        return;
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    while (socket->bytesAvailable() > 0)
    {
        if (socket->bytesAvailable() < (int)sizeof(quint32))
            return;

        in.startTransaction();
        QString message;
        in >> message;

        if (in.commitTransaction())
        {
            emit newMessageReceived(peerUuid, message);
        }
        else
        {
            break;
        }
    }
}

void NetworkManager::handleClientSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QString peerUuid = socketToUuidMap.value(socket);
    QString errorString = socket->errorString();
    lastError = errorString;

    if (!peerUuid.isEmpty())
    {
        emit serverStatusMessage(tr("Network error with peer %1 (UUID: %2): %3")
                                     .arg(peerUuidToNameMap.value(peerUuid, "Unknown"))
                                     .arg(peerUuid)
                                     .arg(errorString));
        emit peerNetworkError(peerUuid, socketError, errorString);
    }
    else
    {
        emit serverStatusMessage(tr("Network error on unmapped socket: %1").arg(errorString));
    }
}

void NetworkManager::handlePendingIncomingSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !socket->isValid() || socket->bytesAvailable() == 0)
        return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    if (socket->bytesAvailable() < (int)sizeof(quint32))
        return;

    in.startTransaction();
    QString message;
    in >> message;

    if (in.commitTransaction())
    {
        qDebug() << "NM::PendingIncomingSocketReadyRead: Received message:" << message << "from" << socket->peerAddress().toString();
        if (message.startsWith("<SYS_HELLO"))
        {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerNameHint = extractAttribute(message, "NameHint");
            qDebug() << "NM::PendingIncomingSocketReadyRead: Extracted peerUUID:" << peerUuid << "NameHint:" << peerNameHint;
            qDebug() << "NM::PendingIncomingSocketReadyRead: Local user UUID for comparison:" << localUserUuid;

            if (peerUuid.isEmpty() || peerUuid == localUserUuid)
            {
                qWarning() << "NM::PendingIncomingSocketReadyRead: Invalid HELLO - peerUUID is empty or matches localUserUuid. PeerUUID:" << peerUuid << "LocalUUID:" << localUserUuid;
                emit serverStatusMessage(tr("Error: Received HELLO from %1 without valid UUID or self-connect. Rejecting.")
                                             .arg(socket->peerAddress().toString()));
                sendSystemMessage(socket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid HELLO"));
                removePendingIncomingSocket(socket);
                socket->abort();
                return;
            }
            if (connectedSockets.contains(peerUuid))
            {
                qWarning() << "NM::PendingIncomingSocketReadyRead: Peer" << peerUuid << "is already connected. Rejecting new session.";
                emit serverStatusMessage(tr("Peer %1 (UUID: %2) is already connected. Rejecting new session attempt.")
                                             .arg(peerNameHint)
                                             .arg(peerUuid));
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
        }
        else
        {
            qWarning() << "NM::PendingIncomingSocketReadyRead: Expected HELLO, got:" << message.left(50) << "from" << socket->peerAddress().toString();
            emit serverStatusMessage(tr("Error: Expected HELLO from %1, got: %2. Closing.")
                                         .arg(socket->peerAddress().toString())
                                         .arg(message.left(50)));
            removePendingIncomingSocket(socket);
            socket->abort();
        }
    }
}

void NetworkManager::handlePendingIncomingSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    emit serverStatusMessage(tr("Pending connection from %1 disconnected before session establishment.")
                                 .arg(socket->peerAddress().toString()));
    removePendingIncomingSocket(socket);
    socket->deleteLater();
}

void NetworkManager::handlePendingIncomingSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    Q_UNUSED(socketError);
    emit serverStatusMessage(tr("Error on pending connection from %1: %2")
                                 .arg(socket->peerAddress().toString())
                                 .arg(socket->errorString()));
    removePendingIncomingSocket(socket);
    socket->deleteLater();
}

void NetworkManager::handleOutgoingSocketConnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QString peerNameToSet = outgoingSocketsAwaitingSessionAccepted.value(socket).first;
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
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !outgoingSocketsAwaitingSessionAccepted.contains(socket))
        return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    if (socket->bytesAvailable() < (int)sizeof(quint32))
        return;

    in.startTransaction();
    QString message;
    in >> message;

    if (in.commitTransaction())
    {
        QString localNameForPeerAttempt = outgoingSocketsAwaitingSessionAccepted.value(socket).first;
        QString targetPeerUuidHint = outgoingSocketsAwaitingSessionAccepted.value(socket).second; // Get the UUID hint

        qDebug() << "NM::OutgoingSocketReadyRead: Received message:" << message << "from attempted peer:" << localNameForPeerAttempt << "UUID Hint:" << targetPeerUuidHint;
        if (message.startsWith("<SYS_SESSION_ACCEPTED"))
        {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerName = extractAttribute(message, "Name");

            if (peerUuid.isEmpty())
            {
                qWarning() << "NM::OutgoingSocketReadyRead: SESSION_ACCEPTED from" << localNameForPeerAttempt << "missing UUID. Closing.";
                emit serverStatusMessage(tr("Error: SESSION_ACCEPTED from %1 missing UUID. Closing connection.").arg(localNameForPeerAttempt));
                outgoingSocketsAwaitingSessionAccepted.remove(socket);
                socket->abort();
                return;
            }
            // Optional: Verify if peerUuid matches targetPeerUuidHint if hint was provided
            if (!targetPeerUuidHint.isEmpty() && peerUuid != targetPeerUuidHint)
            {
                qWarning() << "NM::OutgoingSocketReadyRead: SESSION_ACCEPTED UUID" << peerUuid << "does not match Hint" << targetPeerUuidHint << "from" << localNameForPeerAttempt << ".Proceeding with received UUID.";
                // Decide if this is an error or just a mismatch to log. For now, proceed.
            }

            emit serverStatusMessage(tr("Session accepted by %1 (UUID: %2). Connection established.")
                                         .arg(peerName.isEmpty() ? localNameForPeerAttempt : peerName)
                                         .arg(peerUuid));

            // Transition socket from pending to connected
            outgoingSocketsAwaitingSessionAccepted.remove(socket);
            // Corrected function call:
            addEstablishedConnection(socket, peerUuid,
                                     peerName.isEmpty() ? localNameForPeerAttempt : peerName,
                                     socket->peerAddress().toString(), socket->peerPort());
            emit peerConnected(peerUuid, peerName.isEmpty() ? localNameForPeerAttempt : peerName, socket->peerAddress().toString(), socket->peerPort());
        }
        else if (message.startsWith("<SYS_SESSION_REJECTED"))
        {
            QString reason = extractAttribute(message, "Reason");
            qWarning() << "NM::OutgoingSocketReadyRead: Session rejected by" << localNameForPeerAttempt << "Reason:" << reason;
            emit serverStatusMessage(tr("Session rejected by %1. Reason: %2")
                                         .arg(localNameForPeerAttempt)
                                         .arg(reason.isEmpty() ? tr("Unknown") : reason));
            outgoingSocketsAwaitingSessionAccepted.remove(socket);
            socket->abort(); // This will trigger handleOutgoingSocketDisconnected
        }
        else
        {
            qWarning() << "NM::OutgoingSocketReadyRead: Expected SESSION_ACCEPTED or REJECTED from" << localNameForPeerAttempt << "got:" << message.left(50);
            emit serverStatusMessage(tr("Error: Unexpected response from %1: %2. Closing.")
                                         .arg(localNameForPeerAttempt)
                                         .arg(message.left(50)));
            outgoingSocketsAwaitingSessionAccepted.remove(socket);
            socket->abort();
        }
    }
}

void NetworkManager::handleOutgoingSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    if (outgoingSocketsAwaitingSessionAccepted.contains(socket))
    {
        QString peerName = outgoingSocketsAwaitingSessionAccepted.value(socket).first;
        emit serverStatusMessage(tr("Outgoing connection to %1 failed or disconnected before session established.")
                                     .arg(peerName));
        emit outgoingConnectionFailed(peerName, tr("Disconnected before session established"));
        removeOutgoingSocketAwaitingAcceptance(socket);
    }
    socket->deleteLater();
}

void NetworkManager::handleOutgoingSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    Q_UNUSED(socketError);
    if (outgoingSocketsAwaitingSessionAccepted.contains(socket))
    {
        QString peerName = outgoingSocketsAwaitingSessionAccepted.value(socket).first;
        QString errorString = socket->errorString();
        emit serverStatusMessage(tr("Error on outgoing connection to %1: %2")
                                     .arg(peerName)
                                     .arg(errorString));
        emit outgoingConnectionFailed(peerName, errorString);
    }
}

void NetworkManager::acceptIncomingSession(QTcpSocket *tempSocket, const QString &peerUuid, const QString &localNameForPeer)
{
    qDebug() << "NM::acceptIncomingSession: Attempting to accept session for PeerUUID:" << peerUuid << "LocalName:" << localNameForPeer << "My UUID:" << localUserUuid;
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket))
    {
        qWarning() << "NM::acceptIncomingSession: Socket not found or not pending.";
        emit serverStatusMessage(tr("Error: Cannot accept session, socket not found or not pending."));
        if (tempSocket)
            tempSocket->deleteLater();
        return;
    }
    if (peerUuid.isEmpty() || peerUuid == localUserUuid)
    {
        qWarning() << "NM::acceptIncomingSession: Invalid peer UUID for acceptance. PeerUUID:" << peerUuid;
        emit serverStatusMessage(tr("Error: Cannot accept session, invalid peer UUID."));
        sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid UUID"));
        removePendingIncomingSocket(tempSocket);
        tempSocket->abort();
        return;
    }
    if (connectedSockets.contains(peerUuid))
    {
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
                                 .arg(localNameForPeer)
                                 .arg(peerUuid));
}

void NetworkManager::rejectIncomingSession(QTcpSocket *tempSocket)
{
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket))
    {
        qWarning() << "NM::rejectIncomingSession: Socket not found or not pending.";
        emit serverStatusMessage(tr("Error: Cannot reject session, socket not found or not pending."));
        if (tempSocket)
            tempSocket->deleteLater();
        return;
    }
    qDebug() << "NM::rejectIncomingSession: Rejecting session from" << tempSocket->peerAddress().toString();
    emit serverStatusMessage(tr("Incoming session from %1 rejected by user.")
                                 .arg(tempSocket->peerAddress().toString()));
    sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Rejected by user"));
    removePendingIncomingSocket(tempSocket);
    tempSocket->abort();
}

void NetworkManager::cleanupSocket(QTcpSocket *socket, bool removeFromConnected)
{
    if (!socket)
        return;
    qDebug() << "NM::cleanupSocket: Cleaning up socket for peer" << socketToUuidMap.value(socket, "N/A")
             << "Addr:" << socket->peerAddress().toString() << ":" << socket->peerPort()
             << "State:" << socket->state();

    // Disconnect all signals from this socket to prevent calls to slots on this NetworkManager
    // instance or other objects that might be in the process of destruction.
    disconnect(socket, nullptr, nullptr, nullptr); // Crucial

    if (socket->isOpen())
    {
        socket->abort(); // Forcefully close, don't wait for graceful close
    }

    QString peerUuid = socketToUuidMap.value(socket);
    if (!peerUuid.isEmpty())
    {
        if (removeFromConnected)
        {
            connectedSockets.remove(peerUuid);
        }
        // Only remove from socketToUuidMap if it's the correct UUID for this socket.
        // This check is mostly for sanity, as value() would return the mapped UUID.
        if (socketToUuidMap.value(socket) == peerUuid)
        {
            socketToUuidMap.remove(socket);
        }
        peerUuidToNameMap.remove(peerUuid);
    }

    // Remove from other lists/maps if it could be there.
    // These lists should ideally be managed such that a socket isn't in multiple
    // "pending" type states simultaneously.
    pendingIncomingSockets.removeAll(socket);
    if (outgoingSocketsAwaitingSessionAccepted.contains(socket))
    {
        outgoingSocketsAwaitingSessionAccepted.remove(socket);
    }

    socket->deleteLater(); // Schedule for deletion
    qDebug() << "NM::cleanupSocket: Socket scheduled for deletion.";
}

void NetworkManager::sendSystemMessage(QTcpSocket *socket, const QString &sysMessage)
{
    if (socket && socket->isOpen() && socket->state() == QAbstractSocket::ConnectedState)
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_5);
        out << sysMessage;
        socket->write(block);
        socket->flush();
    }
}

void NetworkManager::addEstablishedConnection(QTcpSocket *socket, const QString &peerUuid, const QString &peerName, const QString &peerAddress, quint16 peerPort)
{
    if (!socket || peerUuid.isEmpty())
    {
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

void NetworkManager::removePendingIncomingSocket(QTcpSocket *socket)
{
    if (!socket)
        return;
    pendingIncomingSockets.removeOne(socket);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handlePendingIncomingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handlePendingIncomingSocketError);
}

void NetworkManager::removeOutgoingSocketAwaitingAcceptance(QTcpSocket *socket)
{
    if (!socket)
        return;
    outgoingSocketsAwaitingSessionAccepted.remove(socket);
    disconnect(socket, &QTcpSocket::connected, this, &NetworkManager::handleOutgoingSocketConnected);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleOutgoingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleOutgoingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleOutgoingSocketError);
}

// New private helper method
QString NetworkManager::getDiscoveryMessageValue(const QStringList &parts, const QString &key) const
{
    QString searchKey = key + "=";
    for (const QString &part : parts)
    {
        if (part.startsWith(searchKey))
        {
            return part.mid(searchKey.length());
        }
    }
    return QString();
}
