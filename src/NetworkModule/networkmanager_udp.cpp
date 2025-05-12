#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDebug>

// UDP specific methods of NetworkManager

void NetworkManager::startUdpDiscovery()
{
    if (!udpDiscoveryEnabled)
    {
        qDebug() << "NM::startUdpDiscovery: Attempted to start but UDP discovery is disabled.";
        return;
    }

    if (udpDiscoveryListenerSocket && udpDiscoveryListenerSocket->state() != QAbstractSocket::UnconnectedState)
    {
        emit serverStatusMessage(tr("UDP discovery is already active on port %1.").arg(currentUdpDiscoveryPort)); // Use member variable
        return;
    }

    if (udpDiscoveryListenerSocket)
    {
        qDebug() << "NM::startUdpDiscovery: Cleaning up old udpDiscoveryListenerSocket.";
        disconnect(udpDiscoveryListenerSocket, nullptr, nullptr, nullptr);
        udpDiscoveryListenerSocket->close();
        udpDiscoveryListenerSocket->deleteLater(); // Ensure old socket is deleted
        udpDiscoveryListenerSocket = nullptr;
    }

    udpDiscoveryListenerSocket = new QUdpSocket(this); // Parent is NetworkManager
    connect(udpDiscoveryListenerSocket, &QUdpSocket::readyRead, this, &NetworkManager::processPendingUdpDatagrams);
    connect(udpDiscoveryListenerSocket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError)
            {
        Q_UNUSED(socketError);
        QString errorStr;
        if(this->udpDiscoveryListenerSocket) { // Access member via this->
            errorStr = this->udpDiscoveryListenerSocket->errorString();
            emit serverStatusMessage(tr("UDP Socket Error: %1").arg(errorStr));
        } else {
            errorStr = tr("Unknown UDP socket error (socket already deleted)");
            emit serverStatusMessage(tr("UDP Socket Error on an already deleted socket."));
        }
        qWarning() << "NM::udpSocket Error:" << errorStr; });

    if (udpDiscoveryListenerSocket->bind(QHostAddress::AnyIPv4, currentUdpDiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) // Use member variable
    {
        emit serverStatusMessage(tr("UDP discovery started, listening for broadcasts on port %1.").arg(currentUdpDiscoveryPort)); // Use member variable

        // Ensure broadcast sender socket is also initialized
        if (!udpBroadcastSenderSocket)
        {
            udpBroadcastSenderSocket = new QUdpSocket(this);
            // No bind needed for sender usually, unless specific interface or port is required for sending from.
        }
        
        sendUdpBroadcast(); // Send initial broadcast upon successful start

        // Start periodic broadcast timer if discovery and continuous broadcast are enabled
        if (udpDiscoveryEnabled && udpContinuousBroadcastEnabled && udpBroadcastTimer)
        {
            if (udpBroadcastTimer->isActive()) udpBroadcastTimer->stop(); // Stop if already running with old interval
            udpBroadcastTimer->start(udpBroadcastIntervalSeconds * 1000); // Use seconds
            qDebug() << "NM::startUdpDiscovery: Continuous broadcast timer started with interval:" << udpBroadcastIntervalSeconds << "s";
        }
        else if (udpBroadcastTimer && udpBroadcastTimer->isActive())
        {
            udpBroadcastTimer->stop(); // Stop if not continuous
            qDebug() << "NM::startUdpDiscovery: Continuous broadcast disabled, timer stopped.";
        }
    }
    else
    {
        emit serverStatusMessage(tr("UDP discovery could not start on port %1: %2").arg(currentUdpDiscoveryPort).arg(udpDiscoveryListenerSocket->errorString())); // Use member variable
        qWarning() << "NM::startUdpDiscovery: Failed to bind UDP listener socket:" << udpDiscoveryListenerSocket->errorString();
        if (udpDiscoveryListenerSocket)
        { // Clean up failed socket
            udpDiscoveryListenerSocket->deleteLater();
            udpDiscoveryListenerSocket = nullptr;
        }
    }
}

void NetworkManager::stopUdpDiscovery()
{
    if (udpBroadcastTimer && udpBroadcastTimer->isActive())
    { // Stop broadcast timer
        udpBroadcastTimer->stop();
        qDebug() << "NM::stopUdpDiscovery: Stopped periodic UDP broadcast timer.";
    }
    cleanupTemporaryUdpResponseListener(); // Ensure temporary listener is cleaned up

    if (udpDiscoveryListenerSocket)
    {
        qDebug() << "NM::stopUdpDiscovery: Stopping UDP discovery listener. Current state:" << udpDiscoveryListenerSocket->state();
        if (udpDiscoveryListenerSocket->state() != QAbstractSocket::UnconnectedState)
        {
            udpDiscoveryListenerSocket->close(); // Close the socket
        }
    }
    else
    {
        qDebug() << "NM::stopUdpDiscovery: udpDiscoveryListenerSocket is already null.";
    }
    if (udpBroadcastSenderSocket)
    {
        qDebug() << "NM::stopUdpDiscovery: Closing UDP broadcast sender. Current state:" << udpBroadcastSenderSocket->state();
        if (udpBroadcastSenderSocket->state() != QAbstractSocket::UnconnectedState)
        {
            udpBroadcastSenderSocket->close();
        }
        // It will be deleted in NetworkManager's destructor
    }
    else
    {
        qDebug() << "NM::stopUdpDiscovery: udpBroadcastSenderSocket is already null.";
    }
    if (udpDiscoveryListenerSocket || udpBroadcastSenderSocket || (udpBroadcastTimer && udpBroadcastTimer->isActive())) // check timer too
    {
        emit serverStatusMessage(tr("UDP discovery stopped."));
    }
}

void NetworkManager::triggerManualUdpBroadcast()
{
    if (!udpDiscoveryEnabled)
    {
        emit serverStatusMessage(tr("Cannot send manual broadcast: UDP discovery is disabled."));
        return;
    }

    if (!udpDiscoveryListenerSocket || udpDiscoveryListenerSocket->state() == QAbstractSocket::UnconnectedState || !udpBroadcastSenderSocket)
    {
        emit serverStatusMessage(tr("UDP socket(s) not ready for manual broadcast. Attempting to initialize..."));
        qDebug() << "NM::triggerManualUdpBroadcast: UDP socket(s) not ready, attempting to start discovery first.";
        startUdpDiscovery(); // This will try to bind and send initial broadcast
        if (!udpDiscoveryListenerSocket || udpDiscoveryListenerSocket->state() == QAbstractSocket::UnconnectedState || !udpBroadcastSenderSocket)
        { // If start failed
            emit serverStatusMessage(tr("Failed to initialize UDP for manual broadcast."));
            return;
        }
    }

    emit serverStatusMessage(tr("Sending manual UDP discovery broadcast..."));
    sendUdpBroadcast(); // Explicitly send
}

void NetworkManager::sendUdpBroadcast()
{
    if (localUserUuid.isEmpty())
    {
        qWarning() << "NM::sendUdpBroadcast: Local user UUID is empty. Cannot send broadcast.";
        return;
    }
    if (!udpDiscoveryEnabled)
    {
        qDebug() << "NM::sendUdpBroadcast: UDP discovery is disabled. Skipping broadcast.";
        return;
    }
    // Check if listener is bound (as a proxy for discovery being "active") and sender is available
    if (!udpDiscoveryListenerSocket || udpDiscoveryListenerSocket->state() == QAbstractSocket::UnconnectedState || !udpBroadcastSenderSocket)
    {
        qWarning() << "NM::sendUdpBroadcast: UDP listener not bound or sender socket not available. Cannot send broadcast.";
        if (udpDiscoveryEnabled)
        {
            qDebug() << "NM::sendUdpBroadcast: Attempting to re-initialize UDP sockets via startUdpDiscovery.";
            startUdpDiscovery(); // This itself calls sendUdpBroadcast() upon success
            // Check again after attempt to start
            if (!udpDiscoveryListenerSocket || udpDiscoveryListenerSocket->state() == QAbstractSocket::UnconnectedState || !udpBroadcastSenderSocket) {
                 // If still not ready, exit. startUdpDiscovery would have emitted an error.
                return; 
            }
        }
        else
        {
            return;
        }
    }

    QString messageStr;
    quint16 advertisedReplyToPort = 0;

    if (tcpServer && tcpServer->isListening())
    {
        messageStr = QString("%1;UUID=%2;Name=%3;TCPPort=%4;")
                         .arg(UDP_DISCOVERY_MSG_PREFIX) // ANNOUNCE
                         .arg(localUserUuid)
                         .arg(localUserDisplayName)
                         .arg(tcpServer->serverPort());
        qDebug() << "NM::sendUdpBroadcast (ANNOUNCE):" << messageStr;
    }
    else
    {                                          // Sending NEED message
        cleanupTemporaryUdpResponseListener(); // Cleanup any previous temporary listener first
        udpTemporaryResponseListenerSocket = new QUdpSocket(this);
        connect(udpTemporaryResponseListenerSocket, &QUdpSocket::readyRead, this, &NetworkManager::processUdpResponseToNeed);
        connect(udpTemporaryResponseListenerSocket, &QUdpSocket::errorOccurred, this, &NetworkManager::handleTemporaryUdpSocketError);

        if (udpTemporaryResponseListenerSocket->bind(QHostAddress::AnyIPv4, 0))
        { // Bind to OS-assigned port
            advertisedReplyToPort = udpTemporaryResponseListenerSocket->localPort();
            qDebug() << "NM::sendUdpBroadcast (NEED): Temporary listener bound to port" << advertisedReplyToPort;
            if (udpResponseListenerTimer)
            { // Ensure timer exists
                udpResponseListenerTimer->start(UDP_TEMP_RESPONSE_LISTENER_TIMEOUT_MS); // Ensure this constant is defined
            }
        }
        else
        {
            qWarning() << "NM::sendUdpBroadcast (NEED): Failed to bind temporary listener socket:" << udpTemporaryResponseListenerSocket->errorString();
            // No deleteLater here, cleanupTemporaryUdpResponseListener will handle it if called or destructor
            if (udpTemporaryResponseListenerSocket)
            {
                udpTemporaryResponseListenerSocket->deleteLater(); // clean it up now
                udpTemporaryResponseListenerSocket = nullptr;
            }
        }

        messageStr = QString("%1;UUID=%2;Name=%3;")
                         .arg(UDP_NEED_CONNECTION_PREFIX) // NEED
                         .arg(localUserUuid)
                         .arg(localUserDisplayName);
        if (advertisedReplyToPort > 0)
        {
            messageStr += QString("%1=%2;").arg(UDP_REPLY_TO_PORT_FIELD_KEY).arg(advertisedReplyToPort);
        }
        qDebug() << "NM::sendUdpBroadcast (NEED):" << messageStr;
    }

    QByteArray datagram = messageStr.toUtf8();
    // Use udpBroadcastSenderSocket for writing
    qint64 bytesSent = udpBroadcastSenderSocket->writeDatagram(datagram, QHostAddress::Broadcast, currentUdpDiscoveryPort); // Use member variable

    if (bytesSent == -1)
    {
        emit serverStatusMessage(tr("UDP broadcast failed: %1").arg(udpBroadcastSenderSocket->errorString()));
        qWarning() << "NM::sendUdpBroadcast: writeDatagram failed:" << udpBroadcastSenderSocket->errorString();
        if (!tcpServer || !tcpServer->isListening())
        {                                          // If it was a NEED broadcast that failed to send
            cleanupTemporaryUdpResponseListener(); // Don't leave temporary listener if broadcast failed
        }
    }
    else
    {
        qDebug() << "NM::sendUdpBroadcast: Sent" << bytesSent << "bytes.";
    }
}

void NetworkManager::processPendingUdpDatagrams()
{
    if (!udpDiscoveryListenerSocket || !udpDiscoveryEnabled)
        return; // Use udpDiscoveryListenerSocket

    while (udpDiscoveryListenerSocket->hasPendingDatagrams())
    { // Use udpDiscoveryListenerSocket
        QByteArray datagram;
        datagram.resize(udpDiscoveryListenerSocket->pendingDatagramSize()); // Use udpDiscoveryListenerSocket
        QHostAddress senderAddress;
        quint16 senderUdpPort;

        udpDiscoveryListenerSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderUdpPort); // Use udpDiscoveryListenerSocket

        QString message = QString::fromUtf8(datagram);
        qDebug() << "NM::processPendingUdpDatagrams: Received from" << senderAddress.toString() << ":" << senderUdpPort << "Data:" << message;

        QStringList parts = message.split(';', Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            qWarning() << "NM::processPendingUdpDatagrams: Empty or malformed message:" << message;
            continue;
        }

        QString messageType = parts[0];
        QString peerUuid = getDiscoveryMessageValue(parts, "UUID");     // Use helper
        QString peerNameHint = getDiscoveryMessageValue(parts, "Name"); // Use helper
        quint16 peerTcpPort = 0;
        QString tcpPortStr = getDiscoveryMessageValue(parts, "TCPPort"); // Use helper
        if (!tcpPortStr.isEmpty())
        {
            bool ok;
            peerTcpPort = tcpPortStr.toUShort(&ok);
            if (!ok)
            {
                qWarning() << "NM::processPendingUdpDatagrams: Invalid TCPPort in message:" << message;
                peerTcpPort = 0;
            }
        }

        if (peerUuid.isEmpty())
        {
            qWarning() << "NM::processPendingUdpDatagrams: Message missing UUID:" << message;
            continue;
        }

        if (peerUuid == localUserUuid)
        {
            qDebug() << "NM::processPendingUdpDatagrams: Ignoring own broadcast/message.";
            continue;
        }

        if (connectedSockets.contains(peerUuid))
        {
            qDebug() << "NM::processPendingUdpDatagrams: Peer" << peerUuid << "is already connected. Ignoring discovery message type:" << messageType;
            continue;
        }

        // Check if an outgoing connection attempt for this peerUuid is already in progress
        bool alreadyAttemptingOutgoing = false;
        if (!peerUuid.isEmpty())
        {
            for (const QPair<QString, QString> &attemptDetails : outgoingSocketsAwaitingSessionAccepted.values())
            {
                if (attemptDetails.second == peerUuid)
                {
                    alreadyAttemptingOutgoing = true;
                    qDebug() << "NM::processPendingUdpDatagrams: Already attempting outgoing connection to UUID" << peerUuid << ". Ignoring discovery message type:" << messageType;
                    break;
                }
            }
        }
        if (alreadyAttemptingOutgoing)
        {
            continue;
        }

        if (messageType == UDP_DISCOVERY_MSG_PREFIX)
        { // Received ANNOUNCE
            if (peerTcpPort == 0)
            {
                qWarning() << "NM::processPendingUdpDatagrams: ANNOUNCE message from" << peerUuid << "missing valid TCPPort:" << message;
                continue;
            }
            emit serverStatusMessage(tr("UDP Discovery (ANNOUNCE): Found peer %1 (UUID: %2) at %3, TCP Port: %4. Attempting TCP connection.")
                                         .arg(peerNameHint.isEmpty() ? peerUuid : peerNameHint)
                                         .arg(peerUuid)
                                         .arg(senderAddress.toString())
                                         .arg(peerTcpPort));
            connectToHost(peerNameHint.isEmpty() ? peerUuid : peerNameHint, peerUuid, senderAddress.toString(), peerTcpPort);
        }
        else if (messageType == UDP_NEED_CONNECTION_PREFIX)
        { // Received NEED
            if (tcpServer && tcpServer->isListening())
            {
                QString responseMsg = QString("%1;UUID=%2;Name=%3;TCPPort=%4;")
                                          .arg(UDP_RESPONSE_TO_NEED_PREFIX) // REQNEED
                                          .arg(localUserUuid)
                                          .arg(localUserDisplayName)
                                          .arg(tcpServer->serverPort());
                QByteArray responseDatagram = responseMsg.toUtf8();

                quint16 targetUdpPort = senderUdpPort; // Default to the port the NEED message came from
                QString replyToPortStr = getDiscoveryMessageValue(parts, UDP_REPLY_TO_PORT_FIELD_KEY);
                if (!replyToPortStr.isEmpty())
                {
                    bool ok;
                    quint16 advertisedPort = replyToPortStr.toUShort(&ok);
                    if (ok && advertisedPort > 0)
                    {
                        targetUdpPort = advertisedPort;
                        qDebug() << "NM::processPendingUdpDatagrams (Responding to NEED): Using advertised reply port" << targetUdpPort;
                    }
                    else
                    {
                        qWarning() << "NM::processPendingUdpDatagrams (Responding to NEED): Invalid ReplyToUDPPort" << replyToPortStr << ". Defaulting to senderUdpPort" << senderUdpPort;
                    }
                }

                qDebug() << "NM::processPendingUdpDatagrams (Responding to NEED): Sending REQNEED to"
                         << senderAddress.toString() << ":" << targetUdpPort << "Data:" << responseMsg;

                if (udpBroadcastSenderSocket->writeDatagram(responseDatagram, senderAddress, targetUdpPort) == -1) // targetUdpPort is correct here
                {
                    qWarning() << "NM::processPendingUdpDatagrams: Failed to send REQNEED to" << senderAddress.toString() << ":" << targetUdpPort << "Error:" << udpBroadcastSenderSocket->errorString();
                }
                else
                {
                    emit serverStatusMessage(tr("UDP Discovery (NEED received): Responded to %1 (UUID: %2) at %3 (port %4) with our connection info.")
                                                 .arg(peerNameHint.isEmpty() ? peerUuid : peerNameHint)
                                                 .arg(peerUuid)
                                                 .arg(senderAddress.toString())
                                                 .arg(targetUdpPort));
                }
            }
            else
            {
                qDebug() << "NM::processPendingUdpDatagrams (NEED received): Not listening on TCP, cannot respond to NEED from" << peerUuid;
            }
        }
        else if (messageType == UDP_RESPONSE_TO_NEED_PREFIX)
        { // Received REQNEED
            if (peerTcpPort == 0)
            {
                qWarning() << "NM::processPendingUdpDatagrams: REQNEED message from" << peerUuid << "missing valid TCPPort:" << message;
                continue;
            }
            emit serverStatusMessage(tr("UDP Discovery (REQNEED received): Peer %1 (UUID: %2) at %3 responded with TCP Port: %4. Attempting TCP connection.")
                                         .arg(peerNameHint.isEmpty() ? peerUuid : peerNameHint)
                                         .arg(peerUuid)
                                         .arg(senderAddress.toString())
                                         .arg(peerTcpPort));
            connectToHost(peerNameHint.isEmpty() ? peerUuid : peerNameHint, peerUuid, senderAddress.toString(), peerTcpPort);
        }
        else
        {
            qWarning() << "NM::processPendingUdpDatagrams: Unknown UDP message type:" << messageType << "Full message:" << message;
        }
    }
}

// New private helper method
void NetworkManager::cleanupTemporaryUdpResponseListener()
{
    if (udpResponseListenerTimer && udpResponseListenerTimer->isActive())
    {
        udpResponseListenerTimer->stop();
    }
    if (udpTemporaryResponseListenerSocket)
    {
        qDebug() << "NM::cleanupTemporaryUdpResponseListener: Cleaning up temporary UDP response listener socket.";
        disconnect(udpTemporaryResponseListenerSocket, nullptr, nullptr, nullptr);
        udpTemporaryResponseListenerSocket->close();
        udpTemporaryResponseListenerSocket->deleteLater();
        udpTemporaryResponseListenerSocket = nullptr;
    }
}

// New slot implementation
void NetworkManager::processUdpResponseToNeed()
{
    if (!udpTemporaryResponseListenerSocket)
        return;

    qDebug() << "NM::processUdpResponseToNeed: Data received on temporary listener.";
    if (udpResponseListenerTimer->isActive())
    {
        udpResponseListenerTimer->stop();
    }

    while (udpTemporaryResponseListenerSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(udpTemporaryResponseListenerSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderUdpPort; // Port the REQNEED came from, not necessarily useful here

        udpTemporaryResponseListenerSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderUdpPort);
        QString message = QString::fromUtf8(datagram);
        qDebug() << "NM::processUdpResponseToNeed: Received from" << senderAddress.toString() << ":" << senderUdpPort << "Data:" << message;

        QStringList parts = message.split(';', Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            qWarning() << "NM::processUdpResponseToNeed: Empty or malformed message:" << message;
            continue;
        }
        QString messageType = parts[0];
        if (messageType == UDP_RESPONSE_TO_NEED_PREFIX)
        { // Received REQNEED
            QString peerUuid = getDiscoveryMessageValue(parts, "UUID");
            QString peerNameHint = getDiscoveryMessageValue(parts, "Name");
            bool ok;
            quint16 peerTcpPort = getDiscoveryMessageValue(parts, "TCPPort").toUShort(&ok);

            if (peerUuid.isEmpty() || !ok || peerTcpPort == 0)
            {
                qWarning() << "NM::processUdpResponseToNeed: Invalid REQNEED message:" << message;
                continue;
            }

            emit serverStatusMessage(tr("UDP Discovery (REQNEED received on temp port): Peer %1 (UUID: %2) at %3 responded with TCP Port: %4. Attempting TCP connection.")
                                         .arg(peerNameHint.isEmpty() ? peerUuid : peerNameHint)
                                         .arg(peerUuid)
                                         .arg(senderAddress.toString())
                                         .arg(peerTcpPort));
            connectToHost(peerNameHint.isEmpty() ? peerUuid : peerNameHint, peerUuid, senderAddress.toString(), peerTcpPort);

            // Assuming one REQNEED is sufficient, cleanup immediately.
            // If multiple responses could arrive for one NEED, this logic would need adjustment.
            cleanupTemporaryUdpResponseListener();
            return; // Processed one REQNEED, exit loop and function
        }
        else
        {
            qWarning() << "NM::processUdpResponseToNeed: Expected REQNEED, got:" << messageType;
        }
    }
}

// New slot implementation
void NetworkManager::handleUdpResponseListenerTimeout()
{
    qDebug() << "NM::handleUdpResponseListenerTimeout: Timeout waiting for REQNEED on temporary port.";
    emit serverStatusMessage(tr("UDP Discovery: Timeout waiting for a direct response to our NEED request."));
    cleanupTemporaryUdpResponseListener();
}

// New slot implementation
void NetworkManager::handleTemporaryUdpSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (udpTemporaryResponseListenerSocket)
    {
        qWarning() << "NM::handleTemporaryUdpSocketError: Error on temporary UDP listener socket:" << udpTemporaryResponseListenerSocket->errorString();
        emit serverStatusMessage(tr("Error on temporary UDP response listener: %1").arg(udpTemporaryResponseListenerSocket->errorString()));
        cleanupTemporaryUdpResponseListener(); // Cleanup on error
    }
}

void NetworkManager::setUdpDiscoveryPreferences(bool enabled, quint16 port, bool continuousBroadcast, int broadcastIntervalSeconds)
{
    bool portChanged = (currentUdpDiscoveryPort != port);
    bool enabledChanged = (udpDiscoveryEnabled != enabled);
    bool continuousChanged = (udpContinuousBroadcastEnabled != continuousBroadcast);
    bool intervalChanged = (udpBroadcastIntervalSeconds != broadcastIntervalSeconds);

    if (!enabledChanged && !portChanged && !continuousChanged && !intervalChanged) return;

    qDebug() << "NM::setUdpDiscoveryPreferences: UDP Discovery" << (enabled ? "enabled" : "disabled")
             << "on port" << port
             << "Continuous:" << (continuousBroadcast ? "enabled" : "disabled")
             << "Interval:" << broadcastIntervalSeconds << "s";

    udpDiscoveryEnabled = enabled;
    currentUdpDiscoveryPort = port;
    udpContinuousBroadcastEnabled = continuousBroadcast;
    udpBroadcastIntervalSeconds = (broadcastIntervalSeconds > 0) ? broadcastIntervalSeconds : DEFAULT_UDP_BROADCAST_INTERVAL_SECONDS;


    if (udpDiscoveryEnabled) {
        // If settings changed that require a restart of the listener/timer
        if (enabledChanged || portChanged || continuousChanged || intervalChanged) {
            if (udpDiscoveryListenerSocket && udpDiscoveryListenerSocket->state() != QAbstractSocket::UnconnectedState) {
                 // Stop existing discovery first if it's running, especially if port changed or it's being disabled/reconfigured
                stopUdpDiscovery(); 
            }
            startUdpDiscovery(); // This will handle initial broadcast and timer based on new settings
        }
    } else { // UDP Discovery is being disabled
        stopUdpDiscovery(); // This stops the listener and the broadcast timer
        cleanupTemporaryUdpResponseListener(); 
        emit serverStatusMessage(tr("UDP Discovery disabled."));
    }

    // Emit status based on the new state
    if (udpDiscoveryEnabled) {
        QString status = tr("UDP Discovery enabled on port %1.").arg(currentUdpDiscoveryPort);
        if (udpContinuousBroadcastEnabled) {
            status += tr(" Continuous broadcast every %1 seconds.").arg(udpBroadcastIntervalSeconds);
        } else {
            status += tr(" Continuous broadcast disabled (sends once on start/manual trigger).");
        }
        emit serverStatusMessage(status);
    }
}
// 重载的非完全参数的setUdpDiscoveryPreferences,用于兼容旧版本
void NetworkManager::setUdpDiscoveryPreferences(bool enabled, quint16 port)
{
    // Call the new method with all parameters, keeping current continuous broadcast and interval settings
    setUdpDiscoveryPreferences(enabled, port, udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds);
}
