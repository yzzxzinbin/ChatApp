#include "mainwindow.h"
#include "peerinfowidget.h"
#include "formattingtoolbarhandler.h"
#include "styleutils.h"
#include "chatmessagedisplay.h"

#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QFontComboBox>
#include <QApplication>
#include <QLabel>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTextCharFormat>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>
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

    // 为 messageInputEdit 安装事件过滤器
    messageInputEdit->installEventFilter(this); // 'this' 指向 MainWindow 实例
}