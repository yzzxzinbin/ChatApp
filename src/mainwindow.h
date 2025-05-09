#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QMap> // For chat history
#include <QStringList> // For chat history
#include "chatmessagedisplay.h" // 添加自定义组件头文件
#include "networkmanager.h" // Include NetworkManager
// #include "settingsdialog.h" // Forward declare instead or include if SettingsDialog is used as value

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
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QString getLocalUserName() const;

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
    quint16 localListenPort;
    quint16 localOutgoingPort;         // 新增：传出连接的源端口
    bool useSpecificOutgoingPort; // 新增：是否使用特定的传出源端口

    void setupUI();
    void applyStyles();

private slots:
    void onClearButtonClicked();
    void onAddContactButtonClicked();
    void handleContactAdded(const QString &name);
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
    void handleIncomingConnectionRequest(const QString &peerAddress, quint16 peerPort);

    // Formatting slots
    void onBoldButtonToggled(bool checked);
    void onItalicButtonToggled(bool checked);
    void onUnderlineButtonToggled(bool checked);
    void onColorButtonClicked(); // 添加颜色按钮点击处理
    void onBgColorButtonClicked(); // 新增背景色按钮点击事件
    void onFontSizeChanged(const QString &text);
    void onFontFamilyChanged(const QFont &font);
    void onCurrentCharFormatChanged(const QTextCharFormat &format);
};
#endif // MAINWINDOW_H
