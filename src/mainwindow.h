#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QMap> // For chat history
#include <QStringList> // For chat history
#include <QUuid> // For UUID generation
#include <QSettings> // For storing UUID
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
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QString getLocalUserName() const;
    QString getLocalUserUuid() const; // 新增

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

    // 替换QTextBrowser为自定义组件
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

    // 用户设置
    QString localUserName;
    QString localUserUuid; // 新增：本地用户的UUID
    quint16 localListenPort;
    quint16 localOutgoingPort;         // 新增：传出连接的源端口
    bool useSpecificOutgoingPort; // 新增：是否使用特定的传出源端口

    PeerInfoWidget *peerInfoDisplayWidget; // New widget instance
    FormattingToolbarHandler *formattingHandler; // New handler instance

    void setupUI();
    void loadOrCreateUserIdentity(); // 新增方法

private slots:
    void onClearButtonClicked();
    void onAddContactButtonClicked();
    void handleContactAdded(const QString &name, const QString &uuid); // Modified to include UUID
    void onContactSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onSendButtonClicked();
    void onSettingsButtonClicked(); // 设置按钮的槽函数
    void handleSettingsApplied(const QString &userName,
                               quint16 listenPort,
                               quint16 outgoingPort, bool useSpecificOutgoingPort); // 处理设置应用的槽函数

    // NetworkManager slots
    void handleNetworkConnected();
    void handleNetworkDisconnected();
    void handleNewMessageReceived(const QString &message);
    void handleNetworkError(QAbstractSocket::SocketError socketError);
    void updateNetworkStatus(const QString &status);
    // Modified to include UUID and name hint
    void handleIncomingConnectionRequest(const QString &peerAddress, quint16 peerPort, const QString &peerUuid, const QString &peerNameHint);

    // Formatting related slots are now removed, will be handled by FormattingToolbarHandler
    // void onBoldButtonToggled(bool checked);
    // void onItalicButtonToggled(bool checked);
    // void onUnderlineButtonToggled(bool checked);
    // void onColorButtonClicked();
    // void onBgColorButtonClicked();
    // void onFontSizeChanged(const QString &text);
    // void onFontFamilyChanged(const QFont &font);
    // void onCurrentCharFormatChanged(const QTextCharFormat &format);

    // New slots to update MainWindow's color state from FormattingToolbarHandler
    void handleTextColorChanged(const QColor &color);
    void handleBackgroundColorChanged(const QColor &color);
};
#endif // MAINWINDOW_H
