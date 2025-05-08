#include "mainwindow.h"
#include "contactmanager.h"
#include "chatmessagedisplay.h"
#include "networkmanager.h" // 确保包含了 NetworkManager

#include <QApplication>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QFontComboBox>
#include <QComboBox>
#include <QLabel>
#include <QIcon>
#include <QTextCharFormat>
#include <QTextDocumentFragment>
#include <QScrollBar>
#include <QColorDialog>
#include <QStatusBar> // 确保包含了 QStatusBar
#include <QMessageBox> // 确保包含了 QMessageBox
#include <QInputDialog> // For naming incoming connections

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    // 1. 首先初始化 NetworkManager
    networkManager = new NetworkManager(this);

    // 2. 然后初始化 ContactManager，并将 NetworkManager 实例传递给它
    contactManager = new ContactManager(networkManager, this);
    connect(contactManager, &ContactManager::contactAdded, this, &MainWindow::handleContactAdded);

    // 初始化默认文本颜色和背景色
    currentTextColor = QColor(Qt::black);
    currentBgColor = QColor(Qt::transparent); // 默认背景色为透明

    setupUI();
    applyStyles();
    setWindowTitle("Chat Application");
    resize(1024, 768);

    // 连接 NetworkManager 的信号到 MainWindow 的槽
    connect(networkManager, &NetworkManager::connected, this, &MainWindow::handleNetworkConnected);
    connect(networkManager, &NetworkManager::disconnected, this, &MainWindow::handleNetworkDisconnected);
    connect(networkManager, &NetworkManager::newMessageReceived, this, &MainWindow::handleNewMessageReceived);
    connect(networkManager, &NetworkManager::networkError, this, &MainWindow::handleNetworkError);
    connect(networkManager, &NetworkManager::serverStatusMessage, this, &MainWindow::updateNetworkStatus);
    connect(networkManager, &NetworkManager::incomingConnectionRequest, this, &MainWindow::handleIncomingConnectionRequest);

    // 应用启动后默认开启端口监听
    networkManager->startListening();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);                  // spacing（间距）是布局中各个控件之间的空隙
    mainLayout->setContentsMargins(0, 0, 0, 0); // margins（边距）是布局整体与父窗口边界之间的距离

    // 左侧固定宽度的侧边栏
    leftSidebar = new QWidget(this);
    leftSidebar->setObjectName("leftSidebar");
    leftSidebar->setFixedWidth(50);
    leftSidebarLayout = new QVBoxLayout(leftSidebar);
    leftSidebarLayout->setContentsMargins(5, 5, 5, 5);
    leftSidebarLayout->setSpacing(10);
    // 添加联系人按钮
    addContactButton = new QPushButton(this);
    addContactButton->setObjectName("addContactButton");
    addContactButton->setIcon(QIcon(":/icons/add_user.svg"));
    addContactButton->setIconSize(QSize(24, 24));
    addContactButton->setToolTip("Add new contact");
    connect(addContactButton, &QPushButton::clicked, this, &MainWindow::onAddContactButtonClicked);
    leftSidebarLayout->addWidget(addContactButton);
    leftSidebarLayout->addStretch();    // 添加垂直填充
    mainLayout->addWidget(leftSidebar); // 将左侧侧边栏添加到主布局

    // 联系人列表
    contactListWidget = new QListWidget(this);
    contactListWidget->setObjectName("contactListWidget");
    contactListWidget->setMaximumWidth(250);
    contactListWidget->setMinimumWidth(200);
    connect(contactListWidget, &QListWidget::currentItemChanged, this, &MainWindow::onContactSelected);
    mainLayout->addWidget(contactListWidget, 0);

    // 聊天区域
    chatAreaWidget = new QWidget(this);
    chatAreaWidget->setObjectName("chatAreaWidget");
    chatAreaLayout = new QVBoxLayout(chatAreaWidget);
    chatAreaLayout->setSpacing(0);
    chatAreaLayout->setContentsMargins(0, 0, 0, 0);

    // QStackedWidget 是一个可以在多个子窗口之间切换显示的控件（类似于“页面切换”），
    // 只会显示其中一个子窗口，其余的隐藏。常用于实现多页面界面或内容切换。
    chatStackedWidget = new QStackedWidget(this);
    activeChatContentsWidget = new QWidget(this);
    activeChatContentsLayout = new QVBoxLayout(activeChatContentsWidget);
    activeChatContentsLayout->setSpacing(6);
    activeChatContentsLayout->setContentsMargins(10, 10, 10, 10);

    // 聊天记录显示区域
    messageDisplay = new ChatMessageDisplay(this);
    messageDisplay->setObjectName("messageDisplay");
    messageDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 创建格式化工具栏
    formattingToolbarWidget = new QWidget(this);
    formattingToolbarWidget->setObjectName("formattingToolbarWidget");
    formattingToolbarLayout = new QHBoxLayout(formattingToolbarWidget);
    formattingToolbarLayout->setContentsMargins(0, 0, 0, 0);
    formattingToolbarLayout->setSpacing(5);

    // 横向布局的工具栏组件:加粗、斜体、下划线、颜色选择器、字体大小选择器、字体选择器
    boldButton = new QPushButton("B", this);
    boldButton->setCheckable(true);
    connect(boldButton, &QPushButton::toggled, this, &MainWindow::onBoldButtonToggled);
    italicButton = new QPushButton("I", this);
    italicButton->setCheckable(true);
    connect(italicButton, &QPushButton::toggled, this, &MainWindow::onItalicButtonToggled);
    underlineButton = new QPushButton("U", this);
    underlineButton->setCheckable(true);
    connect(underlineButton, &QPushButton::toggled, this, &MainWindow::onUnderlineButtonToggled);
    colorButton = new QPushButton(this); // 创建前景颜色按钮
    colorButton->setObjectName("colorButton");
    colorButton->setToolTip("Text Color");
    QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentTextColor.name());
    colorButton->setStyleSheet(colorStyle);
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::onColorButtonClicked);
    bgColorButton = new QPushButton(this); // 创建背景色按钮
    bgColorButton->setObjectName("bgColorButton");
    bgColorButton->setToolTip("Background Color");
    QString bgColorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentBgColor.name());
    bgColorButton->setStyleSheet(bgColorStyle);
    connect(bgColorButton, &QPushButton::clicked, this, &MainWindow::onBgColorButtonClicked);
    fontSizeComboBox = new QComboBox(this); // 字体大小选择器
    for (int i = 8; i <= 28; i += 2)
    {
        fontSizeComboBox->addItem(QString::number(i));
    }
    connect(fontSizeComboBox, &QComboBox::currentTextChanged, this, &MainWindow::onFontSizeChanged);
    fontFamilyComboBox = new QFontComboBox(this); // 字体选择器
    connect(fontFamilyComboBox, &QFontComboBox::currentFontChanged, this, &MainWindow::onFontFamilyChanged);
    // 添加格式化工具栏组件到布局formattingToolbarLayout中
    formattingToolbarLayout->addWidget(boldButton);
    formattingToolbarLayout->addWidget(italicButton);
    formattingToolbarLayout->addWidget(underlineButton);
    formattingToolbarLayout->addWidget(colorButton);
    formattingToolbarLayout->addWidget(bgColorButton);
    formattingToolbarLayout->addWidget(fontSizeComboBox);
    formattingToolbarLayout->addWidget(fontFamilyComboBox, 1);

    // 创建输入区域
    inputAreaWidget = new QWidget(this);
    inputAreaWidget->setObjectName("inputAreaWidget");
    inputAreaLayout = new QHBoxLayout(inputAreaWidget);
    inputAreaLayout->setContentsMargins(0, 0, 0, 0);
    inputAreaLayout->setSpacing(5);
    messageInputEdit = new QTextEdit(this); // 创建文本输入框
    messageInputEdit->setObjectName("messageInputEdit");
    messageInputEdit->setPlaceholderText("Type your message...");
    messageInputEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 自动伸展
    messageInputEdit->setMaximumHeight(120);
    connect(messageInputEdit, &QTextEdit::currentCharFormatChanged, this, &MainWindow::onCurrentCharFormatChanged);
    inputAreaLayout->addWidget(messageInputEdit, 1);

    // 创建辅助按钮区域
    buttonsWidget = new QWidget(this);
    buttonsLayout = new QVBoxLayout(buttonsWidget);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(5);
    buttonsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    buttonsWidget->setFixedWidth(100); // 确保按钮区域宽度固定
    // 辅助按钮设定
    sendButton = new QPushButton("Send", this);
    sendButton->setObjectName("sendButton");
    sendButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    clearButton = new QPushButton("Clear", this);
    clearButton->setObjectName("clearButton");
    clearButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearButtonClicked);
    closeChatButton = new QPushButton("Close", this);
    closeChatButton->setObjectName("closeChatButton");
    closeChatButton->setToolTip("Close window");
    closeChatButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    connect(closeChatButton, &QPushButton::clicked, this, &QMainWindow::close);
    // 将按钮添加到按钮布局buttonsLayout中
    buttonsLayout->addWidget(sendButton, 1);
    buttonsLayout->addWidget(clearButton, 1);
    buttonsLayout->addWidget(closeChatButton, 1);

    inputAreaLayout->addWidget(buttonsWidget); // 将按钮区域添加到输入区域布局inputAreaLayout中
    // 激活聊天内容布局中加入聊天记录显示区域、格式化工具栏区域和输入区域
    activeChatContentsLayout->addWidget(messageDisplay, 1);
    activeChatContentsLayout->addWidget(formattingToolbarWidget, 0);
    activeChatContentsLayout->addWidget(inputAreaWidget, 0);
    // 聊天Stack的默认文本
    emptyChatPlaceholderLabel = new QLabel("Select a contact to start chatting or add a new one.", this);
    emptyChatPlaceholderLabel->setObjectName("emptyChatPlaceholderLabel");
    emptyChatPlaceholderLabel->setAlignment(Qt::AlignCenter);

    chatStackedWidget->addWidget(emptyChatPlaceholderLabel);
    chatStackedWidget->addWidget(activeChatContentsWidget);

    chatAreaLayout->addWidget(chatStackedWidget);
    mainLayout->addWidget(chatAreaWidget, 1); // 设置聊天区域为主要伸缩区域
    centralWidget->setLayout(mainLayout);

    chatStackedWidget->setCurrentWidget(emptyChatPlaceholderLabel); // 初始化时不选中联系人对话

    // 添加状态栏用于显示网络消息
    networkStatusLabel = new QLabel("Network Status: Idle", this);
    statusBar()->addWidget(networkStatusLabel);

    QTextCharFormat initialFormat; // 初始化文本属性
    if (fontFamilyComboBox->currentFont().pointSize() > 0)
    { // 初始化字体类型
        initialFormat.setFont(fontFamilyComboBox->currentFont());
    }
    else
    {
        initialFormat.setFontFamilies({QApplication::font().family()});
    }
    bool sizeFound = false;
    for (int i = 0; i < fontSizeComboBox->count(); ++i)
    { // 查找并初始化文本字体大小和索引
        if (fontSizeComboBox->itemText(i) == "12")
        {
            fontSizeComboBox->setCurrentIndex(i);
            initialFormat.setFontPointSize(12);
            sizeFound = true;
            break;
        }
    }
    if (!sizeFound && fontSizeComboBox->count() > 0)
    {
        fontSizeComboBox->setCurrentIndex(0);
        initialFormat.setFontPointSize(fontSizeComboBox->currentText().toInt());
    }
    else if (!sizeFound)
    {
        initialFormat.setFontPointSize(12);
    }
    initialFormat.setForeground(currentTextColor);
    initialFormat.setBackground(currentBgColor); // 设置初始背景色
    messageInputEdit->setCurrentCharFormat(initialFormat);
    onCurrentCharFormatChanged(messageInputEdit->currentCharFormat());
}

void MainWindow::onClearButtonClicked()
{
    messageInputEdit->clear();
    QTextCharFormat defaultFormat;
    if (fontFamilyComboBox->currentFont().pointSize() > 0)
    {
        defaultFormat.setFont(fontFamilyComboBox->currentFont());
    }
    else
    {
        defaultFormat.setFontFamilies({QApplication::font().family()});
    }
    defaultFormat.setFontPointSize(fontSizeComboBox->currentText().toInt());
    defaultFormat.setForeground(currentTextColor);
    defaultFormat.setBackground(currentBgColor); // 保留当前背景色
    messageInputEdit->setCurrentCharFormat(defaultFormat);
}

void MainWindow::onAddContactButtonClicked()
{
    contactManager->showAddContactDialog(this);
}

void MainWindow::handleContactAdded(const QString &name)
{
    if (chatHistories.find(name) == chatHistories.end())
    {
        chatHistories[name] = QStringList();
    }
    QListWidgetItem *newItem = new QListWidgetItem(name, contactListWidget);
    contactListWidget->setCurrentItem(newItem);
}

void MainWindow::onContactSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current)
    {
        currentOpenChatContactName = current->text();

        QStringList messagesToDisplay;
        if (chatHistories.contains(currentOpenChatContactName))
        {
            messagesToDisplay = chatHistories.value(currentOpenChatContactName);
        }
        else
        {
            chatHistories[currentOpenChatContactName] = QStringList();
        }

        messageDisplay->setMessages(messagesToDisplay);

        messageInputEdit->clear();
        messageInputEdit->setFocus();
        if (chatStackedWidget->currentWidget() != activeChatContentsWidget)
        {
            chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
        }
    }
    else
    {
        currentOpenChatContactName.clear();
        messageDisplay->clear();
        messageInputEdit->clear();
        if (chatStackedWidget->currentWidget() != emptyChatPlaceholderLabel)
        {
            chatStackedWidget->setCurrentWidget(emptyChatPlaceholderLabel);
        }
    }
}

void MainWindow::onSendButtonClicked()
{
    if (currentOpenChatContactName.isEmpty()) {
        updateNetworkStatus(tr("No active chat to send message."));
        return;
    }
    // 检查 NetworkManager 是否已初始化并且处于连接状态
    if (!networkManager || networkManager->getCurrentSocketState() != QAbstractSocket::ConnectedState) {
         updateNetworkStatus(tr("Not connected. Cannot send message."));
         QMessageBox::warning(this, tr("Network Error"), tr("Not connected to any peer. Please connect first."));
         return;
    }

    QString plainMessageText = messageInputEdit->toPlainText().trimmed();
    if (!plainMessageText.isEmpty())
    {
        QString messageContentHtml = messageInputEdit->toHtml();

        QTextDocument doc;
        doc.setHtml(messageContentHtml);
        QString innerBodyHtml = doc.toHtml();

        QString coreContent = innerBodyHtml;
        if (coreContent.startsWith("<p", Qt::CaseInsensitive) && coreContent.count("<p", Qt::CaseInsensitive) == 1 && coreContent.endsWith("</p>", Qt::CaseInsensitive))
        {
            int pTagEnd = coreContent.indexOf('>');
            int pEndTagStart = coreContent.lastIndexOf("</p>", -1, Qt::CaseInsensitive);
            if (pTagEnd != -1 && pEndTagStart > pTagEnd)
            {
                coreContent = coreContent.mid(pTagEnd + 1, pEndTagStart - (pTagEnd + 1));
            }
        }
        if (coreContent.trimmed().isEmpty() && !plainMessageText.isEmpty())
        {
            QTextDocumentFragment fragment = QTextDocumentFragment::fromHtml(messageInputEdit->toHtml());
            coreContent = fragment.toHtml();
        }

        QString userMessageHtml = QString(
                                      "<div style=\"text-align: right; margin-bottom: 2px;\">"
                                      "<p style=\"margin:0; padding:0; text-align: right;\">"
                                      "<span style=\"font-weight: bold; background-color: #a7dcb2; padding: 2px 6px; margin-left: 4px; border-radius: 3px;\">Me:</span> %1"
                                      "</p>"
                                      "</div>")
                                      .arg(coreContent);

        chatHistories[currentOpenChatContactName].append(userMessageHtml);

        messageDisplay->addMessage(userMessageHtml);
        
        // 通过 NetworkManager 发送消息
        networkManager->sendMessage(coreContent); // 发送实际内容，而不是包含 "Me:" 的完整 HTML

        messageInputEdit->clear();
        QTextCharFormat defaultFormat;
        if (fontFamilyComboBox->currentFont().pointSize() > 0)
        {
            defaultFormat.setFont(fontFamilyComboBox->currentFont());
        }
        else
        {
            defaultFormat.setFontFamilies({QApplication::font().family()});
        }
        defaultFormat.setFontPointSize(fontSizeComboBox->currentText().toInt());
        defaultFormat.setForeground(currentTextColor);
        defaultFormat.setBackground(currentBgColor); // 保留当前背景色
        messageInputEdit->setCurrentCharFormat(defaultFormat);
        onCurrentCharFormatChanged(defaultFormat);

        messageInputEdit->setFocus();
    }
}

void MainWindow::onBoldButtonToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void MainWindow::onItalicButtonToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setFontItalic(checked);
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void MainWindow::onUnderlineButtonToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(checked);
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void MainWindow::onFontSizeChanged(const QString &text)
{
    bool ok;
    double pointSize = text.toDouble(&ok);
    if (ok && pointSize > 0)
    {
        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        messageInputEdit->mergeCurrentCharFormat(fmt);
        messageInputEdit->setFocus();
    }
}

void MainWindow::onFontFamilyChanged(const QFont &font)
{
    QTextCharFormat fmt;
    fmt.setFontFamilies(font.families());
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

// 实现颜色选择按钮点击事件
void MainWindow::onColorButtonClicked()
{
    QColor color = QColorDialog::getColor(currentTextColor, this, "Select Text Color");
    if (color.isValid())
    {
        currentTextColor = color;

        // 更新颜色按钮样式
        QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(color.name());
        colorButton->setStyleSheet(colorStyle);

        // 应用文本颜色
        QTextCharFormat fmt;
        fmt.setForeground(color);
        messageInputEdit->mergeCurrentCharFormat(fmt);
        messageInputEdit->setFocus();
    }
}

// 实现背景色选择按钮点击事件
void MainWindow::onBgColorButtonClicked()
{
    QColor color = QColorDialog::getColor(currentBgColor.isValid() ? currentBgColor : Qt::white,
                                          this, "Select Background Color",
                                          QColorDialog::ShowAlphaChannel);
    if (color.isValid())
    {
        currentBgColor = color;

        // 更新背景色按钮样式
        QString bgColorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(color.name());
        bgColorButton->setStyleSheet(bgColorStyle);

        // 应用文本背景色
        QTextCharFormat fmt;
        fmt.setBackground(color);
        messageInputEdit->mergeCurrentCharFormat(fmt);
        messageInputEdit->setFocus();
    }
}

void MainWindow::onCurrentCharFormatChanged(const QTextCharFormat &format)
{
    boldButton->blockSignals(true);
    boldButton->setChecked(format.fontWeight() >= QFont::Bold);
    boldButton->blockSignals(false);

    italicButton->blockSignals(true);
    italicButton->setChecked(format.fontItalic());
    italicButton->blockSignals(false);

    underlineButton->blockSignals(true);
    underlineButton->setChecked(format.fontUnderline());
    underlineButton->blockSignals(false);

    fontSizeComboBox->blockSignals(true);
    int pointSize = static_cast<int>(format.fontPointSize() + 0.5);
    if (pointSize <= 0 && format.font().pointSize() > 0)
        pointSize = format.font().pointSize();
    if (pointSize <= 0)
        pointSize = QApplication::font().pointSize();
    if (pointSize <= 0)
        pointSize = 12;

    QString sizeStr = QString::number(pointSize);
    int index = fontSizeComboBox->findText(sizeStr);
    if (index != -1)
    {
        fontSizeComboBox->setCurrentIndex(index);
    }
    else
    {
        fontSizeComboBox->setCurrentText(sizeStr);
    }
    fontSizeComboBox->blockSignals(false);

    fontFamilyComboBox->blockSignals(true);
    QFont fontFromFormat = format.font();
    QStringList currentFamilies = fontFromFormat.families();

    if (!currentFamilies.isEmpty())
    {
        fontFamilyComboBox->setCurrentFont(fontFromFormat);
    }
    else
    {
        fontFamilyComboBox->setCurrentFont(fontFromFormat);
    }
    fontFamilyComboBox->blockSignals(false);

    // 更新当前颜色
    if (format.hasProperty(QTextFormat::ForegroundBrush))
    {
        currentTextColor = format.foreground().color();
        QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentTextColor.name());
        colorButton->setStyleSheet(colorStyle);
    }

    // 更新当前背景色
    if (format.hasProperty(QTextFormat::BackgroundBrush))
    {
        currentBgColor = format.background().color();
        QString bgColorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentBgColor.name());
        bgColorButton->setStyleSheet(bgColorStyle);
    }
}

void MainWindow::applyStyles()
{
    QString styleSheet = R"(
        QMainWindow {
            background-color: #f0f0f2;
        }

        #leftSidebar {
            background-color: #2c3e50;
            border-right: 1px solid #34495e;
        }

        #leftSidebar QPushButton {
            background-color: transparent;
            border: none;
            padding: 5px;
            border-radius: 4px;
        }
        #leftSidebar QPushButton:hover {
            background-color: #34495e;
        }
        #leftSidebar QPushButton:pressed {
            background-color: #1a252f;
        }

        #contactListWidget {
            background-color: #ffffff;
            border: 1px solid #dcdcdc;
            border-radius-top-right: 8px;
            border-radius-bottom-right: 8px;
            padding: 8px;
            font-size: 14px;
            margin-left: 0px;
            margin-top: 0px;
            margin-bottom: 0px;
            margin-right: 5px;
        }
        #contactListWidget::item {
            padding: 10px;
            border-radius: 4px;
        }
        #contactListWidget::item:selected {
            background-color: #0078d4;
            color: white;
        }
        #contactListWidget::item:selected:hover {
            background-color: #0078d4;
            color: white;
        }
        #contactListWidget::item:hover {
            background-color: #e6f2ff;
        }

        #chatAreaWidget {
            background-color: #fdfdfd;
            border-radius: 8px;
        }

        #emptyChatPlaceholderLabel {
            font-size: 16px;
            color: #7f8c8d;
        }

        #messageDisplay {
            background-color: #ffffff;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 10px;
            font-size: 14px;
            color: #333333;
        }

        #formattingToolbarWidget {
            margin: 0px;
            padding: 0px;
        }

        #formattingToolbarWidget QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: 1px solid #cccccc;
            padding: 0px;
            font-size: 13px;
            border-radius: 4px;
            min-height: 30px;
            max-height: 30px;
            min-width: 30px;
            max-width: 30px;
            font-weight: bold;
        }
        
        #colorButton, #bgColorButton {
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            border-radius: 4px;
        }
        
        #bgColorButton {
            background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAGElEQVQYlWNgYGD4z0AEMBGkwIGJiQEKABUQAQCQ1ARCQV5unQAAAABJRU5ErkJggg==);
            background-repeat: repeat;
        }

        #formattingToolbarWidget QPushButton:checked {
            background-color: #cce5ff;
            border: 1px solid #0078d4;
        }
        #formattingToolbarWidget QPushButton:hover {
            background-color: #e0e0e0;
        }

        #formattingToolbarWidget QComboBox, 
        #formattingToolbarWidget QFontComboBox {
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 1px 5px 1px 5px;
            min-height: 28px;
            max-height: 28px;
            font-size: 13px;
            background-color: #f0f0f0;
            color: #333;
        }

        #formattingToolbarWidget QComboBox::drop-down,
        #formattingToolbarWidget QFontComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left-width: 1px;
            border-left-color: #cccccc;
            border-left-style: solid;
            border-top-right-radius: 3px;
            border-bottom-right-radius: 3px;
            background-color: #e0e0e0;
        }
        #formattingToolbarWidget QComboBox::down-arrow,
        #formattingToolbarWidget QFontComboBox::down-arrow {
            image: url(:/icons/dropdown_arrow.svg);
            width: 12px;
            height: 12px;
        }

        #formattingToolbarWidget QComboBox QAbstractItemView,
        #formattingToolbarWidget QFontComboBox QAbstractItemView {
            border: 1px solid #cccccc;
            background-color: white;
            selection-background-color: #0078d4;
            selection-color: white;
            padding: 2px;
        }

        #messageInputEdit {
            background-color: #ffffff;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 8px;
            font-size: 14px;
            color: #333333;
        }

        #sendButton, #clearButton, #closeChatButton {
            background-color: #0078d4;
            color: white;
            border: none;
            padding: 4px 12px;
            font-size: 14px;
            border-radius: 5px;
        }
        #sendButton:hover, #clearButton:hover, #closeChatButton:hover {
            background-color: #005a9e;
        }
        #sendButton:pressed, #clearButton:pressed, #closeChatButton:pressed {
            background-color: #004578;
        }
        #clearButton {
            background-color: #e74c3c;
        }
        #clearButton:hover {
            background-color: #c0392b;
        }
        #clearButton:pressed {
            background-color: #a93226;
        }
        #closeChatButton {
            background-color: #6c757d;
        }
        #closeChatButton:hover {
            background-color: #5a6268;
        }
        #closeChatButton:pressed {
            background-color: #545b62;
        }
    )";
    this->setStyleSheet(styleSheet);

    QFont formattingButtonFont;
    formattingButtonFont.setPixelSize(14);

    QFont boldIconFont = formattingButtonFont;
    boldIconFont.setBold(true);
    boldButton->setFont(boldIconFont);

    QFont italicIconFont = formattingButtonFont;
    italicIconFont.setItalic(true);
    italicButton->setFont(italicIconFont);

    QFont underlineIconFont = formattingToolbarWidget->font();
    underlineIconFont.setUnderline(true);
    underlineIconFont.setPixelSize(14);
    underlineButton->setFont(underlineIconFont);

    boldButton->setFixedSize(30, 30);
    italicButton->setFixedSize(30, 30);
    underlineButton->setFixedSize(30, 30);
    colorButton->setFixedSize(30, 30);
    bgColorButton->setFixedSize(30, 30);
}

void MainWindow::handleNetworkConnected()
{
    updateNetworkStatus(tr("Connected."));
    if (contactListWidget->currentItem() == nullptr && networkManager && networkManager->getPeerInfo().first.size() > 0) {
        QString peerName = networkManager->getPeerInfo().first;
        bool found = false;
        for(int i=0; i<contactListWidget->count(); ++i) {
            if(contactListWidget->item(i)->text() == peerName) {
                contactListWidget->setCurrentRow(i);
                found = true;
                break;
            }
        }
        if(!found) {
            handleContactAdded(peerName);
        }
    }
}

void MainWindow::handleNetworkDisconnected()
{
    updateNetworkStatus(tr("Disconnected."));
}

void MainWindow::handleNewMessageReceived(const QString &message)
{
    if (currentOpenChatContactName.isEmpty() && networkManager && networkManager->getPeerInfo().first.size() > 0) {
        QString peerName = networkManager->getPeerInfo().first;
        bool found = false;
        for(int i=0; i<contactListWidget->count(); ++i) {
            if(contactListWidget->item(i)->text() == peerName) {
                contactListWidget->setCurrentRow(i);
                found = true;
                break;
            }
        }
        if(!found) {
             handleContactAdded(peerName);
        }
    }

    if (!currentOpenChatContactName.isEmpty()) {
        QString receivedMessageHtml = QString(
                                         "<div style=\"text-align: left; margin-bottom: 2px;\">"
                                         "<p style=\"margin:0; padding:0; text-align: left;\">"
                                         "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span> %2"
                                         "</p>"
                                         "</div>")
                                         .arg(currentOpenChatContactName.toHtmlEscaped())
                                         .arg(message);

        chatHistories[currentOpenChatContactName].append(receivedMessageHtml);
        messageDisplay->addMessage(receivedMessageHtml);
    } else {
        updateNetworkStatus(tr("Received message but no active chat: %1").arg(message));
    }
}

void MainWindow::handleNetworkError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (networkManager) {
      updateNetworkStatus(tr("Network Error: %1").arg(networkManager->getLastError()));
    }
}

void MainWindow::updateNetworkStatus(const QString &status)
{
    if (networkStatusLabel) {
        networkStatusLabel->setText(status);
    } else {
        statusBar()->showMessage(status, 5000);
    }
}

void MainWindow::handleIncomingConnectionRequest(const QString &peerAddress, quint16 peerPort)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Incoming Connection"),
                                  tr("Accept connection from %1:%2?").arg(peerAddress).arg(peerPort),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        bool ok;
        QString contactName = QInputDialog::getText(this, tr("Name Contact"),
                                                  tr("Enter a name for this contact:"), QLineEdit::Normal,
                                                  peerAddress, &ok);
        if (ok && !contactName.isEmpty()) {
            networkManager->acceptPendingConnection(contactName);
        } else if (ok && contactName.isEmpty()) {
            networkManager->acceptPendingConnection(peerAddress);
        } else {
            networkManager->rejectPendingConnection();
            updateNetworkStatus(tr("Incoming connection naming cancelled. Rejected."));
        }
    } else {
        networkManager->rejectPendingConnection();
    }
}
