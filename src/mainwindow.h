#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QMap> // For chat history
#include <QStringList> // For chat history
#include "chatmessagedisplay.h" // 添加自定义组件头文件

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
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // Declare widgets and layouts
    QWidget *centralWidget;
    QHBoxLayout *mainLayout;

    QWidget *leftSidebar;
    QVBoxLayout *leftSidebarLayout;
    QPushButton *addContactButton;

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
    QComboBox *fontSizeComboBox;
    QFontComboBox *fontFamilyComboBox;
    QColor currentTextColor; // 当前文本颜色

    QWidget *inputAreaWidget;
    QHBoxLayout *inputAreaLayout;
    QTextEdit *messageInputEdit;

    QWidget *buttonsWidget;
    QVBoxLayout *buttonsLayout;
    QPushButton *sendButton;
    QPushButton *clearButton;
    QPushButton *closeChatButton;

    QLabel *emptyChatPlaceholderLabel;

    ContactManager *contactManager;

    // Data members for chat history and current contact
    QMap<QString, QStringList> chatHistories;
    QString currentOpenChatContactName;

    void setupUI();
    void applyStyles();

private slots:
    void onClearButtonClicked();
    void onAddContactButtonClicked();
    void handleContactAdded(const QString &name);
    void onContactSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onSendButtonClicked();

    // Formatting slots
    void onBoldButtonToggled(bool checked);
    void onItalicButtonToggled(bool checked);
    void onUnderlineButtonToggled(bool checked);
    void onColorButtonClicked(); // 添加颜色按钮点击处理
    void onFontSizeChanged(const QString &text);
    void onFontFamilyChanged(const QFont &font);
    void onCurrentCharFormatChanged(const QTextCharFormat &format);
};
#endif // MAINWINDOW_H
