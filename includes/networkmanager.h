#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QMap>
#include <QStringList> // For getConnectedPeerUuids
#include <QNetworkInterface> // Required for getting local IP addresses
#include <QTimer> // QTimer for retryListenTimer
#include <QUdpSocket> // 用于UDP发现

// Define system message constants and formats
const QString SYS_MSG_HELLO_FORMAT = QStringLiteral("<SYS_HELLO UUID=\"%1\" NameHint=\"%2\"/>");
const QString SYS_MSG_SESSION_ACCEPTED_FORMAT = QStringLiteral("<SYS_SESSION_ACCEPTED UUID=\"%1\" Name=\"%2\"/>");
const QString SYS_MSG_SESSION_REJECTED_FORMAT = QStringLiteral("<SYS_SESSION_REJECTED Reason=\"%1\"/>"); // New

// UDP发现相关常量
const int DEFAULT_UDP_BROADCAST_INTERVAL_SECONDS = 5; // 默认5秒广播一次
const QString UDP_DISCOVERY_MSG_PREFIX = "CHAT_DISCOVERY_V1"; // For ANNOUNCE
const QString UDP_NEED_CONNECTION_PREFIX = "CHAT_NEED_CONN_V1"; // For NEED
const QString UDP_RESPONSE_TO_NEED_PREFIX = "CHAT_RESP_NEED_V1"; // For REQNEED (Response to NEED)
const QString UDP_REPLY_TO_PORT_FIELD_KEY = "ReplyToUDPPort"; // New: Key for reply port in NEED message
const int UDP_TEMP_RESPONSE_LISTENER_TIMEOUT_MS = 15000; // New: Timeout for temporary listener (15s)

// File Transfer Message Formats
const QString FT_MSG_OFFER_FORMAT = QStringLiteral("<FT_OFFER TransferID=\"%1\" FileName=\"%2\" FileSize=\"%3\" SenderUUID=\"%4\"/>");
const QString FT_MSG_ACCEPT_FORMAT = QStringLiteral("<FT_ACCEPT TransferID=\"%1\" ReceiverUUID=\"%2\"/>");
const QString FT_MSG_REJECT_FORMAT = QStringLiteral("<FT_REJECT TransferID=\"%1\" Reason=\"%2\" ReceiverUUID=\"%3\"/>");
// Later: FT_MSG_CHUNK_FORMAT, FT_MSG_EOF_FORMAT, FT_MSG_ACK_FORMAT, FT_MSG_ERROR_FORMAT

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    // 启动服务器监听
    bool startListening();
    
    // 设置监听首选项
    void setListenPreferences(quint16 port, bool autoStartListen); // 修改签名
    // 设置传出连接首选项
    void setOutgoingConnectionPreferences(quint16 port, bool useSpecific);
    
    // 停止服务器监听
    void stopListening();

    // 连接到指定IP和端口
    // peerNameToSet is the local name chosen for this peer, targetPeerUuidHint can be used if known for re-establishing
    void connectToHost(const QString &peerNameToSet, const QString &targetPeerUuidHint, const QString &hostAddress, quint16 port);
    
    // 断开与特定对等方的连接
    void disconnectFromPeer(const QString &peerUuid);

    // 发送消息给特定对等方
    void sendMessage(const QString &targetPeerUuid, const QString &message);

    // 获取特定对等方的socket状态
    QAbstractSocket::SocketState getPeerSocketState(const QString& peerUuid) const;
    // 获取特定对等方的信息 (名称/IP, 端口)
    QPair<QString, quint16> getPeerInfo(const QString& peerUuid) const;
    // 获取特定对等方的IP地址
    QString getPeerIpAddress(const QString& peerUuid) const;
    // 获取所有已连接对等方的UUID列表
    QStringList getConnectedPeerUuids() const;
    // 获取最后发生的通用服务器错误信息
    QString getLastError() const;

    void setLocalUserDetails(const QString& uuid, const QString& displayName);
    void setUdpDiscoveryPreferences(bool enabled, quint16 port); // 保留旧方法以向后兼容
    void setUdpDiscoveryPreferences(bool enabled, quint16 port, bool continuousBroadcast, int broadcastIntervalSeconds); // 新增：具有所有参数的方法
    void startUdpDiscovery();
    void stopUdpDiscovery();

signals:
    // 对等方连接成功信号 (包含UUID, 名称, 地址, 端口)
    void peerConnected(const QString &peerUuid, const QString &peerName, const QString& peerAddress, quint16 peerPort);
    
    // 对等方断开连接信号
    void peerDisconnected(const QString &peerUuid);
    
    // 收到来自特定对等方的新消息信号
    void newMessageReceived(const QString &peerUuid, const QString &message);
    
    // 特定对等方的网络错误信号
    void peerNetworkError(const QString &peerUuid, QAbstractSocket::SocketError socketError, const QString& errorString);
    
    // 服务器状态信息
    void serverStatusMessage(const QString &message);

    // 传入会话请求信号 (UI决定是否接受)
    void incomingSessionRequest(QTcpSocket* tempSocket, const QString &peerAddress, quint16 peerPort, const QString &peerUuid, const QString &peerNameHint);

    // 新增：当出站连接尝试失败或被拒绝时发出
    void outgoingConnectionFailed(const QString& peerNameAttempted, const QString& reason);

public slots:
    // 接受传入的会话
    void acceptIncomingSession(QTcpSocket* tempSocket, const QString& peerUuid, const QString& localNameForPeer);
    // 拒绝传入的会话
    void rejectIncomingSession(QTcpSocket* tempSocket);
    // 新增：手动触发UDP广播
    void triggerManualUdpBroadcast();

private slots:
    // 处理服务器的新连接请求
    void onNewConnection();
    
    // 处理已建立连接的客户端socket断开
    void handleClientSocketDisconnected();
    // 处理已建立连接的客户端socket可读数据
    void handleClientSocketReadyRead();
    // 处理已建立连接的客户端socket错误
    void handleClientSocketError(QAbstractSocket::SocketError socketError);

    // 处理等待HELLO消息的传入socket可读数据
    void handlePendingIncomingSocketReadyRead();
    // 处理等待HELLO消息的传入socket断开
    void handlePendingIncomingSocketDisconnected();
    // 处理等待HELLO消息的传入socket错误
    void handlePendingIncomingSocketError(QAbstractSocket::SocketError socketError);

    // 处理出站连接尝试成功建立TCP连接
    void handleOutgoingSocketConnected();
    // 处理出站连接等待SESSION_ACCEPTED消息时socket可读数据
    void handleOutgoingSocketReadyRead();
    // 处理出站连接的socket断开
    void handleOutgoingSocketDisconnected();
    // 处理出站连接的socket错误
    void handleOutgoingSocketError(QAbstractSocket::SocketError socketError);

    void attemptToListen(); // 新增：重试监听的槽

    // 新增UDP相关槽函数
    void sendUdpBroadcast(); // 保持声明，但不再由定时器调用
    void processPendingUdpDatagrams();
    void processUdpResponseToNeed(); // 新增：处理临时UDP监听器收到的REQNEED
    void handleUdpResponseListenerTimeout(); // 新增：处理临时UDP监听器超时
    void handleTemporaryUdpSocketError(QAbstractSocket::SocketError socketError); // 新增：处理临时UDP监听器错误

private:
    QTcpServer *tcpServer;
    QUdpSocket *udpDiscoveryListenerSocket;
    QUdpSocket *udpBroadcastSenderSocket;
    QUdpSocket *udpTemporaryResponseListenerSocket;
    QTimer *udpResponseListenerTimer;
    
    // 管理已建立的连接
    QMap<QString, QTcpSocket*> connectedSockets; // Key: Peer UUID
    QMap<QTcpSocket*, QString> socketToUuidMap;  // Helper: Socket -> Peer UUID
    QMap<QString, QString> peerUuidToNameMap; // Helper: Peer UUID -> Peer Name (as known locally)

    // 管理正在建立的连接
    QList<QTcpSocket*> pendingIncomingSockets; // Sockets from onNewConnection, waiting for HELLO
    // Key: Socket, Value: Pair of (TentativePeerName, TargetPeerUUIDHint)
    QMap<QTcpSocket*, QPair<QString, QString>> outgoingSocketsAwaitingSessionAccepted; 

    quint16 defaultPort;        // 默认端口 (保留，但首选端口更重要)
    QString lastError;          // 最后发生的通用服务器错误字符串

    quint16 preferredListenPort;
    quint16 preferredOutgoingPortNumber;
    bool bindToSpecificOutgoingPort;

    // 本地用户信息
    QString localUserUuid;
    QString localUserDisplayName;

    bool autoStartListeningEnabled;
    bool udpDiscoveryEnabled;
    quint16 currentUdpDiscoveryPort; // 新增：当前UDP发现端口
    bool udpContinuousBroadcastEnabled; // 新增：是否启用连续广播
    int udpBroadcastIntervalSeconds; // 新增：广播间隔秒数
    QTimer *retryListenTimer;
    QTimer *udpBroadcastTimer;
    int retryListenIntervalMs;

    void setupServer();
    void cleanupSocket(QTcpSocket* socket, bool removeFromConnectedSockets = true);
    void sendSystemMessage(QTcpSocket* socket, const QString& sysMessage);
    // 将socket添加到connectedSockets和相关映射中
    void addEstablishedConnection(QTcpSocket* socket, const QString& peerUuid, const QString& peerName, const QString& peerAddress, quint16 peerPort);
    // 从pendingIncomingSockets中移除socket并断开其临时信号槽
    void removePendingIncomingSocket(QTcpSocket* socket);
    // 从outgoingSocketsAwaitingSessionAccepted中移除socket并断开其临时信号槽
    void removeOutgoingSocketAwaitingAcceptance(QTcpSocket* socket);

    // 新增辅助方法
    QList<QHostAddress> getLocalIpAddresses() const;
    bool isSelfConnection(const QString& targetHost, quint16 targetPort) const;
    void cleanupTemporaryUdpResponseListener(); // 新增：清理临时UDP监听器
    QString getDiscoveryMessageValue(const QStringList& parts, const QString& key) const; // 新增：解析消息字段
};

#endif // NETWORKMANAGER_H
