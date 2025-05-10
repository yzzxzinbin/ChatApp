#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QMap> // For chat history
#include <QStringList> // For chat history
#include <QUuid> // For UUID generation
#include <QSettings> // For storing UUID
#include <QKeyEvent> // 新增：包含 QKeyEvent
#include "chatmessagedisplay.h" // 添加自定义组件头文件
#include "networkmanager.h" // Include NetworkManager

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
class QTextEdit;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QWidget;
class QFontComboBox;
class QComboBox;
class QTextCharFormat; // For formatting
class QColor; // For color selection

class ContactManager;
class SettingsDialog; // Forward declaration
class PeerInfoWidget; // Forward declaration for our new widget
class FormattingToolbarHandler; // Forward declaration for the new handler
class NetworkEventHandler; // Forward declaration for network event handler
class ChatHistoryManager; // 新增：前向声明 ChatHistoryManager
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QString getLocalUserName() const;
    QString getLocalUserUuid() const; // 新增
    void updateNetworkStatus(const QString &status); // Make updateNetworkStatus public
    void loadOrCreateUserIdentity(); // 新增方法
    void saveChatHistory(const QString& peerUuid); // 确保此方法是 public 或 NetworkEventHandler 可以访问

protected: // 新增：或者 public，取决于您的偏好
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void handleContactAdded(const QString &name, const QString &uuid, const QString &ip, quint16 port); // 更新签名以包含IP和端口，用于持久化

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
    ChatMessageDisplay *messageDisplay;

    QWidget *formattingToolbarWidget;
    QHBoxLayout *formattingToolbarLayout;
    QPushButton *boldButton;
    QPushButton *italicButton;
    QPushButton *underlineButton;
    QPushButton *colorButton; // 添加颜色按钮
    QPushButton *bgColorButton; // 文本背景色按钮
    QComboBox *fontSizeComboBox;
    QFontComboBox *fontFamilyComboBox;
    QColor currentTextColor; // 当前文本颜色
    QColor currentBgColor; // 当前背景色

    QWidget *inputAreaWidget;
    QHBoxLayout *inputAreaLayout;
    QTextEdit *messageInputEdit;

    QWidget *buttonsWidget;
    QVBoxLayout *buttonsLayout;
    QPushButton *sendButton;
    QPushButton *clearButton;
    QPushButton *closeChatButton;

    QLabel *emptyChatPlaceholderLabel;
    QLabel *networkStatusLabel; // For displaying network status

    ContactManager *contactManager;
    NetworkManager *networkManager; // NetworkManager instance
    SettingsDialog *settingsDialog; // 设置对话框实例

    // Data members for chat history and current contact
    QMap<QString, QStringList> chatHistories;
    QString currentOpenChatContactName;
    ChatHistoryManager* chatHistoryManager; // 新增：聊天记录管理器

    // 用户设置
    QString localUserName;
    QString localUserUuid; // 新增：本地用户的UUID
    quint16 localListenPort;
    bool autoNetworkListeningEnabled; // 新增：用户是否启用了网络监听
    bool udpDiscoveryEnabled;         // 新增：用户是否启用了UDP发现
    quint16 localOutgoingPort;
    bool useSpecificOutgoingPort; // 新增：是否使用特定的传出源端口

    PeerInfoWidget *peerInfoDisplayWidget; // New widget instance
    FormattingToolbarHandler *formattingHandler; // New handler instance
    NetworkEventHandler *networkEventHandler; // New network event handler instance

    void setupUI();
    // 新增：联系人持久化和重连方法
    void saveContacts();
    void loadContactsAndAttemptReconnection();

private slots:
    void onClearButtonClicked();
    void onAddContactButtonClicked();
    void onContactSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onSendButtonClicked();
    void onSettingsButtonClicked(); // 设置按钮的槽函数
    void handleSettingsApplied(const QString &userName,
                               quint16 listenPort,
                               bool enableListening, // 新增
                               quint16 outgoingPort, bool useSpecificOutgoingPort,
                               bool enableUdpDiscovery); // 新增：处理UDP设置
    void handleRetryListenNow(); // 新增槽函数
    void handleManualUdpBroadcastRequested(); // 新增槽

    // This slot remains in MainWindow due to heavy UI interaction (QMessageBox, QInputDialog)
    // 更新签名以匹配 NetworkManager::incomingSessionRequest
    void handleIncomingConnectionRequest(QTcpSocket* tempSocket, const QString &peerAddress, quint16 peerPort, const QString &peerUuid, const QString &peerNameHint);

    // New slots to update MainWindow's color state from FormattingToolbarHandler
    void handleTextColorChanged(const QColor &color);
    void handleBackgroundColorChanged(const QColor &color);
};
#endif // MAINWINDOW_H
