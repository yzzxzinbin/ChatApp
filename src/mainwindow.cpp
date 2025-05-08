#include "mainwindow.h"
#include "contactmanager.h"
#include "chatmessagedisplay.h"

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
#include <QColorDialog> // 添加颜色对话框头文件

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    contactManager = new ContactManager(this);
    connect(contactManager, &ContactManager::contactAdded, this, &MainWindow::handleContactAdded);

    // 初始化默认文本颜色为黑色
    currentTextColor = QColor(Qt::black);
    
    setupUI();
    applyStyles();
    setWindowTitle("Chat Application");
    resize(1024, 768);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    leftSidebar = new QWidget(this);
    leftSidebar->setObjectName("leftSidebar");
    leftSidebar->setFixedWidth(50);
    leftSidebarLayout = new QVBoxLayout(leftSidebar);
    leftSidebarLayout->setContentsMargins(5, 5, 5, 5);
    leftSidebarLayout->setSpacing(10);

    addContactButton = new QPushButton(this);
    addContactButton->setObjectName("addContactButton");
    addContactButton->setIcon(QIcon(":/icons/add_user.svg"));
    addContactButton->setIconSize(QSize(24, 24));
    addContactButton->setToolTip("Add new contact");
    connect(addContactButton, &QPushButton::clicked, this, &MainWindow::onAddContactButtonClicked);
    leftSidebarLayout->addWidget(addContactButton);

    leftSidebarLayout->addStretch();
    mainLayout->addWidget(leftSidebar);

    contactListWidget = new QListWidget(this);
    contactListWidget->setObjectName("contactListWidget");
    contactListWidget->setMaximumWidth(250);
    contactListWidget->setMinimumWidth(200);
    connect(contactListWidget, &QListWidget::currentItemChanged, this, &MainWindow::onContactSelected);
    mainLayout->addWidget(contactListWidget, 0);

    chatAreaWidget = new QWidget(this);
    chatAreaWidget->setObjectName("chatAreaWidget");
    chatAreaLayout = new QVBoxLayout(chatAreaWidget);
    chatAreaLayout->setSpacing(0);
    chatAreaLayout->setContentsMargins(0, 0, 0, 0);

    chatStackedWidget = new QStackedWidget(this);

    activeChatContentsWidget = new QWidget(this);
    activeChatContentsLayout = new QVBoxLayout(activeChatContentsWidget);
    activeChatContentsLayout->setSpacing(6);
    activeChatContentsLayout->setContentsMargins(10, 10, 10, 10);

    messageDisplay = new ChatMessageDisplay(this);
    messageDisplay->setObjectName("messageDisplay");
    messageDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    formattingToolbarWidget = new QWidget(this);
    formattingToolbarWidget->setObjectName("formattingToolbarWidget");
    formattingToolbarLayout = new QHBoxLayout(formattingToolbarWidget);
    formattingToolbarLayout->setContentsMargins(0, 0, 0, 0);
    formattingToolbarLayout->setSpacing(5);

    boldButton = new QPushButton("B", this);
    boldButton->setCheckable(true);
    connect(boldButton, &QPushButton::toggled, this, &MainWindow::onBoldButtonToggled);

    italicButton = new QPushButton("I", this);
    italicButton->setCheckable(true);
    connect(italicButton, &QPushButton::toggled, this, &MainWindow::onItalicButtonToggled);

    underlineButton = new QPushButton("U", this);
    underlineButton->setCheckable(true);
    connect(underlineButton, &QPushButton::toggled, this, &MainWindow::onUnderlineButtonToggled);
    
    // 创建颜色按钮
    colorButton = new QPushButton(this);
    colorButton->setObjectName("colorButton");
    colorButton->setToolTip("Text Color");
    // 设置初始颜色为黑色
    QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentTextColor.name());
    colorButton->setStyleSheet(colorStyle);
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::onColorButtonClicked);

    fontSizeComboBox = new QComboBox(this);
    for (int i = 8; i <= 28; i += 2) {
        fontSizeComboBox->addItem(QString::number(i));
    }
    connect(fontSizeComboBox, &QComboBox::currentTextChanged, this, &MainWindow::onFontSizeChanged);

    fontFamilyComboBox = new QFontComboBox(this);
    connect(fontFamilyComboBox, &QFontComboBox::currentFontChanged, this, &MainWindow::onFontFamilyChanged);

    formattingToolbarLayout->addWidget(boldButton);
    formattingToolbarLayout->addWidget(italicButton);
    formattingToolbarLayout->addWidget(underlineButton);
    formattingToolbarLayout->addWidget(colorButton);  // 添加颜色按钮到工具栏
    formattingToolbarLayout->addWidget(fontSizeComboBox);
    formattingToolbarLayout->addWidget(fontFamilyComboBox, 1);

    inputAreaWidget = new QWidget(this);
    inputAreaWidget->setObjectName("inputAreaWidget");
    inputAreaLayout = new QHBoxLayout(inputAreaWidget);
    inputAreaLayout->setContentsMargins(0, 0, 0, 0);
    inputAreaLayout->setSpacing(5);

    messageInputEdit = new QTextEdit(this);
    messageInputEdit->setObjectName("messageInputEdit");
    messageInputEdit->setPlaceholderText("Type your message...");
    messageInputEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageInputEdit->setMaximumHeight(120);
    connect(messageInputEdit, &QTextEdit::currentCharFormatChanged, this, &MainWindow::onCurrentCharFormatChanged);
    inputAreaLayout->addWidget(messageInputEdit, 1);

    buttonsWidget = new QWidget(this);
    buttonsLayout = new QVBoxLayout(buttonsWidget);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(5);
    buttonsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    buttonsWidget->setFixedWidth(100);

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

    buttonsLayout->addWidget(sendButton, 1);
    buttonsLayout->addWidget(clearButton, 1);
    buttonsLayout->addWidget(closeChatButton, 1);

    inputAreaLayout->addWidget(buttonsWidget);

    activeChatContentsLayout->addWidget(messageDisplay, 1);
    activeChatContentsLayout->addWidget(formattingToolbarWidget, 0);
    activeChatContentsLayout->addWidget(inputAreaWidget, 0);

    emptyChatPlaceholderLabel = new QLabel("Select a contact to start chatting or add a new one.", this);
    emptyChatPlaceholderLabel->setObjectName("emptyChatPlaceholderLabel");
    emptyChatPlaceholderLabel->setAlignment(Qt::AlignCenter);

    chatStackedWidget->addWidget(emptyChatPlaceholderLabel);
    chatStackedWidget->addWidget(activeChatContentsWidget);

    chatAreaLayout->addWidget(chatStackedWidget);
    mainLayout->addWidget(chatAreaWidget, 1);
    centralWidget->setLayout(mainLayout);

    chatStackedWidget->setCurrentWidget(emptyChatPlaceholderLabel);

    QTextCharFormat initialFormat;
    if (fontFamilyComboBox->currentFont().pointSize() > 0) {
        initialFormat.setFont(fontFamilyComboBox->currentFont());
    } else {
        initialFormat.setFontFamilies({QApplication::font().family()});
    }
    bool sizeFound = false;
    for (int i = 0; i < fontSizeComboBox->count(); ++i) {
        if (fontSizeComboBox->itemText(i) == "12") {
            fontSizeComboBox->setCurrentIndex(i);
            initialFormat.setFontPointSize(12);
            sizeFound = true;
            break;
        }
    }
    if (!sizeFound && fontSizeComboBox->count() > 0) {
        fontSizeComboBox->setCurrentIndex(0);
        initialFormat.setFontPointSize(fontSizeComboBox->currentText().toInt());
    } else if (!sizeFound) {
        initialFormat.setFontPointSize(12);
    }
    initialFormat.setForeground(currentTextColor); // 设置初始文本颜色
    messageInputEdit->setCurrentCharFormat(initialFormat);
    onCurrentCharFormatChanged(messageInputEdit->currentCharFormat());
}

void MainWindow::onClearButtonClicked()
{
    messageInputEdit->clear();
    QTextCharFormat defaultFormat;
    if (fontFamilyComboBox->currentFont().pointSize() > 0) {
        defaultFormat.setFont(fontFamilyComboBox->currentFont());
    } else {
        defaultFormat.setFontFamilies({QApplication::font().family()});
    }
    defaultFormat.setFontPointSize(fontSizeComboBox->currentText().toInt());
    defaultFormat.setForeground(currentTextColor); // 保留当前文本颜色
    messageInputEdit->setCurrentCharFormat(defaultFormat);
}

void MainWindow::onAddContactButtonClicked()
{
    contactManager->showAddContactDialog(this);
}

void MainWindow::handleContactAdded(const QString &name)
{
    if (chatHistories.find(name) == chatHistories.end()) {
        chatHistories[name] = QStringList();
    }
    QListWidgetItem* newItem = new QListWidgetItem(name, contactListWidget);
    contactListWidget->setCurrentItem(newItem);
}

void MainWindow::onContactSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current) {
        currentOpenChatContactName = current->text();
        
        QStringList messagesToDisplay;
        if (chatHistories.contains(currentOpenChatContactName)) {
            messagesToDisplay = chatHistories.value(currentOpenChatContactName);
            if (messagesToDisplay.isEmpty()) {
                QString welcomeMessageText = "Hello! How are you?";
                QString welcomeMessageHtml = QString(
                    "<div style=\"text-align: left; margin-bottom: 2px;\">"
                    "<p style=\"margin:0; padding:0; text-align: left;\">"
                    "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span><br> %2"
                    "</p>"
                    "</div>"
                ).arg(currentOpenChatContactName.toHtmlEscaped())
                 .arg(welcomeMessageText.toHtmlEscaped());
                
                chatHistories[currentOpenChatContactName].append(welcomeMessageHtml);
                messagesToDisplay.append(welcomeMessageHtml);
            }
        } else {
            chatHistories[currentOpenChatContactName] = QStringList();
            QString welcomeMessageText = "Hello! How are you?";
            QString welcomeMessageHtml = QString(
                "<div style=\"text-align: left; margin-bottom: 2px;\">"
                "<p style=\"margin:0; padding:0; text-align: left;\">"
                "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span> %2"
                "</p>"
                "</div>"
            ).arg(currentOpenChatContactName.toHtmlEscaped())
             .arg(welcomeMessageText.toHtmlEscaped());
            
            chatHistories[currentOpenChatContactName].append(welcomeMessageHtml);
            messagesToDisplay.append(welcomeMessageHtml);
        }

        messageDisplay->setMessages(messagesToDisplay);

        messageInputEdit->clear();
        messageInputEdit->setFocus();
        if (chatStackedWidget->currentWidget() != activeChatContentsWidget) {
            chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
        }
    } else {
        currentOpenChatContactName.clear();
        messageDisplay->clear();
        messageInputEdit->clear();
        if (chatStackedWidget->currentWidget() != emptyChatPlaceholderLabel) {
            chatStackedWidget->setCurrentWidget(emptyChatPlaceholderLabel);
        }
    }
}

void MainWindow::onSendButtonClicked()
{
    if (currentOpenChatContactName.isEmpty()) return;

    QString plainMessageText = messageInputEdit->toPlainText().trimmed();
    if (!plainMessageText.isEmpty()) {
        QString messageContentHtml = messageInputEdit->toHtml();

        QTextDocument doc;
        doc.setHtml(messageContentHtml);
        QString innerBodyHtml = doc.toHtml();

        QString coreContent = innerBodyHtml;
        if (coreContent.startsWith("<p", Qt::CaseInsensitive) && coreContent.count("<p", Qt::CaseInsensitive) == 1 && coreContent.endsWith("</p>", Qt::CaseInsensitive)) {
            int pTagEnd = coreContent.indexOf('>');
            int pEndTagStart = coreContent.lastIndexOf("</p>", -1, Qt::CaseInsensitive);
            if (pTagEnd != -1 && pEndTagStart > pTagEnd) {
                coreContent = coreContent.mid(pTagEnd + 1, pEndTagStart - (pTagEnd + 1));
            }
        }
        if (coreContent.trimmed().isEmpty() && !plainMessageText.isEmpty()) {
             QTextDocumentFragment fragment = QTextDocumentFragment::fromHtml(messageInputEdit->toHtml());
             coreContent = fragment.toHtml();
        }

        QString userMessageHtml = QString(
            "<div style=\"text-align: right; margin-bottom: 2px;\">"
            "<p style=\"margin:0; padding:0; text-align: right;\">"
            "<span style=\"font-weight: bold; background-color: #a7dcb2; padding: 2px 6px; margin-left: 4px; border-radius: 3px;\">Me:</span> %1"
            "</p>"
            "</div>"
        ).arg(coreContent);

        chatHistories[currentOpenChatContactName].append(userMessageHtml);
        
        messageDisplay->addMessage(userMessageHtml);
        
        messageInputEdit->clear();
        QTextCharFormat defaultFormat;
        if (fontFamilyComboBox->currentFont().pointSize() > 0) {
            defaultFormat.setFont(fontFamilyComboBox->currentFont());
        } else {
            defaultFormat.setFontFamilies({QApplication::font().family()});
        }
        defaultFormat.setFontPointSize(fontSizeComboBox->currentText().toInt());
        defaultFormat.setForeground(currentTextColor); // 保留当前文本颜色
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
    if (ok && pointSize > 0) {
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
    if (color.isValid()) {
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
    if (pointSize <= 0 && format.font().pointSize() > 0) pointSize = format.font().pointSize();
    if (pointSize <= 0) pointSize = QApplication::font().pointSize();
    if (pointSize <= 0) pointSize = 12;

    QString sizeStr = QString::number(pointSize);
    int index = fontSizeComboBox->findText(sizeStr);
    if (index != -1) {
        fontSizeComboBox->setCurrentIndex(index);
    } else {
        fontSizeComboBox->setCurrentText(sizeStr);
    }
    fontSizeComboBox->blockSignals(false);

    fontFamilyComboBox->blockSignals(true);
    QFont fontFromFormat = format.font();
    QStringList currentFamilies = fontFromFormat.families();

    if (!currentFamilies.isEmpty()) {
        fontFamilyComboBox->setCurrentFont(fontFromFormat);
    } else {
        fontFamilyComboBox->setCurrentFont(fontFromFormat);
    }
    fontFamilyComboBox->blockSignals(false);
    
    // 更新当前颜色
    if (format.hasProperty(QTextFormat::ForegroundBrush)) {
        currentTextColor = format.foreground().color();
        QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentTextColor.name());
        colorButton->setStyleSheet(colorStyle);
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
        
        #colorButton {
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            border-radius: 4px;
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

    boldButton->setFixedSize(30,30);
    italicButton->setFixedSize(30,30);
    underlineButton->setFixedSize(30,30);
    colorButton->setFixedSize(30,30);
}
