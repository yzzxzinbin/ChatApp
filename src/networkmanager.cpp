#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream>
#include <QXmlStreamReader> // For parsing simple XML-like messages
#include <QRegularExpression> // For parsing

// Define system message constants and formats
const QString SYS_MSG_HELLO_FORMAT = QStringLiteral("<SYS_HELLO UUID=\"%1\" NameHint=\"%2\"/>");
const QString SYS_MSG_SESSION_ACCEPTED_FORMAT = QStringLiteral("<SYS_SESSION_ACCEPTED UUID=\"%1\" Name=\"%2\"/>");

// Helper function to extract attribute from simple XML-like string
QString extractAttribute(const QString& message, const QString& attributeName) {
    QRegularExpression regex(QStringLiteral("%1=\\\"([^\\\"]*)\\\"").arg(attributeName));
    QRegularExpressionMatch match = regex.match(message);
    if (match.hasMatch() && match.capturedTexts().size() > 1) {
        return match.captured(1);
    }
    return QString();
}

// 定义系统消息常量
NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent), tcpServer(nullptr), clientSocket(nullptr), pendingClientSocket(nullptr), defaultPort(60248),
      preferredListenPort(60248), // 初始化首选监听端口为默认值
      preferredOutgoingPortNumber(0), bindToSpecificOutgoingPort(false),
      isWaitingForPeerConfirmation(false), pendingConnectedPeerPort(0), // 初始化新成员
      localUserUuid(), localUserDisplayName() // localUserUuid, localUserDisplayName 会在 setLocalUserDetails 中设置
{
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    connect(tcpServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError socketError){
        lastError = tcpServer->errorString();
        emit networkError(socketError);
        emit serverStatusMessage(QString("Server Error: %1").arg(lastError));
    });
}

NetworkManager::~NetworkManager()
{
    stopListening();
    if (clientSocket && clientSocket->isOpen()) {
        clientSocket->disconnectFromHost();
        clientSocket->waitForDisconnected(1000);
    }
    if (pendingClientSocket) {
        pendingClientSocket->disconnectFromHost();
        pendingClientSocket->deleteLater();
    }
}

void NetworkManager::setupServer()
{
    if (tcpServer) {
        tcpServer->close();
        delete tcpServer; 
        tcpServer = nullptr;
    }

    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    connect(tcpServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError socketError){
        lastError = tcpServer->errorString();
        emit networkError(socketError);
        emit serverStatusMessage(QString("Server Error: %1").arg(lastError));
    });
}

bool NetworkManager::startListening()
{
    if (tcpServer->isListening()) {
        emit serverStatusMessage(QString("Server is already listening on port %1.").arg(tcpServer->serverPort()));
        return true;
    }

    setupServer();

    bool success = false;
    quint16 portToListen = preferredListenPort > 0 ? preferredListenPort : defaultPort; // 使用首选端口，否则用默认

    if (portToListen == 0) { // 避免监听端口0
        portToListen = defaultPort; // 如果首选和默认都是0（不太可能），则强制使用一个非0默认值
        if (portToListen == 0) portToListen = 60248; // 最后的保障
    }
    
    success = tcpServer->listen(QHostAddress::Any, portToListen);

    if (!success) {
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
    if (clientSocket) {
         if(clientSocket->state() == QAbstractSocket::ConnectedState) {
            clientSocket->disconnectFromHost();
         }
         clientSocket->deleteLater();
         clientSocket = nullptr;
         currentPeerName.clear();
    }
    if (pendingClientSocket) {
        pendingClientSocket->abort();
        pendingClientSocket->deleteLater();
        pendingClientSocket = nullptr;
    }
}

void NetworkManager::connectToHost(const QString &peerNameToSet, const QString &ipAddress, quint16 port) // 修改签名
{
    if (clientSocket && clientSocket->isOpen()) {
        clientSocket->disconnectFromHost();
        if (clientSocket->state() != QAbstractSocket::UnconnectedState) {
             clientSocket->waitForDisconnected(1000); 
        }
    }
    
    // 先删除旧的 socket（如果存在）
    if(clientSocket) {
        clientSocket->deleteLater();
        clientSocket = nullptr;
        currentPeerName.clear(); // 也清除旧的对端名称
    }
    
    clientSocket = new QTcpSocket(this);

    // 在 connectToHost 之前尝试绑定本地端口
    if (bindToSpecificOutgoingPort && preferredOutgoingPortNumber > 0) {
        if (!clientSocket->bind(QHostAddress::AnyIPv4, preferredOutgoingPortNumber)) { // 或者 QHostAddress::Any 用于双栈
            emit serverStatusMessage(tr("Warning: Could not bind to outgoing port %1. Error: %2. Proceeding with dynamic port.")
                                     .arg(preferredOutgoingPortNumber).arg(clientSocket->errorString()));
            // 绑定失败，操作系统将选择一个动态端口
        } else {
            emit serverStatusMessage(tr("Successfully bound to outgoing port %1 for next connection.").arg(preferredOutgoingPortNumber));
        }
    } else if (bindToSpecificOutgoingPort && preferredOutgoingPortNumber == 0) {
        // 如果用户选择指定端口但输入0，也视为动态（或警告用户）
         emit serverStatusMessage(tr("Outgoing port set to 0 with 'specify' option, OS will choose a dynamic port."));
    }

    connect(clientSocket, &QTcpSocket::connected, this, [this, peerNameToSet, port](){ // 捕获 peerNameToSet 和 port
        // TCP连接已建立，但等待对方应用层确认
        this->isWaitingForPeerConfirmation = true;
        this->pendingPeerNameToSet = peerNameToSet;
        this->pendingConnectedPeerPort = port; // 存储远程端口

        // Send SYS_HELLO message
        QString helloMessage = SYS_MSG_HELLO_FORMAT.arg(localUserUuid).arg(localUserDisplayName);
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_5);
        out << helloMessage;
        clientSocket->write(block);
        clientSocket->flush();
        emit serverStatusMessage(tr("Sent HELLO to %1.").arg(peerNameToSet));

        emit tcpLinkEstablished(peerNameToSet); // 发出TCP链路建立信号
    });
    connect(clientSocket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, this, &NetworkManager::onSocketReadyRead);
    connect(clientSocket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);

    emit serverStatusMessage(QString("Attempting to connect to %1:%2...").arg(ipAddress).arg(port));
    clientSocket->connectToHost(ipAddress, port);
}

void NetworkManager::disconnectFromHost()
{
    if (clientSocket && clientSocket->isOpen()) {
        clientSocket->disconnectFromHost();
        currentPeerName.clear();
    }
}

void NetworkManager::sendMessage(const QString &message)
{
    if (clientSocket && clientSocket->isOpen() && clientSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_5);
        out << message;
        clientSocket->write(block);
        clientSocket->flush();
    } else {
         lastError = "No active connection.";
         emit serverStatusMessage("Cannot send message: No active connection.");
    }
}

void NetworkManager::setListenPreferences(quint16 port)
{
    preferredListenPort = (port > 0) ? port : defaultPort; // 确保首选端口不为0
}

void NetworkManager::setOutgoingConnectionPreferences(quint16 port, bool useSpecific)
{
    preferredOutgoingPortNumber = port;
    bindToSpecificOutgoingPort = useSpecific;
}

void NetworkManager::setLocalUserDetails(const QString& uuid, const QString& displayName)
{
    this->localUserUuid = uuid;
    this->localUserDisplayName = displayName;
}

void NetworkManager::onNewConnection()
{
    if (pendingClientSocket || (clientSocket && clientSocket->isOpen())) {
        QTcpSocket *extraSocket = tcpServer->nextPendingConnection();
        if (extraSocket) {
            extraSocket->disconnectFromHost();
            extraSocket->deleteLater();
            emit serverStatusMessage("Busy: Rejected new incoming connection attempt.");
        }
        return;
    }

    pendingClientSocket = tcpServer->nextPendingConnection();
    if (pendingClientSocket) {
        emit serverStatusMessage(tr("Pending connection from %1:%2. Waiting for HELLO.")
                                 .arg(pendingClientSocket->peerAddress().toString())
                                 .arg(pendingClientSocket->peerPort()));
        connect(pendingClientSocket, &QTcpSocket::readyRead, this, &NetworkManager::onPendingSocketReadyRead);
        connect(pendingClientSocket, &QTcpSocket::disconnected, this, &NetworkManager::onPendingSocketDisconnected);
        connect(pendingClientSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::onPendingSocketError);
    }
}

void NetworkManager::onPendingSocketReadyRead()
{
    if (!pendingClientSocket || !pendingClientSocket->isValid() || pendingClientSocket->bytesAvailable() == 0) return;

    QDataStream in(pendingClientSocket);
    in.setVersion(QDataStream::Qt_6_5);

    if (pendingClientSocket->bytesAvailable() < (int)sizeof(quint32)) return;

    in.startTransaction();
    QString message;
    in >> message;

    if (in.commitTransaction()) {
        if (message.startsWith("<SYS_HELLO")) {
            tempPeerUuidFromHello = extractAttribute(message, "UUID");
            tempPeerNameHintFromHello = extractAttribute(message, "NameHint");

            if (tempPeerUuidFromHello.isEmpty()) {
                emit serverStatusMessage(tr("Error: Received HELLO from %1 without UUID. Rejecting.")
                                         .arg(pendingClientSocket->peerAddress().toString()));
                rejectPendingConnection(); // Or just close pendingClientSocket
                return;
            }
            
            emit serverStatusMessage(tr("Received HELLO from %1 (UUID: %2, Hint: %3).")
                                     .arg(pendingClientSocket->peerAddress().toString())
                                     .arg(tempPeerUuidFromHello)
                                     .arg(tempPeerNameHintFromHello));

            // Disconnect HELLO-specific slots, as we'll handle future comms differently if accepted
            disconnect(pendingClientSocket, &QTcpSocket::readyRead, this, &NetworkManager::onPendingSocketReadyRead);
            // Do not disconnect 'disconnected' or 'error' yet, they are still relevant for pendingClientSocket

            emit incomingConnectionRequest(pendingClientSocket->peerAddress().toString(),
                                           pendingClientSocket->peerPort(),
                                           tempPeerUuidFromHello,
                                           tempPeerNameHintFromHello);
        } else {
            emit serverStatusMessage(tr("Error: Expected HELLO from %1, got: %2. Closing.")
                                     .arg(pendingClientSocket->peerAddress().toString()).arg(message.left(50)));
            if (pendingClientSocket) { // Check again as reject might nullify it
                pendingClientSocket->abort(); // Force close
                delete pendingClientSocket;
                pendingClientSocket = nullptr;
            }
            tempPeerUuidFromHello.clear();
            tempPeerNameHintFromHello.clear();
        }
    } else {
        // Transaction failed, data incomplete. Wait for more.
    }
}

void NetworkManager::onPendingSocketDisconnected()
{
    if (pendingClientSocket) {
        emit serverStatusMessage(tr("Pending connection from %1 disconnected before session establishment.")
                                 .arg(pendingClientSocket->peerAddress().toString()));
        pendingClientSocket->deleteLater();
        pendingClientSocket = nullptr;
        tempPeerUuidFromHello.clear();
        tempPeerNameHintFromHello.clear();
    }
}

void NetworkManager::onPendingSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (pendingClientSocket) {
        emit serverStatusMessage(tr("Error on pending connection from %1: %2")
                                 .arg(pendingClientSocket->peerAddress().toString())
                                 .arg(pendingClientSocket->errorString()));
        pendingClientSocket->deleteLater(); // Clean up
        pendingClientSocket = nullptr;
        tempPeerUuidFromHello.clear();
        tempPeerNameHintFromHello.clear();
    }
}

void NetworkManager::acceptPendingConnection(const QString& peerName)
{
    if (!pendingClientSocket) {
        emit serverStatusMessage(tr("Error: No pending connection to accept."));
        return;
    }
    if (tempPeerUuidFromHello.isEmpty()) {
         emit serverStatusMessage(tr("Error: Cannot accept connection, peer UUID from HELLO is missing."));
         rejectPendingConnection(); // This will clean up pendingClientSocket
         return;
    }

    clientSocket = pendingClientSocket;
    pendingClientSocket = nullptr; // pendingClientSocket is now clientSocket

    this->currentPeerName = peerName.isEmpty() ? (tempPeerNameHintFromHello.isEmpty() ? clientSocket->peerAddress().toString() : tempPeerNameHintFromHello) : peerName;
    this->currentPeerUuid = tempPeerUuidFromHello; // Store the UUID received in HELLO
    this->connectedPeerPort = clientSocket->peerPort();

    // Clear temporary HELLO data
    tempPeerUuidFromHello.clear();
    tempPeerNameHintFromHello.clear();

    connect(clientSocket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, this, &NetworkManager::onSocketReadyRead);
    connect(clientSocket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);
    
    // Send session accepted confirmation message to the client, including server's UUID and name
    QString acceptedMessage = SYS_MSG_SESSION_ACCEPTED_FORMAT.arg(localUserUuid).arg(localUserDisplayName);
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << acceptedMessage;
    clientSocket->write(block);
    clientSocket->flush();

    emit connected(); // 服务器端连接完成
    emit serverStatusMessage(QString("Client connected: %1 (UUID: %2, Addr: %3:%4). Sent session acceptance.")
                             .arg(currentPeerName)
                             .arg(currentPeerUuid)
                             .arg(clientSocket->peerAddress().toString())
                             .arg(clientSocket->peerPort()));
}

void NetworkManager::rejectPendingConnection()
{
    if (pendingClientSocket) {
        pendingClientSocket->abort(); // Force close
        // deleteLater will be called by onPendingSocketDisconnected if connected
        // If not connected yet, or if onPendingSocketDisconnected doesn't fire, ensure cleanup
        if(pendingClientSocket) pendingClientSocket->deleteLater();
        pendingClientSocket = nullptr;
        emit serverStatusMessage("Incoming connection rejected.");
    }
    tempPeerUuidFromHello.clear();
    tempPeerNameHintFromHello.clear();
}

void NetworkManager::onSocketDisconnected()
{
    emit disconnected();
    emit serverStatusMessage(QString("Socket disconnected from %1.").arg(currentPeerName.isEmpty() ? pendingPeerNameToSet : currentPeerName));
    currentPeerName.clear();
    currentPeerUuid.clear(); // 清除对端UUID
    pendingPeerNameToSet.clear();
    pendingPeerUuidToSet.clear(); // 清除临时对端UUID
    isWaitingForPeerConfirmation = false;
    pendingConnectedPeerPort = 0;
    if (clientSocket) {
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
}

void NetworkManager::onSocketReadyRead()
{
    if (!clientSocket || !clientSocket->isValid() || clientSocket->bytesAvailable() == 0) return;

    QDataStream in(clientSocket);
    in.setVersion(QDataStream::Qt_6_5);

    while(clientSocket->bytesAvailable() > 0) {
        if (clientSocket->bytesAvailable() < (int)sizeof(quint32))
            return;

        in.startTransaction();
        QString message;
        in >> message;

        if (in.commitTransaction()) {
            if (isWaitingForPeerConfirmation && message.startsWith("<SYS_SESSION_ACCEPTED")) {
                QString serverUuid = extractAttribute(message, "UUID");
                QString serverNameHint = extractAttribute(message, "Name"); // Server's name as it knows itself

                if (serverUuid.isEmpty()) {
                    emit serverStatusMessage(tr("Error: Received SESSION_ACCEPTED without UUID from %1. Disconnecting.")
                                             .arg(pendingPeerNameToSet));
                    disconnectFromHost();
                    return;
                }

                isWaitingForPeerConfirmation = false;
                currentPeerName = pendingPeerNameToSet; // Client uses the name it initiated with
                currentPeerUuid = serverUuid;           // Store server's UUID
                connectedPeerPort = pendingConnectedPeerPort;

                pendingPeerNameToSet.clear();
                pendingPeerUuidToSet.clear();
                pendingConnectedPeerPort = 0;

                emit serverStatusMessage(tr("Session with %1 (UUID: %2, Reported Name: %3) accepted by peer.")
                                         .arg(currentPeerName).arg(currentPeerUuid).arg(serverNameHint));
                emit connected(); // 客户端应用层连接完成
            } else if (message.startsWith("<SYS_SESSION_ACCEPTED") && !isWaitingForPeerConfirmation) {
                emit serverStatusMessage(tr("Received unexpected session acceptance from %1.").arg(currentPeerName.isEmpty() ? clientSocket->peerAddress().toString() : currentPeerName));
            }
            else if (message.startsWith("<SYS_HELLO")) {
                emit serverStatusMessage(tr("Received unexpected HELLO from %1.").arg(currentPeerName.isEmpty() ? clientSocket->peerAddress().toString() : currentPeerName));
            }
            else {
                emit newMessageReceived(message);
            }
        } else {
            break; 
        }
    }
}

void NetworkManager::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (clientSocket) {
         lastError = clientSocket->errorString();
         emit networkError(clientSocket->error());
         emit serverStatusMessage(QString("Socket Error (%1): %2").arg(currentPeerName).arg(lastError));
    } else if (pendingClientSocket) {
        lastError = pendingClientSocket->errorString();
        emit networkError(pendingClientSocket->error());
        emit serverStatusMessage(QString("Pending Socket Error: %1").arg(lastError));
    } else {
        lastError = "Unknown socket error.";
        emit serverStatusMessage(lastError);
    }
}

QAbstractSocket::SocketState NetworkManager::getCurrentSocketState() const
{
    if (clientSocket) {
        return clientSocket->state();
    }
    return QAbstractSocket::UnconnectedState;
}

QPair<QString, quint16> NetworkManager::getPeerInfo() const {
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState && !isWaitingForPeerConfirmation) { // 只有在完全连接后才提供信息
        return qMakePair(currentPeerName.isEmpty() ? clientSocket->peerAddress().toString() : currentPeerName, connectedPeerPort);
    } else if (isWaitingForPeerConfirmation && !pendingPeerNameToSet.isEmpty()) {
        return qMakePair(pendingPeerNameToSet, pendingConnectedPeerPort);
    }
    return qMakePair(QString(), 0);
}

QString NetworkManager::getCurrentPeerUuid() const
{
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState && !isWaitingForPeerConfirmation) {
        return currentPeerUuid;
    }
    return QString();
}

QString NetworkManager::getCurrentPeerIpAddress() const
{
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState && !isWaitingForPeerConfirmation) {
        return clientSocket->peerAddress().toString();
    }
    // For pending connections (client-side, before SYS_SESSION_ACCEPTED)
    else if (clientSocket && isWaitingForPeerConfirmation && clientSocket->peerAddress().isNull() == false) {
         return clientSocket->peerAddress().toString();
    }
    // For pending connections (server-side, before acceptPendingConnection promotes it to clientSocket)
    else if (pendingClientSocket && pendingClientSocket->peerAddress().isNull() == false) {
        return pendingClientSocket->peerAddress().toString();
    }
    return QString();
}

QString NetworkManager::getLastError() const
{
    return lastError;
}
