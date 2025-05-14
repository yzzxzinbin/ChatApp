#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QMap>        // For chat history
#include <QStringList> // For chat history
#include <QUuid>       // For UUID generation
#include <QSettings>   // For storing UUID
#include <QKeyEvent>   // 新增：包含 QKeyEvent
#include <QTcpSocket>
#include <QTextEdit>   // 添加 QTextEdit 头文件
#include <QMap>        // 添加 QMap 头文件

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
class QTextEdit; // 前向声明
class QPushButton; // 前向声明
class QVBoxLayout;
class QHBoxLayout;
class QWidget;
class QFontComboBox;
class QComboBox;
class QTextCharFormat; // For formatting
class QColor;          // For color selection
QT_END_NAMESPACE

// 自定义类的前向声明
class ChatMessageDisplay;       // 前向声明
class NetworkManager;           // 前向声明
class ContactManager;
class SettingsDialog;           // Forward declaration
class PeerInfoWidget;           // Forward declaration for our new widget
class FormattingToolbarHandler; // Forward declaration for the new handler
class NetworkEventHandler;      // Forward declaration for network event handler
class ChatHistoryManager;       // 新增：前向声明 ChatHistoryManager
class MySqlDatabase;            // 新增：前向声明 MySqlDatabase
class FileTransferManager;      // <-- Add this

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // 修改构造函数以接收登录的用户ID
    explicit MainWindow(const QString &currentUserId, QWidget *parent = nullptr);
    ~MainWindow();
    QString getLocalUserName() const;
    QString getLocalUserUuid() const;
    quint16 getLocalListenPort() const;
    void updateNetworkStatus(const QString &status);
    void loadCurrentUserIdentity();
    void saveChatHistory(const QString &peerUuid);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void handleContactAdded(const QString &name, const QString &uuid, const QString &ip, quint16 port);
    // Add slots for FileTransferManager signals if UI needs to react directly
    void handleIncomingFileOffer(const QString& transferID, const QString& peerUuid, const QString& fileName, qint64 fileSize);
    void updateFileTransferProgress(const QString& transferID, qint64 bytesTransferred, qint64 totalSize);
    void handleFileTransferFinished(const QString& transferID, const QString& peerUuid, const QString& fileName, bool success, const QString& message);

private slots: // 将这些声明为 private slots
    void onAddContactButtonClicked();
    void onSettingsButtonClicked();
    void onContactSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onSendButtonClicked();
    void onClearButtonClicked(); // 确保这个函数有定义，或者移除连接它的代码
    void handleTextColorChanged(const QColor &color);
    void handleBackgroundColorChanged(const QColor &color);
    void handleIncomingConnectionRequest(QTcpSocket *tempSocket, const QString &peerAddress, quint16 peerPort, const QString &peerUuid, const QString &peerNameHint);
    void handleSettingsApplied(const QString &userName,
                               quint16 listenPort,
                               bool enableListening,
                               quint16 outgoingPort, bool useSpecificOutgoingPortVal,
                               bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                               bool enableContinuousUdpBroadcast, int udpBroadcastInterval);
    void handleRetryListenNowRequested(); // 新增槽
    void handleManualUdpBroadcastRequested(); // 新增槽

    // 新增：编辑区清除按钮的槽函数声明
    void onMessageInputTextChanged();
    void onClearMessageInputClicked();
    void onSendFileButtonClicked(); // <-- Add slot for send file button

private:
    // Declare widgets and layouts
    QWidget *centralWidget;
    QHBoxLayout *mainLayout;

    QWidget *leftSidebar;
    QVBoxLayout *leftSidebarLayout;
    QPushButton *addContactButton;
    QPushButton *settingsButton; // 新增设置按钮

    QListWidget *contactListWidget;

    QWidget *chatAreaWidget;
    QVBoxLayout *chatAreaLayout;
    QStackedWidget *chatStackedWidget;

    QWidget *activeChatContentsWidget;
    QVBoxLayout *activeChatContentsLayout;

    // 使用自定义组件显示消息历史
    ChatMessageDisplay *messageDisplay; // 指针，使用前向声明即可

    QWidget *formattingToolbarWidget;
    QHBoxLayout *formattingToolbarLayout;
    QPushButton *boldButton;
    QPushButton *italicButton;
    QPushButton *underlineButton;
    QPushButton *colorButton;   // 添加颜色按钮
    QPushButton *bgColorButton; // 文本背景色按钮
    QComboBox *fontSizeComboBox;
    QFontComboBox *fontFamilyComboBox;
    QColor currentTextColor; // 当前文本颜色
    QColor currentBgColor;   // 当前背景色

    QWidget *inputAreaWidget;
    QHBoxLayout *inputAreaLayout;
    QTextEdit *messageInputEdit; // 指针，使用前向声明即可

    QWidget *buttonsWidget;
    QVBoxLayout *buttonsLayout;
    QPushButton *sendButton; // 指针，使用前向声明即可
    QPushButton *clearButton; // 指针，使用前向声明即可
    QPushButton *closeChatButton; // 指针，使用前向声明即可
    QPushButton *clearMessageButton; // 新增：编辑区清除按钮的指针声明
    QPushButton *sendFileButton; // <-- Add send file button member

    QLabel *emptyChatPlaceholderLabel;
    QLabel *networkStatusLabel; // For displaying network status

    ContactManager *contactManager;
    NetworkManager *networkManager; // 指针，使用前向声明即可
    SettingsDialog *settingsDialog; // 设置对话框实例

    // Data members for chat history and current contact
    QMap<QString, QStringList> chatHistories;
    QString currentOpenChatContactName;
    ChatHistoryManager *chatHistoryManager; // 新增：聊天记录管理器

    // 用户设置
    QString localUserName;
    QString localUserUuid; // 新增：本地用户的UUID
    quint16 localListenPort;
    bool autoNetworkListeningEnabled;   // 新增：用户是否启用了网络监听
    bool udpDiscoveryEnabled;           // 新增：用户是否启用了UDP发现
    quint16 localUdpDiscoveryPort;      // Added
    bool udpContinuousBroadcastEnabled; // Added
    int udpBroadcastIntervalSeconds;    // Added
    quint16 localOutgoingPort;
    bool useSpecificOutgoingPort; // 新增：是否使用特定的传出源端口

    PeerInfoWidget *peerInfoDisplayWidget;       // New widget instance
    FormattingToolbarHandler *formattingHandler; // New handler instance
    NetworkEventHandler *networkEventHandler;    // New network event handler instance
    FileTransferManager *fileTransferManager;    // <-- Add FileTransferManager member

    QString m_currentUserIdStr; // 新增：存储当前登录的用户ID

    void setupUI();
    // 新增：联系人持久化和重连方法
    void saveContacts();
    void loadContactsAndAttemptReconnection();
    void loadCurrentUserContacts(); // 新增：加载当前用户的联系人
    void saveCurrentUserContacts(); // 新增：保存当前用户的联系人
};
#endif // MAINWINDOW_H
