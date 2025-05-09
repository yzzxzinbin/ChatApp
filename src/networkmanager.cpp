#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream>
#include <QRegularExpression> // For parsing

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
      defaultPort(60248),
      preferredListenPort(60248),
      preferredOutgoingPortNumber(0),
      bindToSpecificOutgoingPort(false),
      localUserUuid(),
      localUserDisplayName()
{
    setupServer(); // Initial setup for the server
}

NetworkManager::~NetworkManager()
{
    stopListening(); // This will also clean up connected sockets

    // Clean up any remaining pending sockets
    qDeleteAll(pendingIncomingSockets);
    pendingIncomingSockets.clear();

    qDeleteAll(outgoingSocketsAwaitingSessionAccepted.keys());
    outgoingSocketsAwaitingSessionAccepted.clear();

    if (tcpServer) {
        tcpServer->deleteLater(); // Ensure server is deleted
        tcpServer = nullptr;
    }
}

void NetworkManager::setupServer()
{
    if (tcpServer) {
        tcpServer->close();
        // Disconnect any old connections if server object is being reused.
        // However, typically we delete and create a new one.
        disconnect(tcpServer, nullptr, nullptr, nullptr);
        tcpServer->deleteLater(); // Schedule for deletion
    }

    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    connect(tcpServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError socketError){
        lastError = tcpServer->errorString();
        // For server-wide errors, we don't have a specific peer UUID
        emit peerNetworkError("", socketError, lastError);
        emit serverStatusMessage(QString("Server Error: %1").arg(lastError));
    });
}

bool NetworkManager::startListening()
{
    if (tcpServer->isListening()) {
        emit serverStatusMessage(QString("Server is already listening on port %1.").arg(tcpServer->serverPort()));
        return true;
    }

    // Ensure server object is fresh if it was previously stopped/failed
    setupServer();

    quint16 portToListen = preferredListenPort > 0 ? preferredListenPort : defaultPort;
    if (portToListen == 0) portToListen = defaultPort; // Fallback
    if (portToListen == 0) portToListen = 60248; // Absolute fallback

    if (!tcpServer->listen(QHostAddress::Any, portToListen)) {
        lastError = tcpServer->errorString();
        emit serverStatusMessage(QString("Server could not start on port %1: %2").arg(portToListen).arg(lastError));
        return false;
    }

    emit serverStatusMessage(QString("Server started, listening on port %1.").arg(tcpServer->serverPort()));
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (const QHostAddress &ipAddress : ipAddressesList) {
        if (ipAddress != QHostAddress::LocalHost && ipAddress.toIPv4Address()) {
            emit serverStatusMessage(QString("Accessible IP: %1").arg(ipAddress.toString()));
        }
    }
    return true;
}

void NetworkManager::stopListening()
{
    if (tcpServer && tcpServer->isListening()) {
        tcpServer->close();
        emit serverStatusMessage("Server stopped.");
    }

    // Disconnect and clean up all connected clients
    QStringList currentPeerUuids = connectedSockets.keys();
    for (const QString& peerUuid : currentPeerUuids) {
        disconnectFromPeer(peerUuid); // This will call cleanupSocket
    }
    connectedSockets.clear();
    socketToUuidMap.clear();
    peerUuidToNameMap.clear();

    // Clean up pending incoming sockets
    for (QTcpSocket* socket : pendingIncomingSockets) {
        socket->abort();
        socket->deleteLater();
    }
    pendingIncomingSockets.clear();

    // Clean up outgoing sockets awaiting session acceptance
    for (QTcpSocket* socket : outgoingSocketsAwaitingSessionAccepted.keys()) {
        socket->abort();
        socket->deleteLater();
    }
    outgoingSocketsAwaitingSessionAccepted.clear();
}

void NetworkManager::connectToHost(const QString &peerNameToSet, const QString &targetPeerUuidHint, const QString &hostAddress, quint16 port)
{
    Q_UNUSED(targetPeerUuidHint); // targetPeerUuidHint might be used later for re-establishing with known UUIDs

    // Prevent duplicate connection attempts to the same IP/Port if one is already in outgoingAwaiting state
    // Or if already connected to a peer with the same target IP/Port (more complex to check without full UUID knowledge yet)
    // For now, allow multiple attempts, but UI should prevent this.

    QTcpSocket *socket = new QTcpSocket(this);

    if (bindToSpecificOutgoingPort && preferredOutgoingPortNumber > 0) {
        if (!socket->bind(QHostAddress::AnyIPv4, preferredOutgoingPortNumber)) {
            emit serverStatusMessage(tr("Warning: Could not bind to outgoing port %1. Error: %2. Proceeding with dynamic port.")
                                     .arg(preferredOutgoingPortNumber).arg(socket->errorString()));
        } else {
            emit serverStatusMessage(tr("Successfully bound to outgoing port %1 for connection to %2.").arg(preferredOutgoingPortNumber).arg(hostAddress));
        }
    }

    // Store the name we intend to use for this peer temporarily with the socket
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
        cleanupSocket(socket); // cleanupSocket will handle removal from maps and deletion
    } else {
        emit serverStatusMessage(tr("Cannot disconnect: Peer UUID %1 not found.").arg(peerUuid));
    }
}

void NetworkManager::sendMessage(const QString &targetPeerUuid, const QString &message)
{
    QTcpSocket *socket = connectedSockets.value(targetPeerUuid, nullptr);
    if (socket && socket->isOpen() && socket->state() == QAbstractSocket::ConnectedState) {
        sendSystemMessage(socket, message); // Reusing sendSystemMessage for general message sending
    } else {
        lastError = tr("Peer %1 not connected or socket invalid.").arg(targetPeerUuid);
        emit serverStatusMessage(tr("Cannot send message to %1: Not connected.").arg(peerUuidToNameMap.value(targetPeerUuid, targetPeerUuid)));
        // Optionally emit a peerNetworkError here if desired
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

void NetworkManager::setListenPreferences(quint16 port)
{
    preferredListenPort = (port > 0) ? port : defaultPort;
}

void NetworkManager::setOutgoingConnectionPreferences(quint16 port, bool useSpecific)
{
    preferredOutgoingPortNumber = port;
    bindToSpecificOutgoingPort = useSpecific;
}

// --- Private Slots ---

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
        // Should not happen for a connected socket
        emit serverStatusMessage(tr("Error: Data from unknown connected socket. Closing."));
        cleanupSocket(socket, false); // Don't try to remove from connectedSockets if UUID unknown
        return;
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    while(socket->bytesAvailable() > 0) {
        // Basic framing: assume quint32 size prefix, then QString
        // This is a simplification; real protocols might be more complex.
        // For this example, we'll assume messages are sent as whole QStrings via QDataStream
        if (socket->bytesAvailable() < (int)sizeof(quint32)) // Minimum size for a size prefix
            return; // Wait for more data

        in.startTransaction();
        QString message;
        in >> message; // QDataStream handles QString serialization

        if (in.commitTransaction()) {
            // Check for system messages if any are expected post-connection
            // For now, assume all post-connection messages are user messages
            emit newMessageReceived(peerUuid, message);
        } else {
            // Transaction failed, data incomplete for a full QString. Wait for more.
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
    lastError = errorString; // Store last error

    if (!peerUuid.isEmpty()) {
        emit serverStatusMessage(tr("Network error with peer %1 (UUID: %2): %3")
                                 .arg(peerUuidToNameMap.value(peerUuid, "Unknown"))
                                 .arg(peerUuid)
                                 .arg(errorString));
        emit peerNetworkError(peerUuid, socketError, errorString);
    } else {
        // Error on a socket not fully mapped, possibly during cleanup
        emit serverStatusMessage(tr("Network error on unmapped socket: %1").arg(errorString));
    }
    // cleanupSocket will be called by disconnected if the error leads to disconnection
}


void NetworkManager::handlePendingIncomingSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !socket->isValid() || socket->bytesAvailable() == 0) return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    if (socket->bytesAvailable() < (int)sizeof(quint32)) return; // Wait for size of string

    in.startTransaction();
    QString message;
    in >> message;

    if (in.commitTransaction()) {
        if (message.startsWith("<SYS_HELLO")) {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerNameHint = extractAttribute(message, "NameHint");

            if (peerUuid.isEmpty() || peerUuid == localUserUuid) { // Reject if no UUID or self-connect
                emit serverStatusMessage(tr("Error: Received HELLO from %1 without valid UUID or self-connect. Rejecting.")
                                         .arg(socket->peerAddress().toString()));
                sendSystemMessage(socket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid HELLO"));
                removePendingIncomingSocket(socket);
                socket->abort(); // Then deleteLater via disconnected
                return;
            }
             if (connectedSockets.contains(peerUuid)) {
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

            // Disconnect this slot, MainWindow will decide to accept/reject
            disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
            emit incomingSessionRequest(socket, socket->peerAddress().toString(), socket->peerPort(), peerUuid, peerNameHint);
        } else {
            emit serverStatusMessage(tr("Error: Expected HELLO from %1, got: %2. Closing.")
                                     .arg(socket->peerAddress().toString()).arg(message.left(50)));
            removePendingIncomingSocket(socket); // remove before abort to avoid issues if disconnected signal fires
            socket->abort(); // This will trigger disconnected and cleanup
        }
    } // else wait for more data
}

void NetworkManager::handlePendingIncomingSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    emit serverStatusMessage(tr("Pending connection from %1 disconnected before session establishment.")
                             .arg(socket->peerAddress().toString()));
    removePendingIncomingSocket(socket); // Ensure removal from list
    socket->deleteLater(); // Standard cleanup
}

void NetworkManager::handlePendingIncomingSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    Q_UNUSED(socketError);
    emit serverStatusMessage(tr("Error on pending connection from %1: %2")
                             .arg(socket->peerAddress().toString())
                             .arg(socket->errorString()));
    removePendingIncomingSocket(socket); // Ensure removal from list
    socket->deleteLater(); // Standard cleanup
}

void NetworkManager::handleOutgoingSocketConnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerNameToSet = outgoingSocketsAwaitingSessionAccepted.value(socket, "Unknown Peer");
    emit serverStatusMessage(tr("TCP link established with %1 (%2:%3). Sending HELLO...")
                             .arg(peerNameToSet)
                             .arg(socket->peerAddress().toString())
                             .arg(socket->peerPort()));

    QString helloMessage = SYS_MSG_HELLO_FORMAT.arg(localUserUuid).arg(localUserDisplayName);
    sendSystemMessage(socket, helloMessage);
    // Socket remains in outgoingSocketsAwaitingSessionAccepted, waiting for SYS_SESSION_ACCEPTED
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
        if (message.startsWith("<SYS_SESSION_ACCEPTED")) {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerReportedName = extractAttribute(message, "Name"); // Peer's name for itself

            if (peerUuid.isEmpty() || peerUuid == localUserUuid) {
                emit serverStatusMessage(tr("Error: Received SESSION_ACCEPTED from %1 without valid UUID or self-connect. Disconnecting.")
                                         .arg(localNameForPeerAttempt));
                emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Invalid SESSION_ACCEPTED (UUID error)"));
                removeOutgoingSocketAwaitingAcceptance(socket);
                socket->abort();
                return;
            }
            if (connectedSockets.contains(peerUuid)) {
                 emit serverStatusMessage(tr("Error: Peer %1 (UUID: %2) is already connected (race condition?). Ignoring new session acceptance.")
                                          .arg(localNameForPeerAttempt).arg(peerUuid));
                 emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Peer already connected (race condition)"));
                 removeOutgoingSocketAwaitingAcceptance(socket);
                 socket->abort();
                 return;
            }

            emit serverStatusMessage(tr("Session with %1 (UUID: %2, Reported Name: %3) accepted by peer.")
                                     .arg(localNameForPeerAttempt).arg(peerUuid).arg(peerReportedName));
            
            removeOutgoingSocketAwaitingAcceptance(socket); // Remove from temp map
            addEstablishedConnection(socket, peerUuid, localNameForPeerAttempt, socket->peerAddress().toString(), socket->peerPort());

        } else if (message.startsWith("<SYS_SESSION_REJECTED")) {
            QString reason = extractAttribute(message, "Reason");
            emit serverStatusMessage(tr("Session with %1 rejected by peer. Reason: %2")
                                     .arg(localNameForPeerAttempt)
                                     .arg(reason.isEmpty() ? "No reason given" : reason));
            emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Session rejected by peer: %1").arg(reason.isEmpty() ? tr("No reason given") : reason));
            removeOutgoingSocketAwaitingAcceptance(socket);
            socket->abort(); // This will trigger disconnected and cleanup
        } else {
            emit serverStatusMessage(tr("Expected SESSION_ACCEPTED/REJECTED from %1, got: %2. Disconnecting.")
                                     .arg(localNameForPeerAttempt)
                                     .arg(message.left(50)));
            emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Invalid response from peer"));
            removeOutgoingSocketAwaitingAcceptance(socket);
            socket->abort();
        }
    } // else wait for more data
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

// --- Public Slots for Session Management by MainWindow ---
void NetworkManager::acceptIncomingSession(QTcpSocket* tempSocket, const QString& peerUuid, const QString& localNameForPeer)
{
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket)) {
        emit serverStatusMessage(tr("Error: Cannot accept session, socket not found or not pending."));
        if(tempSocket) tempSocket->deleteLater(); // Clean up if passed but not in list
        return;
    }
    if (peerUuid.isEmpty() || peerUuid == localUserUuid) {
        emit serverStatusMessage(tr("Error: Cannot accept session, invalid peer UUID."));
        sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid UUID"));
        removePendingIncomingSocket(tempSocket);
        tempSocket->abort();
        return;
    }
    if (connectedSockets.contains(peerUuid)) {
        emit serverStatusMessage(tr("Error: Peer with UUID %1 is already connected. Rejecting duplicate session.").arg(peerUuid));
        sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Already connected"));
        removePendingIncomingSocket(tempSocket);
        tempSocket->abort();
        return;
    }


    removePendingIncomingSocket(tempSocket); // Remove from pending list

    QString acceptedMessage = SYS_MSG_SESSION_ACCEPTED_FORMAT.arg(localUserUuid).arg(localUserDisplayName);
    sendSystemMessage(tempSocket, acceptedMessage);

    addEstablishedConnection(tempSocket, peerUuid, localNameForPeer, tempSocket->peerAddress().toString(), tempSocket->peerPort());
    emit serverStatusMessage(tr("Session with %1 (UUID: %2) accepted. Sent session acceptance.")
                             .arg(localNameForPeer).arg(peerUuid));
}

void NetworkManager::rejectIncomingSession(QTcpSocket* tempSocket)
{
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket)) {
        emit serverStatusMessage(tr("Error: Cannot reject session, socket not found or not pending."));
        if(tempSocket) tempSocket->deleteLater();
        return;
    }
    emit serverStatusMessage(tr("Incoming session from %1 rejected by user.")
                             .arg(tempSocket->peerAddress().toString()));
    sendSystemMessage(tempSocket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Rejected by user"));
    removePendingIncomingSocket(tempSocket);
    tempSocket->abort(); // This will trigger its disconnected signal and deleteLater
}


// --- Private Helper Methods ---
void NetworkManager::cleanupSocket(QTcpSocket* socket, bool removeFromConnected)
{
    if (!socket) return;

    QString peerUuid = socketToUuidMap.value(socket);

    // Disconnect all signals from this socket to NetworkManager slots
    disconnect(socket, nullptr, this, nullptr);

    if (removeFromConnected && !peerUuid.isEmpty()) {
        connectedSockets.remove(peerUuid);
        peerUuidToNameMap.remove(peerUuid);
    }
    socketToUuidMap.remove(socket);

    if (socket->isOpen()) {
        socket->abort(); // Force close, don't wait for graceful disconnect
    }
    socket->deleteLater();
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
    if (!socket || peerUuid.isEmpty()) return;

    // Disconnect any temporary signal/slot connections (e.g., from pending or outgoing states)
    // This is crucial to avoid multiple handlers for the same signal.
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handlePendingIncomingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handlePendingIncomingSocketError);

    disconnect(socket, &QTcpSocket::connected, this, &NetworkManager::handleOutgoingSocketConnected);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleOutgoingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleOutgoingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleOutgoingSocketError);

    // Connect to standard client socket handlers
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
    pendingIncomingSockets.removeOne(socket); // QList::removeOne
    // Disconnect its specific signals
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handlePendingIncomingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handlePendingIncomingSocketError);
}

void NetworkManager::removeOutgoingSocketAwaitingAcceptance(QTcpSocket* socket)
{
    if (!socket) return;
    outgoingSocketsAwaitingSessionAccepted.remove(socket);
    // Disconnect its specific signals
    disconnect(socket, &QTcpSocket::connected, this, &NetworkManager::handleOutgoingSocketConnected);
    disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handleOutgoingSocketReadyRead);
    disconnect(socket, &QTcpSocket::disconnected, this, &NetworkManager::handleOutgoingSocketDisconnected);
    disconnect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::handleOutgoingSocketError);
}
