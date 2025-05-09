#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    // 启动服务器监听
    bool startListening();
    
    // 设置监听首选项
    void setListenPreferences(quint16 port); // 移除 bool useDynamic
    // 新增：设置传出连接首选项
    void setOutgoingConnectionPreferences(quint16 port, bool useSpecific);
    
    // 停止服务器监听
    void stopListening();

    // 连接到指定IP和端口
    void connectToHost(const QString &peerNameToSet, const QString &hostAddress, quint16 port); // 修改签名
    
    // 断开当前连接
    void disconnectFromHost();

    // 发送消息
    void sendMessage(const QString &message);

    // 获取当前socket状态
    QAbstractSocket::SocketState getCurrentSocketState() const;
    // 获取最后发生的错误信息
    QString getLastError() const;
    // 获取当前连接对方的信息 (名称/IP, 端口)
    QPair<QString, quint16> getPeerInfo() const;

signals:
    // 连接成功信号
    void connected();
    
    // 断开连接信号
    void disconnected();
    
    // 收到新消息信号
    void newMessageReceived(const QString &message);
    
    // 网络错误信号
    void networkError(QAbstractSocket::SocketError socketError);
    
    // 服务器状态信息
    void serverStatusMessage(const QString &message);
    // 新增：传入连接请求信号
    void incomingConnectionRequest(const QString &peerAddress, quint16 peerPort);
    void tcpLinkEstablished(const QString& tentativePeerName); // 新信号：TCP链路建立，等待对方确认

public slots:
    // 新增：接受待处理的连接
    void acceptPendingConnection(const QString& peerName = QString());
    // 新增：拒绝待处理的连接
    void rejectPendingConnection();

private slots:
    // 处理新连接
    void onNewConnection();
    
    // 处理断开连接
    void onSocketDisconnected();
    
    // 读取数据
    void onSocketReadyRead();
    
    // 处理socket错误
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpServer *tcpServer;      // TCP服务器对象
    QTcpSocket *clientSocket;   // 客户端socket对象
    QTcpSocket *pendingClientSocket; // 新增：待处理的客户端socket对象
    quint16 defaultPort;        // 默认端口
    QString currentPeerName;    // 新增：当前连接对方的名称
    QString lastError;          // 新增：最后发生的错误字符串

    quint16 connectedPeerPort;

    quint16 preferredListenPort;
    quint16 preferredOutgoingPortNumber; // 新增：首选传出端口号
    bool bindToSpecificOutgoingPort;  // 新增：是否绑定到特定的传出端口

    // 新增状态和临时变量
    bool isWaitingForPeerConfirmation;
    QString pendingPeerNameToSet;
    quint16 pendingConnectedPeerPort;

    void setupServer();
};

#endif // NETWORKMANAGER_H
