#include "mainwindow.h"
#include "contactmanager.h"
#include "chatmessagedisplay.h"
#include "networkmanager.h"           // 确保包含了 NetworkManager
#include "settingsdialog.h"           // 确保包含了 SettingsDialog
#include "peerinfowidget.h"           // Include the new PeerInfoWidget header
#include "styleutils.h"               // Include the new StyleUtils header
#include "formattingtoolbarhandler.h" // Include the new FormattingToolbarHandler header
#include "networkeventhandler.h"      // Include the new NetworkEventHandler header
#include "chathistorymanager.h"       // 新增：包含 ChatHistoryManager

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
#include <QDebug> // Ensure QDebug is included for qDebug()
#include <QEvent> // 新增：包含 QEvent，尽管 QKeyEvent 可能已间接包含它

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      networkManager(nullptr),           // 初始化 networkManager 为 nullptr
      settingsDialog(nullptr),           // 初始化 settingsDialog 为 nullptr
      localUserName(tr("Me")),           // 默认用户名
      localUserUuid(QString()),          // 初始化UUID为空
      localListenPort(60248),            // 默认监听端口 60248
      autoNetworkListeningEnabled(true), // 新增：默认启用监听
      localOutgoingPort(0),              // 默认传出端口为0 (动态)
      useSpecificOutgoingPort(false)     // 默认不指定传出端口
{
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    loadOrCreateUserIdentity();

    chatHistoryManager = new ChatHistoryManager(QCoreApplication::applicationName(), this); // 新增：创建 ChatHistoryManager
                                                                                            // 加载或创建用户UUID和名称

    // 1. 首先初始化 NetworkManager
    networkManager = new NetworkManager(this);
    networkManager->setLocalUserDetails(localUserUuid, localUserName); // 设置本地用户信息
    // 在 NetworkManager 启动监听前设置初始首选项
    networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled); // 传递启用状态
    // 设置初始传出连接首选项
    networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);

    // 2. 然后初始化 ContactManager，并将 NetworkManager 实例传递给它
    contactManager = new ContactManager(networkManager, this);
    connect(contactManager, &ContactManager::contactAdded, this, &MainWindow::handleContactAdded);

    // 初始化默认文本颜色和背景色
    currentTextColor = QColor(Qt::black);
    currentBgColor = QColor(Qt::transparent); // 默认背景色为透明

    setupUI(); // setupUI must be called before networkEventHandler is created if it needs UI pointers

    // Instantiate NetworkEventHandler
    networkEventHandler = new NetworkEventHandler(
        networkManager,
        contactListWidget,
        messageDisplay,
        peerInfoDisplayWidget,
        chatStackedWidget,
        messageInputEdit,
        emptyChatPlaceholderLabel,
        activeChatContentsWidget,
        &chatHistories, // Pass address of the map
        this,           // Pass MainWindow instance
        this            // Parent object
    );

    setWindowTitle("ChatApp - " + localUserName + "By CCZU_ZX");
    resize(1024, 768);

    // Connect NetworkManager signals to NetworkEventHandler slots
    // 更新连接以匹配新的信号和槽签名
    connect(networkManager, &NetworkManager::peerConnected, networkEventHandler, &NetworkEventHandler::handlePeerConnected);
    connect(networkManager, &NetworkManager::peerDisconnected, networkEventHandler, &NetworkEventHandler::handlePeerDisconnected);
    connect(networkManager, &NetworkManager::newMessageReceived, networkEventHandler, &NetworkEventHandler::handleNewMessageReceived);
    connect(networkManager, &NetworkManager::peerNetworkError, networkEventHandler, &NetworkEventHandler::handlePeerNetworkError);

    // serverStatusMessage still connects to MainWindow's updateNetworkStatus
    connect(networkManager, &NetworkManager::serverStatusMessage, this, &MainWindow::updateNetworkStatus);
    // incomingConnectionRequest (renamed to incomingSessionRequest in NetworkManager) still connects to MainWindow's slot
    // 注意：NetworkManager 中的信号已重命名为 incomingSessionRequest
    connect(networkManager, &NetworkManager::incomingSessionRequest, this, &MainWindow::handleIncomingConnectionRequest);

    // 修改点：将启动监听的动作移到重连尝试之后

    // 1. 首先尝试加载联系人并重连 (此时不监听)
    loadContactsAndAttemptReconnection();

    // 2. 重连阶段结束后，再根据设置决定是否开始监听端口
    if (autoNetworkListeningEnabled)
    { // 检查用户设置
        networkManager->startListening();
    }
    else
    {
        updateNetworkStatus(tr("Network listening is disabled in settings."));
    }
}

MainWindow::~MainWindow()
{
    // Explicitly stop network manager listening before Qt starts destroying child objects.
    // This can help ensure network resources are released more gracefully.
    if (networkManager)
    {
        qDebug() << "MainWindow::~MainWindow(): Explicitly stopping NetworkManager listening.";
        networkManager->stopListening();
    }

    // settingsDialog 如果被设置了父对象，会被 Qt 自动管理内存
    // formattingHandler is a child of MainWindow, so it will be deleted automatically.
    // peerInfoDisplayWidget is a child of MainWindow, so it will be deleted automatically.
    // networkEventHandler is a child of MainWindow, so it will be deleted automatically.
    // chatHistoryManager is a child of MainWindow, so it will be deleted automatically.

    // 确保在程序退出前，当前打开的聊天记录被保存（如果需要）
    // 实际上，每次消息变动时都保存是更稳妥的做法，这里可能不需要额外操作
}

// 新增：实现 eventFilter 方法
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == messageInputEdit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        // 检查是否是 Enter 键 (Qt::Key_Return 或 Qt::Key_Enter)
        // 并且 Ctrl 修饰键被按下
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            (keyEvent->modifiers() & Qt::ControlModifier))
        {
            onSendButtonClicked(); // 调用发送按钮的槽函数
            return true;           // 事件已处理，不再进一步传递
        }
    }
    // 对于其他对象或其他事件，传递给基类处理
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::loadOrCreateUserIdentity()
{
    QSettings settings;
    // Log the settings file path being used by this instance
    qDebug() << "MW::loadOrCreateUserIdentity: Instance using settings file:" << settings.fileName();
    qDebug() << "MW::loadOrCreateUserIdentity: Application Name for QSettings:" << QCoreApplication::applicationName();

    localUserUuid = settings.value("User/LocalUUID").toString();
    if (localUserUuid.isEmpty())
    {
        localUserUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("User/LocalUUID", localUserUuid);
        qDebug() << "MW::loadOrCreateUserIdentity: Created new UUID:" << localUserUuid;
    }
    else
    {
        qDebug() << "MW::loadOrCreateUserIdentity: Loaded existing UUID:" << localUserUuid;
    }
    // 如果之前没有保存用户名，则使用默认的 "Me"，否则加载已保存的
    localUserName = settings.value("User/LocalUserName", tr("Me")).toString();
    if (localUserName.isEmpty())
    { // 确保用户名不为空
        localUserName = tr("Me");
        settings.setValue("User/LocalUserName", localUserName);
    }
    // 加载端口设置
    localListenPort = settings.value("User/ListenPort", 60248).toUInt();
    autoNetworkListeningEnabled = settings.value("User/AutoNetworkListeningEnabled", true).toBool(); // 新增加载
    localOutgoingPort = settings.value("User/OutgoingPort", 0).toUInt();
    useSpecificOutgoingPort = settings.value("User/UseSpecificOutgoingPort", false).toBool();
    qInfo() << "MW::loadOrCreateUserIdentity: Loaded settings:"
            << "UserName:" << localUserName
            << "UUID:" << localUserUuid
            << "ListenPort:" << localListenPort
            << "AutoNetworkListeningEnabled:" << autoNetworkListeningEnabled
            << "OutgoingPort:" << localOutgoingPort
            << "UseSpecificOutgoingPort:" << useSpecificOutgoingPort;
    // 更新NetworkManager的设置，以防它们在settingsDialog之外被更改（例如，首次运行）
    if (networkManager)
    {
        networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled); // 传递启用状态
        networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);
    }
    qInfo() << "MW::loadOrCreateUserIdentity: Loaded user identity completed:" << localUserName << localUserUuid;
}

QString MainWindow::getLocalUserName() const
{
    return localUserName;
}

QString MainWindow::getLocalUserUuid() const
{
    return localUserUuid;
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
    defaultFormat.setBackground(currentBgColor);   // Use MainWindow's currentBgColor
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
        settingsDialog = new SettingsDialog(localUserName,
                                            localUserUuid,
                                            localListenPort,
                                            autoNetworkListeningEnabled,
                                            localOutgoingPort, useSpecificOutgoingPort,
                                            this);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::handleSettingsApplied);
        connect(settingsDialog, &SettingsDialog::retryListenNowRequested, this, &MainWindow::handleRetryListenNow); // 新增连接
    }
    else
    {
        // Update dialog with current settings if it already exists
        settingsDialog->updateFields(localUserName, localUserUuid, localListenPort, autoNetworkListeningEnabled, localOutgoingPort, useSpecificOutgoingPort);
    }
    settingsDialog->exec(); // 以模态方式显示对话框
}

void MainWindow::handleSettingsApplied(const QString &userName,
                                       quint16 listenPort,
                                       bool enableListening,
                                       quint16 outgoingPort, bool useSpecificOutgoingPortVal)
{
    bool settingsChanged = false;
    QSettings settings;
    bool listeningPrefsChanged = false; // 重命名以更清晰

    if (localUserName != userName)
    {
        localUserName = userName;
        settings.setValue("User/LocalUserName", localUserName);
        if (networkManager)
        {
            networkManager->setLocalUserDetails(localUserUuid, localUserName);
        }
        settingsChanged = true;
    }

    if (localListenPort != listenPort)
    {
        localListenPort = listenPort;
        settings.setValue("User/ListenPort", localListenPort);
        settingsChanged = true;
        listeningPrefsChanged = true;
    }
    // 检查监听启用状态是否改变
    if (autoNetworkListeningEnabled != enableListening)
    {
        autoNetworkListeningEnabled = enableListening;
        settings.setValue("User/AutoNetworkListeningEnabled", autoNetworkListeningEnabled);
        settingsChanged = true;
        listeningPrefsChanged = true;
    }

    if (listeningPrefsChanged)
    {
        networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled);
        if (autoNetworkListeningEnabled)
        {
            updateNetworkStatus(tr("Listener settings changed. Attempting to start listener..."));
            networkManager->startListening(); // 明确尝试启动监听
        }
        else
        {
            updateNetworkStatus(tr("Network listening disabled. Stopping listener..."));
            networkManager->stopListening(); // 明确停止监听
        }
    }

    if (localOutgoingPort != outgoingPort || useSpecificOutgoingPort != useSpecificOutgoingPortVal)
    {
        localOutgoingPort = outgoingPort;
        useSpecificOutgoingPort = useSpecificOutgoingPortVal;
        settings.setValue("User/OutgoingPort", localOutgoingPort);                  // 保存传出端口
        settings.setValue("User/UseSpecificOutgoingPort", useSpecificOutgoingPort); // 保存选项
        networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);
        settingsChanged = true;
        updateNetworkStatus(tr("Outgoing port preference updated. Will apply to new connections."));
    }

    if (settingsChanged)
    {
        updateNetworkStatus(tr("Settings applied. User: %1, Listening: %2 (Port: %3), Outgoing Port: %4")
                                .arg(localUserName)
                                .arg(autoNetworkListeningEnabled ? tr("Enabled") : tr("Disabled")) // 显示监听状态
                                .arg(QString::number(localListenPort))
                                .arg(useSpecificOutgoingPort && localOutgoingPort > 0 ? QString::number(localOutgoingPort) : tr("Dynamic")));
    }
    else if (!listeningPrefsChanged)
    { // 避免在仅重启网络时显示 "unchanged"
        updateNetworkStatus(tr("Settings unchanged."));
    }
}

void MainWindow::handleRetryListenNow() // 新增槽函数实现
{
    if (networkManager)
    {
        if (autoNetworkListeningEnabled)
        {
            updateNetworkStatus(tr("Attempting to listen on port %1 now...").arg(localListenPort));
            networkManager->startListening();
        }
        else
        {
            updateNetworkStatus(tr("Cannot attempt to listen: Network listening is disabled in settings."));
            QMessageBox::information(this, tr("Listening Disabled"), tr("Network listening is currently disabled in settings. Please enable it first."));
        }
    }
}

void MainWindow::handleContactAdded(const QString &name, const QString &uuid, const QString &ip, quint16 port)
{
    if (name.isEmpty() || uuid.isEmpty())
    {
        qWarning() << "Attempted to add contact with empty name or UUID:" << name << uuid;
        return;
    }

    QListWidgetItem *itemToSelect = nullptr;
    // Check contactListWidget for existing UUID first
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *existingItem = contactListWidget->item(i);
        if (existingItem->data(Qt::UserRole).toString() == uuid)
        {
            itemToSelect = existingItem;
            // UUID exists. Update name if different.
            if (existingItem->text() != name)
            {
                existingItem->setText(name); // Update display name
            }
            // 更新存储的IP和端口
            existingItem->setData(Qt::UserRole + 1, ip);
            existingItem->setData(Qt::UserRole + 2, port);
            break;
        }
    }

    if (!itemToSelect)
    {
        itemToSelect = new QListWidgetItem(name, contactListWidget);
        itemToSelect->setData(Qt::UserRole, uuid);     // Store UUID in item's data
        itemToSelect->setData(Qt::UserRole + 1, ip);   // Store IP
        itemToSelect->setData(Qt::UserRole + 2, port); // Store Port
        // 初始设置为离线图标，连接成功后 NetworkEventHandler 会更新
        itemToSelect->setIcon(QIcon(":/icons/offline.svg")); // CHANGED .png to .svg
    }

    saveContacts(); // 保存联系人列表
}

void MainWindow::onContactSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current)
    {
        currentOpenChatContactName = current->text(); // 这仍然可以用于UI显示，但UUID是关键
        QString peerUuid = current->data(Qt::UserRole).toString();

        if (peerUuid.isEmpty())
        {
            qWarning() << "Selected contact" << currentOpenChatContactName << "has no UUID.";
            if (peerInfoDisplayWidget)
                peerInfoDisplayWidget->clearDisplay();
            messageDisplay->clear();
            messageInputEdit->clear();
            messageInputEdit->setEnabled(false);
            if (chatStackedWidget->currentWidget() != emptyChatPlaceholderLabel)
            {
                chatStackedWidget->setCurrentWidget(emptyChatPlaceholderLabel);
            }
            return;
        }

        // 更新 PeerInfoWidget
        if (networkManager && peerInfoDisplayWidget)
        {
            QAbstractSocket::SocketState state = networkManager->getPeerSocketState(peerUuid);
            if (state == QAbstractSocket::ConnectedState)
            {
                QPair<QString, quint16> netInfo = networkManager->getPeerInfo(peerUuid);
                QString ipAddr = networkManager->getPeerIpAddress(peerUuid);
                peerInfoDisplayWidget->updateDisplay(netInfo.first, peerUuid, ipAddr, netInfo.second);
                // 如果连接成功，并且 IP/端口与存储的不同，则更新并保存
                if (current->data(Qt::UserRole + 1).toString() != ipAddr ||
                    current->data(Qt::UserRole + 2).toUInt() != netInfo.second)
                {
                    current->setData(Qt::UserRole + 1, ipAddr);
                    current->setData(Qt::UserRole + 2, netInfo.second);
                    saveContacts();
                }
                messageInputEdit->setEnabled(true);
            }
            else
            {
                // 对等方可能在联系人列表中，但当前未连接
                peerInfoDisplayWidget->updateDisplay(currentOpenChatContactName, peerUuid, tr("Not Connected"), 0);
                messageInputEdit->setEnabled(false); // 如果未连接，则禁用输入
            }
        }

        QStringList messagesToDisplay;
        // 检查内存中是否已有此会话的聊天记录 (即本会话期间是否已加载过)
        if (chatHistories.contains(peerUuid))
        {
            messagesToDisplay = chatHistories.value(peerUuid);
            qDebug() << "onContactSelected: Using in-memory history for" << peerUuid << "Count:" << messagesToDisplay.count();
        }
        else
        {
            // 如果内存中没有，则从文件加载
            if (chatHistoryManager)
            {
                messagesToDisplay = chatHistoryManager->loadChatHistory(peerUuid);
                chatHistories[peerUuid] = messagesToDisplay; // 将加载的记录（或空列表）存入内存
                qDebug() << "onContactSelected: Loaded history using ChatHistoryManager for" << peerUuid << "Count:" << messagesToDisplay.count();
            }
            else
            {
                qWarning() << "onContactSelected: ChatHistoryManager is null. Cannot load history for" << peerUuid;
                chatHistories[peerUuid] = QStringList(); // 如果管理器为空，则为此UUID在内存中设置一个空列表
            }
        }

        messageDisplay->setMessages(messagesToDisplay);
        current->setBackground(QBrush()); // 清除未读消息的背景

        messageInputEdit->clear();
        messageInputEdit->setFocus();
        if (chatStackedWidget->currentWidget() != activeChatContentsWidget)
        {
            chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
        }
        // 根据连接状态启用/禁用输入框
        if (networkManager && networkManager->getPeerSocketState(peerUuid) == QAbstractSocket::ConnectedState)
        {
            messageInputEdit->setEnabled(true);
        }
        else
        {
            messageInputEdit->setEnabled(false);
        }
    }
    else
    {
        currentOpenChatContactName.clear();
        if (peerInfoDisplayWidget)
            peerInfoDisplayWidget->clearDisplay();
        messageDisplay->clear();
        messageInputEdit->clear();
        messageInputEdit->setEnabled(false);
        if (chatStackedWidget->currentWidget() != emptyChatPlaceholderLabel)
        {
            chatStackedWidget->setCurrentWidget(emptyChatPlaceholderLabel);
        }
    }
}

void MainWindow::onSendButtonClicked()
{
    QListWidgetItem *currentItem = contactListWidget->currentItem();
    if (!currentItem)
    {
        updateNetworkStatus(tr("No active chat selected."));
        return;
    }

    QString targetPeerUuid = currentItem->data(Qt::UserRole).toString();
    if (targetPeerUuid.isEmpty())
    {
        updateNetworkStatus(tr("Selected contact has no UUID. Cannot send message."));
        QMessageBox::warning(this, tr("Error"), tr("Selected contact has no UUID."));
        return;
    }

    if (!networkManager || networkManager->getPeerSocketState(targetPeerUuid) != QAbstractSocket::ConnectedState)
    {
        updateNetworkStatus(tr("Not connected to %1. Cannot send message.").arg(currentItem->text()));
        QMessageBox::warning(this, tr("Network Error"), tr("Not connected to %1. Please ensure they are online and connected.").arg(currentItem->text()));
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

        QString activeContactUuid = targetPeerUuid; // 使用从currentItem获取的UUID

        if (!activeContactUuid.isEmpty())
        {
            chatHistories[activeContactUuid].append(userMessageHtml); // Use UUID for history key
            saveChatHistory(activeContactUuid);                       // 调用 MainWindow::saveChatHistory
        }
        else
        {
            qWarning() << "Sending message: Active contact" << currentOpenChatContactName << "has no UUID. Using name as fallback for history.";
            chatHistories[currentOpenChatContactName].append(userMessageHtml);
            // 注意：如果使用name作为fallback，保存时也应该用name，但这通常不推荐
            // chatHistoryManager->saveChatHistory(currentOpenChatContactName, chatHistories.value(currentOpenChatContactName));
        }

        messageDisplay->addMessage(userMessageHtml);

        networkManager->sendMessage(activeContactUuid, coreContent); // 使用 targetPeerUuid 发送

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
        defaultFormat.setBackground(currentBgColor);   // Use MainWindow's currentBgColor
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

void MainWindow::updateNetworkStatus(const QString &status)
{
    if (networkStatusLabel)
    {
        networkStatusLabel->setText(status);
    }
    else
    {
        if (statusBar())
        { // Ensure statusBar is valid
            statusBar()->showMessage(status, 5000);
        }
        else
        {
            qWarning() << "statusBar is null, cannot show status message:" << status;
        }
    }
}

void MainWindow::handleIncomingConnectionRequest(QTcpSocket *tempSocket, const QString &peerAddress, quint16 peerPort, const QString &peerUuid, const QString &peerNameHint)
{
    qDebug() << "MW::handleIncomingConnectionRequest: From" << peerAddress << ":" << peerPort << "PeerUUID:" << peerUuid << "NameHint:" << peerNameHint;
    QMessageBox::StandardButton reply;
    QString suggestedName = peerNameHint.isEmpty() ? peerAddress : peerNameHint;

    // 查找现有联系人
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *item = contactListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == peerUuid)
        {
            QMessageBox::StandardButton reconReply = QMessageBox::question(this, tr("Existing Contact"),
                                                                           tr("Contact '%1' (UUID: %2) is trying to connect from %3:%4.\nUpdate IP/Port and connect?")
                                                                               .arg(item->text())
                                                                               .arg(peerUuid)
                                                                               .arg(peerAddress)
                                                                               .arg(peerPort),
                                                                           QMessageBox::Yes | QMessageBox::No);
            if (reconReply == QMessageBox::Yes)
            {
                // 更新存储的IP和端口信息
                item->setData(Qt::UserRole + 1, peerAddress);
                item->setData(Qt::UserRole + 2, peerPort);
                saveContacts(); // 保存更新后的信息
                networkManager->acceptIncomingSession(tempSocket, peerUuid, item->text());
            }
            else
            {
                networkManager->rejectIncomingSession(tempSocket);
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
            networkManager->acceptIncomingSession(tempSocket, peerUuid, contactName);
        }
        else if (ok && contactName.isEmpty())
        {
            networkManager->acceptIncomingSession(tempSocket, peerUuid, suggestedName.isEmpty() ? peerAddress : suggestedName);
        }
        else
        {
            networkManager->rejectIncomingSession(tempSocket);
            updateNetworkStatus(tr("Incoming connection naming cancelled. Rejected."));
        }
    }
    else
    {
        networkManager->rejectIncomingSession(tempSocket);
    }
}

void MainWindow::saveContacts()
{
    QSettings settings;
    settings.beginWriteArray("Contacts");
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *item = contactListWidget->item(i);
        settings.setArrayIndex(i);
        settings.setValue("uuid", item->data(Qt::UserRole).toString());
        settings.setValue("name", item->text());
        settings.setValue("ip", item->data(Qt::UserRole + 1).toString());
        settings.setValue("port", item->data(Qt::UserRole + 2).toUInt());
    }
    settings.endArray();
    settings.sync(); // 确保立即写入
    updateNetworkStatus(tr("Contacts saved."));
}

void MainWindow::loadContactsAndAttemptReconnection()
{
    QSettings settings;
    int size = settings.beginReadArray("Contacts");
    for (int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);
        QString uuid = settings.value("uuid").toString();
        QString name = settings.value("name").toString();
        QString ip = settings.value("ip").toString();

        if (uuid.isEmpty() || name.isEmpty())
            continue;

        bool found = false;
        for (int j = 0; j < contactListWidget->count(); ++j)
        {
            if (contactListWidget->item(j)->data(Qt::UserRole).toString() == uuid)
            {
                contactListWidget->item(j)->setText(name); // 更新名称
                contactListWidget->item(j)->setData(Qt::UserRole + 1, ip);
                contactListWidget->item(j)->setIcon(QIcon(":/icons/offline.svg")); // CHANGED .png to .svg
                found = true;
                break;
            }
        }
        if (!found)
        {
            QListWidgetItem *item = new QListWidgetItem(name, contactListWidget);
            item->setData(Qt::UserRole, uuid);
            item->setData(Qt::UserRole + 1, ip);
            item->setIcon(QIcon(":/icons/offline.svg")); // CHANGED .png to .svg // 初始设置为离线
        }

        // 尝试重连
        if (networkManager && !ip.isEmpty())
        {
            quint16 targetListenPort = localListenPort; // 使用本应用的监听端口作为尝试连接对方的端口
            updateNetworkStatus(tr("Attempting to reconnect to %1 (%2) at %3:%4...")
                                    .arg(name)
                                    .arg(uuid)
                                    .arg(ip)
                                    .arg(targetListenPort));
            networkManager->connectToHost(name, uuid, ip, targetListenPort);
        }
    }
    settings.endArray();
    if (size > 0)
    {
        updateNetworkStatus(tr("Loaded %1 contacts. Attempting reconnections...").arg(size));
    }
    else
    {
        updateNetworkStatus(tr("No saved contacts found."));
    }
}

void MainWindow::saveChatHistory(const QString &peerUuid)
{
    if (chatHistoryManager && chatHistories.contains(peerUuid))
    {
        if (!chatHistoryManager->saveChatHistory(peerUuid, chatHistories.value(peerUuid)))
        {
            qWarning() << "MainWindow: Failed to save chat history via ChatHistoryManager for peer" << peerUuid;
            // 可以选择在这里通过 updateNetworkStatus 更新UI状态
        }
        else
        {
            qInfo() << "MainWindow: Chat history saved via ChatHistoryManager for peer" << peerUuid;
        }
    }
    else
    {
        qWarning() << "MainWindow::saveChatHistory: ChatHistoryManager is null or no history in memory for peer" << peerUuid;
    }
}
