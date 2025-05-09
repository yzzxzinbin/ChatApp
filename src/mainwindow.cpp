#include "mainwindow.h"
#include "contactmanager.h"
#include "chatmessagedisplay.h"
#include "networkmanager.h" // 确保包含了 NetworkManager
#include "settingsdialog.h" // 确保包含了 SettingsDialog
#include "peerinfowidget.h" // Include the new PeerInfoWidget header
#include "styleutils.h"     // Include the new StyleUtils header
#include "formattingtoolbarhandler.h" // Include the new FormattingToolbarHandler header

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
#include <QStatusBar>   // 确保包含了 QStatusBar
#include <QMessageBox>  // 确保包含了 QMessageBox
#include <QInputDialog> // For naming incoming connections
#include <QUuid>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      settingsDialog(nullptr),       // 初始化 settingsDialog 为 nullptr
      localUserName(tr("Me")),       // 默认用户名
      localUserUuid(QString()),      // 初始化UUID为空
      localListenPort(60248),        // 默认监听端口 60248
      localOutgoingPort(0),          // 默认传出端口为0 (动态)
      useSpecificOutgoingPort(false) // 默认不指定传出端口
{
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QCoreApplication::setOrganizationName("YourCompany"); // 推荐设置
    QCoreApplication::setApplicationName("ChatApp");      // 推荐设置

    loadOrCreateUserIdentity(); // 加载或创建用户UUID和名称

    // 1. 首先初始化 NetworkManager
    networkManager = new NetworkManager(this);
    networkManager->setLocalUserDetails(localUserUuid, localUserName); // 设置本地用户信息
    // 在 NetworkManager 启动监听前设置初始首选项
    networkManager->setListenPreferences(localListenPort);
    // 设置初始传出连接首选项
    networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);

    // 2. 然后初始化 ContactManager，并将 NetworkManager 实例传递给它
    contactManager = new ContactManager(networkManager, this);
    connect(contactManager, &ContactManager::contactAdded, this, &MainWindow::handleContactAdded);

    // 初始化默认文本颜色和背景色
    currentTextColor = QColor(Qt::black);
    currentBgColor = QColor(Qt::transparent); // 默认背景色为透明

    setupUI();
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
    // settingsDialog 如果被设置了父对象，会被 Qt 自动管理内存
    // formattingHandler is a child of MainWindow, so it will be deleted automatically.
    // peerInfoDisplayWidget is a child of MainWindow, so it will be deleted automatically.
}

void MainWindow::loadOrCreateUserIdentity()
{
    QSettings settings;
    localUserUuid = settings.value("User/LocalUUID").toString();
    if (localUserUuid.isEmpty())
    {
        localUserUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("User/LocalUUID", localUserUuid);
    }
    // 如果之前没有保存用户名，则使用默认的 "Me"，否则加载已保存的
    localUserName = settings.value("User/LocalUserName", tr("Me")).toString();
    if (localUserName.isEmpty())
    { // 确保用户名不为空
        localUserName = tr("Me");
        settings.setValue("User/LocalUserName", localUserName);
    }
}

QString MainWindow::getLocalUserName() const
{
    return localUserName;
}

QString MainWindow::getLocalUserUuid() const
{
    return localUserUuid;
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

    leftSidebarLayout->addStretch(); // 添加垂直填充，将后续控件推到底部

    // 添加设置按钮
    settingsButton = new QPushButton(this);
    settingsButton->setObjectName("settingsButton");
    settingsButton->setIcon(QIcon(":/icons/settings.svg")); // 假设您的SVG图标路径
    settingsButton->setIconSize(QSize(24, 24));
    settingsButton->setToolTip(tr("Settings"));
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsButtonClicked);
    leftSidebarLayout->addWidget(settingsButton); // 添加到布局的底部

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

    peerInfoDisplayWidget = new PeerInfoWidget(this); // Create instance of new widget
    // Ensure it's added to the layout correctly
    if (activeChatContentsLayout)
    {
        activeChatContentsLayout->insertWidget(0, peerInfoDisplayWidget);
    }
    if (peerInfoDisplayWidget)
        peerInfoDisplayWidget->clearDisplay();

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
    italicButton = new QPushButton("I", this);
    italicButton->setCheckable(true);
    underlineButton = new QPushButton("U", this);
    underlineButton->setCheckable(true);
    colorButton = new QPushButton(this); // 创建前景颜色按钮
    colorButton->setObjectName("colorButton");
    colorButton->setToolTip("Text Color");
    QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentTextColor.name());
    colorButton->setStyleSheet(colorStyle);
    bgColorButton = new QPushButton(this); // 创建背景色按钮
    bgColorButton->setObjectName("bgColorButton");
    bgColorButton->setToolTip("Background Color");
    QString bgColorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentBgColor.name());
    bgColorButton->setStyleSheet(bgColorStyle);
    fontSizeComboBox = new QComboBox(this); // 字体大小选择器
    for (int i = 8; i <= 28; i += 2)
    {
        fontSizeComboBox->addItem(QString::number(i));
    }
    fontFamilyComboBox = new QFontComboBox(this); // 字体选择器
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

    // Instantiate FormattingToolbarHandler
    formattingHandler = new FormattingToolbarHandler(
        messageInputEdit,
        boldButton, italicButton, underlineButton,
        colorButton, bgColorButton,
        fontSizeComboBox, fontFamilyComboBox,
        currentTextColor, currentBgColor, // Pass initial colors
        this, // Parent for QColorDialog
        this  // Parent for the handler itself
    );

    // Connect formatting toolbar signals to FormattingToolbarHandler slots
    connect(boldButton, &QPushButton::toggled, formattingHandler, &FormattingToolbarHandler::onBoldButtonToggled);
    connect(italicButton, &QPushButton::toggled, formattingHandler, &FormattingToolbarHandler::onItalicButtonToggled);
    connect(underlineButton, &QPushButton::toggled, formattingHandler, &FormattingToolbarHandler::onUnderlineButtonToggled);
    connect(colorButton, &QPushButton::clicked, formattingHandler, &FormattingToolbarHandler::onColorButtonClicked);
    connect(bgColorButton, &QPushButton::clicked, formattingHandler, &FormattingToolbarHandler::onBgColorButtonClicked);
    connect(fontSizeComboBox, &QComboBox::currentTextChanged, formattingHandler, &FormattingToolbarHandler::onFontSizeChanged);
    connect(fontFamilyComboBox, &QFontComboBox::currentFontChanged, formattingHandler, &FormattingToolbarHandler::onFontFamilyChanged);

    // Connect editor's format change to handler's update slot
    connect(messageInputEdit, &QTextEdit::currentCharFormatChanged, formattingHandler, &FormattingToolbarHandler::updateFormatButtons);

    // Connect handler's color change signals back to MainWindow to update stored colors
    connect(formattingHandler, &FormattingToolbarHandler::textColorChanged, this, &MainWindow::handleTextColorChanged);
    connect(formattingHandler, &FormattingToolbarHandler::backgroundColorChanged, this, &MainWindow::handleBackgroundColorChanged);

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
    formattingHandler->updateFormatButtons(messageInputEdit->currentCharFormat());

    // Apply the global stylesheet
    this->setStyleSheet(StyleUtils::getApplicationStyleSheet());

    // Apply widget-specific font and size settings (moved from old applyStyles)
    QFont formattingButtonFont;
    formattingButtonFont.setPixelSize(14);

    QFont boldIconFont = formattingButtonFont;
    boldIconFont.setBold(true);
    boldButton->setFont(boldIconFont);

    QFont italicIconFont = formattingButtonFont;
    italicIconFont.setItalic(true);
    italicButton->setFont(italicIconFont);

    QFont underlineIconFont = formattingToolbarWidget->font(); // Use toolbar's font as base
    underlineIconFont.setUnderline(true);
    underlineIconFont.setPixelSize(14); // Ensure pixel size is set
    underlineButton->setFont(underlineIconFont);

    boldButton->setFixedSize(30, 30);
    italicButton->setFixedSize(30, 30);
    underlineButton->setFixedSize(30, 30);
    colorButton->setFixedSize(30, 30);
    bgColorButton->setFixedSize(30, 30);
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
    defaultFormat.setForeground(currentTextColor); // Use MainWindow's currentTextColor
    defaultFormat.setBackground(currentBgColor); // Use MainWindow's currentBgColor
    messageInputEdit->setCurrentCharFormat(defaultFormat);
}

void MainWindow::onAddContactButtonClicked()
{
    contactManager->showAddContactDialog(this);
}

void MainWindow::onSettingsButtonClicked()
{
    // 使用现有的对话框实例，或者如果不存在则创建
    if (!settingsDialog)
    {
        settingsDialog = new SettingsDialog(localUserName, // 传递当前用户名
                                            localUserUuid, // 传递UUID
                                            localListenPort,
                                            localOutgoingPort, useSpecificOutgoingPort,
                                            this);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::handleSettingsApplied);
    }
    else
    {
        // Update dialog with current settings if it already exists
        settingsDialog->updateFields(localUserName, localUserUuid, localListenPort, localOutgoingPort, useSpecificOutgoingPort);
    }
    settingsDialog->exec(); // 以模态方式显示对话框
}

void MainWindow::handleSettingsApplied(const QString &userName,
                                       quint16 listenPort,
                                       quint16 outgoingPort, bool useSpecificOutgoingPortVal)
{
    bool settingsChanged = false;
    bool networkRestartNeeded = false;

    if (localUserName != userName)
    {
        localUserName = userName;
        QSettings settings;
        settings.setValue("User/LocalUserName", localUserName); // 保存更改的用户名
        if (networkManager)
        {
            networkManager->setLocalUserDetails(localUserUuid, localUserName); // 通知NetworkManager
        }
        settingsChanged = true;
    }

    if (localListenPort != listenPort)
    {
        localListenPort = listenPort;
        settingsChanged = true;
        networkRestartNeeded = true; // 监听端口更改需要重启监听
    }

    if (localOutgoingPort != outgoingPort || useSpecificOutgoingPort != useSpecificOutgoingPortVal)
    {
        localOutgoingPort = outgoingPort;
        useSpecificOutgoingPort = useSpecificOutgoingPortVal;
        networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);
        settingsChanged = true;
        // 传出端口设置更改不需要重启监听，它会在下次连接时生效
        updateNetworkStatus(tr("Outgoing port preference updated. Will apply to new connections."));
    }

    if (networkRestartNeeded)
    {
        updateNetworkStatus(tr("Listen port settings changed. Restarting listener..."));
        networkManager->setListenPreferences(localListenPort);
        networkManager->stopListening();  // 显式停止
        networkManager->startListening(); // 使用新设置重新启动
    }

    if (settingsChanged)
    {
        updateNetworkStatus(tr("Settings applied. User: %1, Listen Port: %2, Outgoing Port: %3")
                                .arg(localUserName)
                                .arg(QString::number(localListenPort))
                                .arg(useSpecificOutgoingPort && localOutgoingPort > 0 ? QString::number(localOutgoingPort) : tr("Dynamic")));
    }
    else if (!networkRestartNeeded)
    { // 避免在仅重启网络时显示 "unchanged"
        updateNetworkStatus(tr("Settings unchanged."));
    }
}

void MainWindow::handleContactAdded(const QString &name, const QString &uuid)
{
    if (name.isEmpty() || uuid.isEmpty())
    {
        qWarning() << "Attempted to add contact with empty name or UUID:" << name << uuid;
        return;
    }

    // Check contactListWidget for existing UUID first
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *existingItem = contactListWidget->item(i);
        if (existingItem->data(Qt::UserRole).toString() == uuid)
        {
            // UUID exists. Update name if different, then select.
            if (existingItem->text() != name)
            {
                existingItem->setText(name); // Update display name
            }
            contactListWidget->setCurrentItem(existingItem);
            return;
        }
    }

    QListWidgetItem *itemToSelect = new QListWidgetItem(name, contactListWidget);
    itemToSelect->setData(Qt::UserRole, uuid); // Store UUID in item's data

    if (chatHistories.find(uuid) == chatHistories.end())
    { // Use UUID as key for chat history
        chatHistories[uuid] = QStringList();
    }

    contactListWidget->setCurrentItem(itemToSelect);
}

void MainWindow::onContactSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current)
    {
        currentOpenChatContactName = current->text();
        QString peerUuid = current->data(Qt::UserRole).toString();
        if (peerUuid.isEmpty())
            peerUuid = tr("N/A (No UUID)");

        QPair<QString, quint16> peerNetInfo;
        // Check if the selected contact is the currently active network connection
        if (networkManager && networkManager->getCurrentSocketState() == QAbstractSocket::ConnectedState &&
            networkManager->getPeerInfo().first == currentOpenChatContactName &&
            networkManager->getCurrentPeerUuid() == peerUuid)
        { // This call should now work
            peerNetInfo = networkManager->getPeerInfo();
            QString peerIpAddress = networkManager->getCurrentPeerIpAddress(); // Get IP specifically
            if (peerInfoDisplayWidget)
                peerInfoDisplayWidget->updateDisplay(peerNetInfo.first, peerUuid, peerIpAddress, peerNetInfo.second);
        }
        else
        {
            if (peerInfoDisplayWidget)
                peerInfoDisplayWidget->updateDisplay(currentOpenChatContactName, peerUuid, tr("N/A"), 0);
        }

        QStringList messagesToDisplay;
        // Use UUID for chat history (as per previous correct change)
        QString currentContactUuid = current->data(Qt::UserRole).toString();
        if (!currentContactUuid.isEmpty() && chatHistories.contains(currentContactUuid))
        {
            messagesToDisplay = chatHistories.value(currentContactUuid);
        }
        else if (!currentContactUuid.isEmpty())
        {
            chatHistories[currentContactUuid] = QStringList(); // Initialize if new
        }
        else
        {
            qWarning() << "Selected contact" << currentOpenChatContactName << "has no UUID for chat history.";
            // Fallback to name-based if UUID is somehow empty, though this shouldn't happen with proper UUID management
            if (chatHistories.contains(currentOpenChatContactName))
            {
                messagesToDisplay = chatHistories.value(currentOpenChatContactName);
            }
            else
            {
                chatHistories[currentOpenChatContactName] = QStringList();
            }
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
            if (peerInfoDisplayWidget)
                peerInfoDisplayWidget->clearDisplay();
        }
    }
}

void MainWindow::onSendButtonClicked()
{
    if (currentOpenChatContactName.isEmpty())
    {
        updateNetworkStatus(tr("No active chat to send message."));
        return;
    }
    if (!networkManager || networkManager->getCurrentSocketState() != QAbstractSocket::ConnectedState)
    {
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
                                      "<span style=\"font-weight: bold; background-color: #a7dcb2; padding: 2px 6px; margin-left: 4px; border-radius: 3px;\">%1:</span> %2"
                                      "</p>"
                                      "</div>")
                                      .arg(localUserName.toHtmlEscaped())
                                      .arg(coreContent);

        QString activeContactUuid = "";
        QListWidgetItem *currentItem = contactListWidget->currentItem();
        if (currentItem)
        {
            activeContactUuid = currentItem->data(Qt::UserRole).toString();
        }

        if (!activeContactUuid.isEmpty())
        {
            chatHistories[activeContactUuid].append(userMessageHtml); // Use UUID for history key
        }
        else
        {
            qWarning() << "Sending message: Active contact" << currentOpenChatContactName << "has no UUID. Using name as fallback for history.";
            chatHistories[currentOpenChatContactName].append(userMessageHtml);
        }

        messageDisplay->addMessage(userMessageHtml);

        networkManager->sendMessage(coreContent);

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
        defaultFormat.setForeground(currentTextColor); // Use MainWindow's currentTextColor
        defaultFormat.setBackground(currentBgColor); // Use MainWindow's currentBgColor
        messageInputEdit->setCurrentCharFormat(defaultFormat);

        messageInputEdit->setFocus();
    }
}

void MainWindow::handleTextColorChanged(const QColor &color)
{
    currentTextColor = color;
}

void MainWindow::handleBackgroundColorChanged(const QColor &color)
{
    currentBgColor = color;
}

void MainWindow::handleNetworkConnected()
{
    updateNetworkStatus(tr("Connected."));
    if (networkManager && !networkManager->getPeerInfo().first.isEmpty())
    {
        QPair<QString, quint16> peerInfo = networkManager->getPeerInfo();
        QString peerUuid = networkManager->getCurrentPeerUuid();
        QString peerIpAddress = networkManager->getCurrentPeerIpAddress();
        if (peerUuid.isEmpty())
            peerUuid = tr("N/A (UUID not received)");

        if (peerInfoDisplayWidget)
            peerInfoDisplayWidget->updateDisplay(peerInfo.first, peerUuid, peerIpAddress, peerInfo.second);

        QString peerName = peerInfo.first;

        // Check contactListWidget for existing UUID first
        QListWidgetItem *itemToSelect = nullptr;
        if (!peerUuid.isEmpty())
        {
            for (int i = 0; i < contactListWidget->count(); ++i)
            {
                QListWidgetItem *existingItem = contactListWidget->item(i);
                if (existingItem->data(Qt::UserRole).toString() == peerUuid)
                {
                    itemToSelect = existingItem;
                    // Update name if it changed (e.g. peer updated their display name)
                    if (itemToSelect->text() != peerName)
                    {
                        itemToSelect->setText(peerName);
                    }
                    break;
                }
            }
        }

        if (!itemToSelect)
        { // If not found by UUID, try by name (less reliable) or add new
            QList<QListWidgetItem *> itemsByName = contactListWidget->findItems(peerName, Qt::MatchExactly);
            if (!itemsByName.isEmpty() && peerUuid.isEmpty())
            { // Found by name, and no UUID from peer (should not happen with new protocol)
                itemToSelect = itemsByName.first();
            }
            else if (!itemsByName.isEmpty() && !peerUuid.isEmpty())
            {                                           // Found by name, but UUID is different or item has no UUID
                handleContactAdded(peerName, peerUuid); // This will create or update based on UUID
                for (int i = 0; i < contactListWidget->count(); ++i)
                {
                    if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
                    {
                        itemToSelect = contactListWidget->item(i);
                        break;
                    }
                }
            }
            else
            { // Not found by name either, or UUID is present and unique
                handleContactAdded(peerName, peerUuid);
                for (int i = 0; i < contactListWidget->count(); ++i)
                {
                    if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
                    {
                        itemToSelect = contactListWidget->item(i);
                        break;
                    }
                }
            }
        }

        if (itemToSelect)
        {
            contactListWidget->setCurrentItem(itemToSelect);
        }

        if (chatStackedWidget->currentWidget() != activeChatContentsWidget)
        {
            chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
        }
        messageInputEdit->setFocus();
    }
}

void MainWindow::handleNetworkDisconnected()
{
    updateNetworkStatus(tr("Disconnected."));
    if (peerInfoDisplayWidget && networkManager)
    {
        // Get the last known name/UUID if possible, otherwise they might be empty
        QString lastName = networkManager->getPeerInfo().first;  // Might be empty if fully disconnected
        QString lastUuid = networkManager->getCurrentPeerUuid(); // Might be empty
        peerInfoDisplayWidget->setDisconnectedState(lastName, lastUuid);
    }
    else if (peerInfoDisplayWidget)
    {
        peerInfoDisplayWidget->setDisconnectedState(tr("N/A"), tr("N/A"));
    }
}

void MainWindow::handleNewMessageReceived(const QString &message)
{
    QString peerName = "";
    QString peerUuid = "";

    if (networkManager)
    {
        peerName = networkManager->getPeerInfo().first;
        peerUuid = networkManager->getCurrentPeerUuid(); // Get UUID of sender
    }

    if (peerName.isEmpty() || peerUuid.isEmpty())
    {
        updateNetworkStatus(tr("Received message from unknown peer or peer without UUID."));
        qWarning() << "Received message from peer with no name/UUID via NetworkManager:" << peerName << peerUuid;
        return;
    }

    QListWidgetItem *contactItem = nullptr;
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
        {
            contactItem = contactListWidget->item(i);
            if (contactItem->text() != peerName)
            {
                contactItem->setText(peerName);
            }
            break;
        }
    }

    if (!contactItem)
    {
        handleContactAdded(peerName, peerUuid);
        for (int i = 0; i < contactListWidget->count(); ++i)
        {
            if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
            {
                contactItem = contactListWidget->item(i);
                break;
            }
        }
    }

    if (!contactItem || contactListWidget->currentItem() != contactItem)
    {
        if (contactItem)
        {
            contactListWidget->setCurrentItem(contactItem);
        }
        else
        {
            qWarning() << "Could not find or add contact for incoming message from UUID:" << peerUuid;
            return;
        }
    }
    QString activeContactDisplayName = contactItem ? contactItem->text() : peerName;

    QString receivedMessageHtml = QString(
                                      "<div style=\"text-align: left; margin-bottom: 2px;\">"
                                      "<p style=\"margin:0; padding:0; text-align: left;\">"
                                      "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span> %2"
                                      "</p>"
                                      "</div>")
                                      .arg(activeContactDisplayName.toHtmlEscaped())
                                      .arg(message);

    chatHistories[peerUuid].append(receivedMessageHtml);

    QListWidgetItem *currentUiItem = contactListWidget->currentItem();
    if (currentUiItem && currentUiItem->data(Qt::UserRole).toString() == peerUuid)
    {
        messageDisplay->addMessage(receivedMessageHtml);
    }
    else
    {
        updateNetworkStatus(tr("New message from %1").arg(activeContactDisplayName));
        if (contactItem)
            contactItem->setBackground(Qt::lightGray);
    }
}

void MainWindow::handleNetworkError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (networkManager)
    {
        updateNetworkStatus(tr("Network Error: %1").arg(networkManager->getLastError()));
    }
}

void MainWindow::updateNetworkStatus(const QString &status)
{
    if (networkStatusLabel)
    {
        networkStatusLabel->setText(status);
    }
    else
    {
        statusBar()->showMessage(status, 5000);
    }
}

void MainWindow::handleIncomingConnectionRequest(const QString &peerAddress, quint16 peerPort, const QString &peerUuid, const QString &peerNameHint)
{
    QMessageBox::StandardButton reply;
    QString suggestedName = peerNameHint.isEmpty() ? peerAddress : peerNameHint;

    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *item = contactListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == peerUuid)
        {
            QMessageBox::StandardButton reconReply = QMessageBox::question(this, tr("Existing Contact"),
                                                                           tr("Contact '%1' (UUID: %2) is trying to connect from %3:%4.\nUpdate and connect?")
                                                                               .arg(item->text())
                                                                               .arg(peerUuid)
                                                                               .arg(peerAddress)
                                                                               .arg(peerPort),
                                                                           QMessageBox::Yes | QMessageBox::No);
            if (reconReply == QMessageBox::Yes)
            {
                networkManager->acceptPendingConnection(item->text());
            }
            else
            {
                networkManager->rejectPendingConnection();
            }
            return;
        }
    }

    reply = QMessageBox::question(this, tr("Incoming Connection"),
                                  tr("Accept connection from %1 (UUID: %2, Name Hint: '%3') at %4:%5?")
                                      .arg(peerAddress)
                                      .arg(peerUuid)
                                      .arg(peerNameHint.isEmpty() ? tr("N/A") : peerNameHint)
                                      .arg(peerAddress)
                                      .arg(peerPort),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        bool ok;
        QString contactName = QInputDialog::getText(this, tr("Name Contact"),
                                                    tr("Enter a name for this contact (UUID: %1):").arg(peerUuid), QLineEdit::Normal,
                                                    suggestedName, &ok);
        if (ok && !contactName.isEmpty())
        {
            networkManager->acceptPendingConnection(contactName);
        }
        else if (ok && contactName.isEmpty())
        {
            networkManager->acceptPendingConnection(suggestedName.isEmpty() ? peerAddress : suggestedName);
        }
        else
        {
            networkManager->rejectPendingConnection();
            updateNetworkStatus(tr("Incoming connection naming cancelled. Rejected."));
        }
    }
    else
    {
        networkManager->rejectPendingConnection();
    }
}
