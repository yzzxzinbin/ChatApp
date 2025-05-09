#include "networkmanager.h"
#include <QNetworkInterface>
#include <QDataStream> // Required for QDataStream

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent), tcpServer(nullptr), clientSocket(nullptr), pendingClientSocket(nullptr), defaultPort(60248),
      preferredListenPort(60248), // 初始化首选监听端口为默认值
      preferredOutgoingPortNumber(0), bindToSpecificOutgoingPort(false)
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

void NetworkManager::connectToHost(const QString &ipAddress, quint16 port)
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

    connect(clientSocket, &QTcpSocket::connected, this, [this, ipAddress, port](){ // 捕获 port 以便在连接时使用
        this->currentPeerName = ipAddress; // 或者从 AddContactDialog 传递过来的名称
        this->connectedPeerPort = port; // 存储连接的远程端口
        emit connected(); // 发送连接成功信号
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
        emit incomingConnectionRequest(pendingClientSocket->peerAddress().toString(), pendingClientSocket->peerPort());
    }
}

void NetworkManager::acceptPendingConnection(const QString& peerName)
{
    if (!pendingClientSocket) return;

    if (clientSocket && clientSocket->isOpen()) {
        emit serverStatusMessage(QString("Cannot accept new connection, already connected to %1.").arg(currentPeerName));
        pendingClientSocket->disconnectFromHost();
        pendingClientSocket->deleteLater();
        pendingClientSocket = nullptr;
        return;
    }

    clientSocket = pendingClientSocket;
    pendingClientSocket = nullptr;

    this->currentPeerName = peerName.isEmpty() ? clientSocket->peerAddress().toString() : peerName;

    connect(clientSocket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, this, &NetworkManager::onSocketReadyRead);
    connect(clientSocket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);
    
    emit connected();
    emit serverStatusMessage(QString("Client connected: %1:%2 (Named: %3)").arg(clientSocket->peerAddress().toString()).arg(clientSocket->peerPort()).arg(currentPeerName));
}

void NetworkManager::rejectPendingConnection()
{
    if (pendingClientSocket) {
        pendingClientSocket->disconnectFromHost();
        pendingClientSocket->deleteLater();
        pendingClientSocket = nullptr;
        emit serverStatusMessage("Incoming connection rejected by user.");
    }
}

void NetworkManager::onSocketDisconnected()
{
    emit disconnected();
    emit serverStatusMessage(QString("Socket disconnected from %1.").arg(currentPeerName));
    currentPeerName.clear();
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
            emit newMessageReceived(message);
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
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
        return qMakePair(currentPeerName.isEmpty() ? clientSocket->peerAddress().toString() : currentPeerName, clientSocket->peerPort());
    }
    return qMakePair(QString(), 0);
}

QString NetworkManager::getLastError() const
{
    return lastError;
}
