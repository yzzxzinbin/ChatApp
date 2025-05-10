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
      udpSocket(nullptr),             // 新增：初始化udpSocket
      udpBroadcastTimer(nullptr),     // 新增：初始化udpBroadcastTimer
      defaultPort(60248),
      preferredListenPort(60248),
      autoStartListeningEnabled(true), // 默认启用监听
      udpDiscoveryEnabled(false),      // 新增：默认禁用UDP发现
      retryListenTimer(nullptr),
      retryListenIntervalMs(15000),   // 默认15秒重试间隔
      preferredOutgoingPortNumber(0),
      bindToSpecificOutgoingPort(false),
      localUserUuid(),
      localUserDisplayName()
{
    setupServer(); // Initial setup for the server
    retryListenTimer = new QTimer(this);
    connect(retryListenTimer, &QTimer::timeout, this, &NetworkManager::attemptToListen);

    // 新增：初始化UDP广播定时器
    udpBroadcastTimer = new QTimer(this);
    connect(udpBroadcastTimer, &QTimer::timeout, this, &NetworkManager::sendUdpBroadcast);
}

NetworkManager::~NetworkManager()
{
    if (retryListenTimer) {
        retryListenTimer->stop();
    }
    if (udpBroadcastTimer) { // 新增：停止UDP广播定时器
        udpBroadcastTimer->stop();
    }

    stopListening(); // This will also clean up connected sockets
    stopUdpDiscovery(); // 新增：确保UDP也停止

    // Clean up any remaining pending sockets
    qDeleteAll(pendingIncomingSockets);
    pendingIncomingSockets.clear();

    qDeleteAll(outgoingSocketsAwaitingSessionAccepted.keys());
    outgoingSocketsAwaitingSessionAccepted.clear();

    if (tcpServer) {
        tcpServer->deleteLater(); // Ensure server is deleted
        tcpServer = nullptr;
    }
    if (udpSocket) { // 新增：清理UDP套接字
        udpSocket->deleteLater();
        udpSocket = nullptr;
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
    if (!autoStartListeningEnabled) {
        emit serverStatusMessage(tr("Network listening is disabled by user settings."));
        if (tcpServer->isListening()) { // 如果之前在监听，现在禁用了，则停止
            stopListening(); // stopListening 会处理 retryListenTimer
        } else {
            if (retryListenTimer->isActive()) {
                retryListenTimer->stop();
            }
        }
        return false;
    }

    if (tcpServer->isListening()) {
        emit serverStatusMessage(QString("Server is already listening on port %1.").arg(tcpServer->serverPort()));
        if(retryListenTimer->isActive()) retryListenTimer->stop(); // 如果成功，停止重试
        return true;
    }

    // Ensure server object is fresh if it was previously stopped/failed
    setupServer();

    quint16 portToListen = preferredListenPort > 0 ? preferredListenPort : defaultPort;
    if (portToListen == 0) portToListen = defaultPort; // Fallback
    if (portToListen == 0) portToListen = 60248; // Absolute fallback

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

    if(retryListenTimer->isActive()) retryListenTimer->stop(); // 成功启动，停止重试计时器

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
        startListening(); // startListening 内部会处理下一次重试的启动（如果失败）
    } else {
        // 如果用户禁用了监听，或者已经成功监听，则停止计时器
        retryListenTimer->stop();
    }
}

void NetworkManager::stopListening()
{
    if (retryListenTimer && retryListenTimer->isActive()) { // 停止重试计时器
        retryListenTimer->stop();
        emit serverStatusMessage(tr("Automatic listen retry stopped."));
    }

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
    // Add loopback explicitly as it's a common way to self-connect
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
        // Not listening, so can't connect to self via our listening port
        return false;
    }

    quint16 listeningPort = tcpServer->serverPort();
    if (targetPort != listeningPort) {
        // Target port is different from our listening port
        return false;
    }

    QHostAddress targetAddress(targetHost);
    // QHostAddress constructor handles "localhost" and IP strings.
    // If targetHost is a hostname that needs resolution, this check might be basic.
    // For direct IP or "localhost", it's fine.
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
    Q_UNUSED(targetPeerUuidHint); // targetPeerUuidHint might be used later for re-establishing with known UUIDs
    qDebug() << "NM::connectToHost: Attempting to connect to Name:" << peerNameToSet
             << "IP:" << hostAddress << "Port:" << port
             << "My UUID:" << localUserUuid << "My NameHint:" << localUserDisplayName;

    if (isSelfConnection(hostAddress, port)) {
        qWarning() << "NM::connectToHost: Attempt to connect to self (" << hostAddress << ":" << port << ") aborted.";
        emit serverStatusMessage(tr("Attempt to connect to self (%1:%2) was aborted.").arg(hostAddress).arg(port));
        // Optionally, emit outgoingConnectionFailed if the UI needs to react to this specific failure.
        // For example: emit outgoingConnectionFailed(peerNameToSet, tr("Cannot connect to self"));
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

void NetworkManager::setListenPreferences(quint16 port, bool autoStartListen) // 修改了签名
{
    preferredListenPort = (port > 0) ? port : defaultPort;
    bool oldAutoStartListeningEnabled = autoStartListeningEnabled;
    autoStartListeningEnabled = autoStartListen; // 设置用户是否希望监听

    // 如果用户禁用了监听，并且服务器正在监听或尝试重试，则停止它
    if (!autoStartListeningEnabled) {
        if (tcpServer && tcpServer->isListening()) {
            stopListening(); // stopListening 会处理 retryListenTimer
        } else if (retryListenTimer && retryListenTimer->isActive()) {
            retryListenTimer->stop();
            emit serverStatusMessage(tr("Network listening disabled by user. Retry stopped."));
        }
    } else if (autoStartListeningEnabled && !oldAutoStartListeningEnabled && (!tcpServer || !tcpServer->isListening())) {
        // 如果从禁用变为启用，并且当前未监听，则尝试启动
        // 这通常由 MainWindow::handleSettingsApplied 中的 startListening() 调用处理，但作为备用
        emit serverStatusMessage(tr("Network listening enabled. Will attempt to start if not already running."));
        // startListening(); // 通常由外部调用
    }
}

// 实现 setOutgoingConnectionPreferences
void NetworkManager::setOutgoingConnectionPreferences(quint16 port, bool useSpecific)
{
    preferredOutgoingPortNumber = port;
    bindToSpecificOutgoingPort = useSpecific;
    qDebug() << "NM::setOutgoingConnectionPreferences: Preferred Outgoing Port:" << preferredOutgoingPortNumber
             << "Use Specific:" << bindToSpecificOutgoingPort;
    // 此设置通常应用于新的传出连接，
    // 因此除了存储值之外，通常此处不执行任何立即操作。
    // connectToHost 方法将使用这些值。
    emit serverStatusMessage(tr("Outgoing connection port preferences updated. Port: %1, Specific: %2")
                             .arg(port == 0 ? tr("Dynamic") : QString::number(port))
                             .arg(useSpecific ? tr("Yes") : tr("No")));
}

// 新增：设置UDP发现首选项
void NetworkManager::setUdpDiscoveryPreferences(bool enabled)
{
    if (udpDiscoveryEnabled == enabled) return; // 没有变化

    udpDiscoveryEnabled = enabled;
    if (udpDiscoveryEnabled) {
        startUdpDiscovery();
    } else {
        stopUdpDiscovery();
    }
}

// 新增：启动UDP发现
void NetworkManager::startUdpDiscovery()
{
    if (!udpDiscoveryEnabled) {
        emit serverStatusMessage(tr("UDP discovery is disabled by user settings."));
        return;
    }

    if (udpSocket && udpSocket->state() != QAbstractSocket::UnconnectedState) {
        emit serverStatusMessage(tr("UDP discovery is already active on port %1.").arg(DISCOVERY_UDP_PORT));
        if (!udpBroadcastTimer->isActive()) { // 确保广播定时器在运行
             udpBroadcastTimer->start(UDP_BROADCAST_INTERVAL_MS);
        }
        return;
    }

    if (udpSocket) { // 清理旧的socket以防万一
        udpSocket->close();
        udpSocket->deleteLater();
    }

    udpSocket = new QUdpSocket(this);
    connect(udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::processPendingUdpDatagrams);
    connect(udpSocket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError){
        Q_UNUSED(socketError);
        emit serverStatusMessage(tr("UDP Socket Error: %1").arg(udpSocket->errorString()));
        // 可以选择在这里停止并尝试重启UDP发现，或者仅记录错误
        // stopUdpDiscovery();
        // if(udpDiscoveryEnabled) QTimer::singleShot(1000, this, &NetworkManager::startUdpDiscovery); // 1秒后尝试重启
    });


    if (udpSocket->bind(QHostAddress::AnyIPv4, DISCOVERY_UDP_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit serverStatusMessage(tr("UDP discovery started, listening on port %1.").arg(DISCOVERY_UDP_PORT));
        udpBroadcastTimer->start(UDP_BROADCAST_INTERVAL_MS);
        sendUdpBroadcast(); // 立即发送一次广播
    } else {
        emit serverStatusMessage(tr("UDP discovery could not start on port %1: %2").arg(DISCOVERY_UDP_PORT).arg(udpSocket->errorString()));
        if (udpSocket) {
            udpSocket->deleteLater();
            udpSocket = nullptr;
        }
    }
}

// 新增：停止UDP发现
void NetworkManager::stopUdpDiscovery()
{
    if (udpBroadcastTimer && udpBroadcastTimer->isActive()) {
        udpBroadcastTimer->stop();
    }
    if (udpSocket) {
        if (udpSocket->state() != QAbstractSocket::UnconnectedState) {
            udpSocket->close();
            emit serverStatusMessage(tr("UDP discovery stopped."));
        }
        // udpSocket->deleteLater(); // 考虑是否立即删除或在析构时删除
        // udpSocket = nullptr;
    }
}

// 新增UDP相关槽函数实现
void NetworkManager::sendUdpBroadcast()
{
    if (!udpSocket || !udpDiscoveryEnabled || localUserUuid.isEmpty() || !tcpServer || !tcpServer->isListening()) {
        // 如果UDP未启用，或本地信息不完整，或TCP服务器未监听（无法告知对方TCP端口），则不广播
        if (udpDiscoveryEnabled && (!tcpServer || !tcpServer->isListening())) {
             qDebug() << "NM::sendUdpBroadcast: Skipping broadcast because TCP server is not listening.";
        }
        return;
    }

    QString message = QString("%1;UUID=%2;Name=%3;TCPPort=%4;")
                          .arg(UDP_DISCOVERY_MSG_PREFIX)
                          .arg(localUserUuid)
                          .arg(localUserDisplayName) // 不再进行 HTML 转义
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
        quint16 senderPort; // 这是UDP发送方端口，不是对方TCP监听端口

        udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        QString message = QString::fromUtf8(datagram);
        qDebug() << "NM::processPendingUdpDatagrams: Received from" << senderAddress.toString() << ":" << senderPort << "Data:" << message;

        if (!message.startsWith(UDP_DISCOVERY_MSG_PREFIX)) { // 使用头文件中定义的常量
            qDebug() << "NM::processPendingUdpDatagrams: Ignoring non-discovery message:" << message;
            continue;
        }

        QStringList parts = message.split(';', Qt::SkipEmptyParts);
        // Prefix, UUID, Name, TCPPort - 至少需要这4个部分
        if (parts.length() < 4 || parts[0] != UDP_DISCOVERY_MSG_PREFIX) { 
            qWarning() << "NM::processPendingUdpDatagrams: Malformed or non-matching prefix discovery message:" << message;
            continue;
        }

        QString peerUuid;
        QString peerNameHint;
        quint16 peerTcpPort = 0;

        for (const QString& part : parts) {
            if (part.startsWith("UUID=")) {
                peerUuid = part.mid(4); // "UUID=" 是4个字符
            } else if (part.startsWith("Name=")) {
                peerNameHint = part.mid(5); // "Name=" 是5个字符
            } else if (part.startsWith("TCPPort=")) {
                bool ok;
                peerTcpPort = part.mid(8).toUShort(&ok); // "TCPPort=" 是8个字符
                if (!ok) {
                    qWarning() << "NM::processPendingUdpDatagrams: Invalid TCPPort in discovery message:" << message;
                    peerTcpPort = 0; // Reset if conversion failed
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

        // 检查是否已连接
        if (connectedSockets.contains(peerUuid)) {
            qDebug() << "NM::processPendingUdpDatagrams: Peer" << peerUuid << "is already connected. Ignoring discovery.";
            // 可以考虑更新IP/端口信息，如果它们变了
            continue;
        }
        
        // 检查是否正在尝试连接到此UUID (避免重复发起)
        // (这部分逻辑比较复杂，暂时简化为不检查，直接尝试连接，connectToHost内部有自连接检查)

        emit serverStatusMessage(tr("UDP Discovery: Found peer %1 (UUID: %2) at %3, TCP Port: %4. Attempting TCP connection.")
                                 .arg(peerNameHint)
                                 .arg(peerUuid)
                                 .arg(senderAddress.toString())
                                 .arg(peerTcpPort));
        
        // 使用发现的IP和TCP端口尝试建立TCP连接
        // 注意：peerNameHint 是对方的名称，我们本地可能还没有为此UUID命名，
        // connectToHost 的第一个参数是我们本地为此连接命名的名称，这里可以用 peerNameHint 或 peerUuid
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
        qDebug() << "NM::PendingIncomingSocketReadyRead: Received message:" << message << "from" << socket->peerAddress().toString();
        if (message.startsWith("<SYS_HELLO")) {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerNameHint = extractAttribute(message, "NameHint");
            qDebug() << "NM::PendingIncomingSocketReadyRead: Extracted peerUUID:" << peerUuid << "NameHint:" << peerNameHint;
            qDebug() << "NM::PendingIncomingSocketReadyRead: Local user UUID for comparison:" << localUserUuid;

            if (peerUuid.isEmpty() || peerUuid == localUserUuid) { // Reject if no UUID or self-connect
                qWarning() << "NM::PendingIncomingSocketReadyRead: Invalid HELLO - peerUUID is empty or matches localUserUuid. PeerUUID:" << peerUuid << "LocalUUID:" << localUserUuid;
                emit serverStatusMessage(tr("Error: Received HELLO from %1 without valid UUID or self-connect. Rejecting.")
                                         .arg(socket->peerAddress().toString()));
                sendSystemMessage(socket, SYS_MSG_SESSION_REJECTED_FORMAT.arg("Invalid HELLO"));
                removePendingIncomingSocket(socket);
                socket->abort(); // Then deleteLater via disconnected
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

            // Disconnect this slot, MainWindow will decide to accept/reject
            disconnect(socket, &QTcpSocket::readyRead, this, &NetworkManager::handlePendingIncomingSocketReadyRead);
            emit incomingSessionRequest(socket, socket->peerAddress().toString(), socket->peerPort(), peerUuid, peerNameHint);
        } else {
            qWarning() << "NM::PendingIncomingSocketReadyRead: Expected HELLO, got:" << message.left(50) << "from" << socket->peerAddress().toString();
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
    qDebug() << "NM::OutgoingSocketConnected: TCP link with" << peerNameToSet << "established. Sending HELLO. My UUID:" << localUserUuid << "My Name:" << localUserDisplayName;
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
        qDebug() << "NM::OutgoingSocketReadyRead: Received message:" << message << "from attempted peer:" << localNameForPeerAttempt;
        if (message.startsWith("<SYS_SESSION_ACCEPTED")) {
            QString peerUuid = extractAttribute(message, "UUID");
            QString peerReportedName = extractAttribute(message, "Name"); // Peer's name for itself
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

            removeOutgoingSocketAwaitingAcceptance(socket); // Remove from temp map
            addEstablishedConnection(socket, peerUuid, localNameForPeerAttempt, socket->peerAddress().toString(), socket->peerPort());

        } else if (message.startsWith("<SYS_SESSION_REJECTED")) {
            QString reason = extractAttribute(message, "Reason");
            qWarning() << "NM::OutgoingSocketReadyRead: Session REJECTED by peer. Peer:" << localNameForPeerAttempt << "Reason:" << reason;
            emit serverStatusMessage(tr("Session with %1 rejected by peer. Reason: %2")
                                     .arg(localNameForPeerAttempt)
                                     .arg(reason.isEmpty() ? "No reason given" : reason));
            emit outgoingConnectionFailed(localNameForPeerAttempt, tr("Session rejected by peer: %1").arg(reason.isEmpty() ? tr("No reason given") : reason));
            removeOutgoingSocketAwaitingAcceptance(socket);
            socket->abort(); // This will trigger disconnected and cleanup
        } else {
            qWarning() << "NM::OutgoingSocketReadyRead: Expected SESSION_ACCEPTED/REJECTED, got:" << message.left(50) << "from" << localNameForPeerAttempt;
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
    qDebug() << "NM::acceptIncomingSession: Attempting to accept session for PeerUUID:" << peerUuid << "LocalName:" << localNameForPeer << "My UUID:" << localUserUuid;
    if (!tempSocket || !pendingIncomingSockets.contains(tempSocket)) {
        qWarning() << "NM::acceptIncomingSession: Socket not found or not pending.";
        emit serverStatusMessage(tr("Error: Cannot accept session, socket not found or not pending."));
        if(tempSocket) tempSocket->deleteLater(); // Clean up if passed but not in list
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


    removePendingIncomingSocket(tempSocket); // Remove from pending list
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
    if (!socket || peerUuid.isEmpty()) {
        qWarning() << "NM::addEstablishedConnection: Invalid socket or empty peerUUID. Cannot add connection.";
        return;
    }
    qDebug() << "NM::addEstablishedConnection: Establishing connection for PeerUUID:" << peerUuid << "Name:" << peerName << "Addr:" << peerAddress << ":" << peerPort;

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
