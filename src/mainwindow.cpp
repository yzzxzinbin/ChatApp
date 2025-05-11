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
      udpDiscoveryEnabled(true),         // 新增：默认启用UDP发现
      localUdpDiscoveryPort(60249),      // Default UDP discovery port
      udpContinuousBroadcastEnabled(true), // Added: 默认启用持续广播
      udpBroadcastIntervalSeconds(5),    // Added: 默认5秒间隔
      localOutgoingPort(0),              // 默认传出端口为0 (动态)
      useSpecificOutgoingPort(false)     // 默认不指定传出端口
{
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    loadOrCreateUserIdentity(); // 这会加载设置，包括 udpDiscoveryEnabled

    chatHistoryManager = new ChatHistoryManager(QCoreApplication::applicationName(), this);

    // 1. 首先初始化 NetworkManager
    networkManager = new NetworkManager(this);
    networkManager->setLocalUserDetails(localUserUuid, localUserName);
    networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled);
    networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);
    networkManager->setUdpDiscoveryPreferences(udpDiscoveryEnabled, localUdpDiscoveryPort, udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds); // Updated: pass all parameters

    // 2. 然后初始化 ContactManager
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

    // 1. 首先尝试加载联系人并重连 (此时不监听TCP，但UDP发现可以启动)
    loadContactsAndAttemptReconnection(); // UDP发现如果启用，NetworkManager内部会处理启动

    // 2. 重连阶段结束后，再根据设置决定是否开始监听TCP端口
    if (autoNetworkListeningEnabled)
    {
        networkManager->startListening();
    }
    else
    {
        updateNetworkStatus(tr("Network listening is disabled in settings."));
    }
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow::~MainWindow() - Starting destruction.";
    // Explicitly stop network manager listening and discovery.
    // This should ensure sockets are closed and potentially scheduled for deletion
    // while the main event loop might still be able to process some events.
    if (networkManager)
    {
        qDebug() << "MainWindow::~MainWindow(): Calling networkManager->stopListening().";
        networkManager->stopListening(); // This closes server, aborts client sockets, and calls deleteLater on them.
        qDebug() << "MainWindow::~MainWindow(): Calling networkManager->stopUdpDiscovery().";
        networkManager->stopUdpDiscovery(); // This closes UDP socket.

        // Disconnect all signals from NetworkManager to prevent them from being handled
        // by MainWindow or NetworkEventHandler slots if NetworkManager emits something
        // during its own subsequent destruction by Qt's parent-child mechanism.
        // This is a defensive measure.
        qDebug() << "MainWindow::~MainWindow(): Disconnecting all signals from networkManager.";
        disconnect(networkManager, nullptr, nullptr, nullptr);
    }

    // contactManager, chatHistoryManager, formattingHandler, networkEventHandler
    // are children of MainWindow and will be deleted by Qt when MainWindow is destructed.
    // Their destructors should be well-behaved.
    // UI elements (contactListWidget, messageDisplay, etc.) are also children and will be deleted by Qt.

    qDebug() << "MainWindow::~MainWindow() - Destruction finished. Qt will now delete child objects (like NetworkManager, UI elements, etc.).";
    // MainWindow's QObject destructor will then delete child objects like networkManager,
    // contactManager, chatHistoryManager, networkEventHandler, formattingHandler,
    // and all UI widgets that were parented to MainWindow or its child widgets.
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
    autoNetworkListeningEnabled = settings.value("User/AutoNetworkListeningEnabled", true).toBool();
    udpDiscoveryEnabled = settings.value("User/UdpDiscoveryEnabled", true).toBool(); // 加载UDP发现设置
    localUdpDiscoveryPort = settings.value("User/UdpDiscoveryPort", 60249).toUInt(); // Load UDP port
    udpContinuousBroadcastEnabled = settings.value("User/UdpContinuousBroadcastEnabled", true).toBool(); // Added: 加载持续广播设置
    udpBroadcastIntervalSeconds = settings.value("User/UdpBroadcastIntervalSeconds", 5).toInt(); // Added: 加载广播间隔
    if (udpBroadcastIntervalSeconds <= 0) udpBroadcastIntervalSeconds = 5; // 确保值为正数
    localOutgoingPort = settings.value("User/OutgoingPort", 0).toUInt();
    useSpecificOutgoingPort = settings.value("User/UseSpecificOutgoingPort", false).toBool();
    
    qInfo() << "MW::loadOrCreateUserIdentity: Loaded settings:"
            << "UserName:" << localUserName
            << "UUID:" << localUserUuid
            << "ListenPort:" << localListenPort
            << "AutoNetworkListeningEnabled:" << autoNetworkListeningEnabled
            << "UdpDiscoveryEnabled:" << udpDiscoveryEnabled 
            << "UdpDiscoveryPort:" << localUdpDiscoveryPort
            << "UdpContinuousBroadcastEnabled:" << udpContinuousBroadcastEnabled // Added
            << "UdpBroadcastIntervalSeconds:" << udpBroadcastIntervalSeconds // Added
            << "OutgoingPort:" << localOutgoingPort
            << "UseSpecificOutgoingPort:" << useSpecificOutgoingPort;
    
    // 更新NetworkManager的设置，以防它们在settingsDialog之外被更改（例如，首次运行）
    if (networkManager)
    {
        networkManager->setLocalUserDetails(localUserUuid, localUserName); // Moved this line here
        networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled);
        networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);
        networkManager->setUdpDiscoveryPreferences(udpDiscoveryEnabled, localUdpDiscoveryPort, udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds); // Updated: pass all parameters
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

// Add the implementation for the new getter method
quint16 MainWindow::getLocalListenPort() const
{
    return localListenPort;
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
                                            udpDiscoveryEnabled, 
                                            localUdpDiscoveryPort,
                                            udpContinuousBroadcastEnabled, // Added: pass continuous broadcast setting
                                            udpBroadcastIntervalSeconds, // Added: pass broadcast interval
                                            this);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::handleSettingsApplied);
        connect(settingsDialog, &SettingsDialog::retryListenNowRequested, this, &MainWindow::handleRetryListenNow); // 新增连接
        connect(settingsDialog, &SettingsDialog::manualUdpBroadcastRequested, this, &MainWindow::handleManualUdpBroadcastRequested); // 新增连接
    }
    else
    {
        // Update dialog with current settings if it already exists
        settingsDialog->updateFields(localUserName, localUserUuid, localListenPort, autoNetworkListeningEnabled, 
                                    localOutgoingPort, useSpecificOutgoingPort, 
                                    udpDiscoveryEnabled, localUdpDiscoveryPort,
                                    udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds); // Updated: pass all parameters
    }
    settingsDialog->exec(); // 以模态方式显示对话框
}

void MainWindow::handleSettingsApplied(const QString &userName,
                                       quint16 listenPort,
                                       bool enableListening,
                                       quint16 outgoingPort, bool useSpecificOutgoingPortVal,
                                       bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                                       bool enableContinuousUdpBroadcast, int udpBroadcastInterval) // Updated: receive new parameters
{
    bool settingsChanged = false;
    QSettings settings;
    bool listeningPrefsChanged = false;
    bool udpDiscoveryPrefsChanged = false;

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

    // 检查UDP发现启用状态、端口、持续广播或间隔是否改变
    if (udpDiscoveryEnabled != enableUdpDiscovery || 
        localUdpDiscoveryPort != udpDiscoveryPort || 
        udpContinuousBroadcastEnabled != enableContinuousUdpBroadcast || // Added: check continuous setting change
        udpBroadcastIntervalSeconds != udpBroadcastInterval) // Added: check interval change
    {
        udpDiscoveryEnabled = enableUdpDiscovery;
        localUdpDiscoveryPort = udpDiscoveryPort; // Store new port
        udpContinuousBroadcastEnabled = enableContinuousUdpBroadcast; // Added: store new continuous setting
        udpBroadcastIntervalSeconds = udpBroadcastInterval > 0 ? udpBroadcastInterval : 5; // Added: store new interval, ensure positive
        
        settings.setValue("User/UdpDiscoveryEnabled", udpDiscoveryEnabled);
        settings.setValue("User/UdpDiscoveryPort", localUdpDiscoveryPort); // Save port
        settings.setValue("User/UdpContinuousBroadcastEnabled", udpContinuousBroadcastEnabled); // Added: save continuous setting
        settings.setValue("User/UdpBroadcastIntervalSeconds", udpBroadcastIntervalSeconds); // Added: save interval
        
        settingsChanged = true;
        udpDiscoveryPrefsChanged = true;
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

    // 处理UDP发现设置更改
    if (udpDiscoveryPrefsChanged)
    {
        if (networkManager)
        {
            networkManager->setUdpDiscoveryPreferences(udpDiscoveryEnabled, localUdpDiscoveryPort, 
                                                       udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds); // Updated: pass all parameters
        }
        
        // 更新状态信息，包含持续广播和间隔信息
        if (udpDiscoveryEnabled)
        {
            QString status = tr("UDP Discovery enabled on port %1.").arg(localUdpDiscoveryPort);
            if (udpContinuousBroadcastEnabled) {
                status += tr(" Continuous broadcast every %1 seconds.").arg(udpBroadcastIntervalSeconds);
            } else {
                status += tr(" Continuous broadcast disabled.");
            }
            updateNetworkStatus(status);
        }
        else
        {
            updateNetworkStatus(tr("UDP Discovery disabled."));
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
        updateNetworkStatus(tr("Settings applied. User: %1, Listening: %2 (Port: %3), UDP Discovery: %4 (Port: %5, Continuous: %6, Interval: %7s), Outgoing Port: %8")
                                .arg(localUserName)
                                .arg(autoNetworkListeningEnabled ? tr("Enabled") : tr("Disabled")) 
                                .arg(QString::number(localListenPort))
                                .arg(udpDiscoveryEnabled ? tr("Enabled") : tr("Disabled"))        
                                .arg(QString::number(localUdpDiscoveryPort))
                                .arg(udpContinuousBroadcastEnabled ? tr("Yes") : tr("No")) // Added: show continuous status
                                .arg(QString::number(udpBroadcastIntervalSeconds)) // Added: show interval
                                .arg(useSpecificOutgoingPort && localOutgoingPort > 0 ? QString::number(localOutgoingPort) : tr("Dynamic")));
    }
    else if (!listeningPrefsChanged && !udpDiscoveryPrefsChanged) // Also check UDP prefs
    { 
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

void MainWindow::handleManualUdpBroadcastRequested() // 新增实现
{
    if (networkManager)
    {
        networkManager->triggerManualUdpBroadcast();
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

    if (!networkManager)
    { // Guard against null networkManager
        qWarning() << "MW::handleIncomingConnectionRequest: networkManager is null, cannot process request.";
        if (tempSocket)
            tempSocket->abort(); // Abort the socket if we can't handle it
        return;
    }

    // 检查是否是已知联系人
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *item = contactListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == peerUuid)
        {
            // 已知联系人，自动接受并更新信息
            QString knownName = item->text();
            updateNetworkStatus(tr("Auto-reconnecting with known contact '%1' (UUID: %2) from %3:%4.")
                                    .arg(knownName)
                                    .arg(peerUuid)
                                    .arg(peerAddress)
                                    .arg(peerPort));

            // 更新存储的IP和端口信息 (如果变化)
            bool infoChanged = false;
            if (item->data(Qt::UserRole + 1).toString() != peerAddress)
            {
                item->setData(Qt::UserRole + 1, peerAddress);
                infoChanged = true;
            }
            if (item->data(Qt::UserRole + 2).toUInt() != peerPort)
            {
                item->setData(Qt::UserRole + 2, peerPort);
                infoChanged = true;
            }
            if (infoChanged)
            {
                saveContacts(); // 保存更新后的信息
            }

            networkManager->acceptIncomingSession(tempSocket, peerUuid, knownName);
            return; // 处理完毕，不再询问用户
        }
    }

    // 未知联系人，按原有逻辑询问用户
    QMessageBox::StandardButton reply;
    QString suggestedName = peerNameHint.isEmpty() ? peerAddress : peerNameHint;

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
        quint16 savedContactPort = settings.value("port").toUInt(); // 这是联系人保存的端口

        if (uuid.isEmpty() || name.isEmpty())
            continue;

        bool found = false;
        for (int j = 0; j < contactListWidget->count(); ++j)
        {
            if (contactListWidget->item(j)->data(Qt::UserRole).toString() == uuid)
            {
                contactListWidget->item(j)->setText(name); // 更新名称
                contactListWidget->item(j)->setData(Qt::UserRole + 1, ip);
                contactListWidget->item(j)->setData(Qt::UserRole + 2, savedContactPort); // 更新端口
                contactListWidget->item(j)->setIcon(QIcon(":/icons/offline.svg"));
                found = true;
                break;
            }
        }
        if (!found)
        {
            QListWidgetItem *item = new QListWidgetItem(name, contactListWidget);
            item->setData(Qt::UserRole, uuid);
            item->setData(Qt::UserRole + 1, ip);
            item->setData(Qt::UserRole + 2, savedContactPort); // 保存端口
            item->setIcon(QIcon(":/icons/offline.svg"));
        }

        // 尝试重连逻辑
        if (networkManager && !ip.isEmpty())
        {
            bool attemptMadeWithLocalListenPortAsTarget = false;

            // 尝试1: 使用本地监听端口 localListenPort 作为目标端口
            if (localListenPort > 0)
            {
                updateNetworkStatus(tr("Attempting reconnect to %1 (UUID: %2) at %3:%4 (using common port convention)...")
                                        .arg(name).arg(uuid).arg(ip).arg(localListenPort));
                networkManager->connectToHost(name, uuid, ip, localListenPort);
                attemptMadeWithLocalListenPortAsTarget = true;
            }

            // 尝试2: 使用联系人保存的端口 savedContactPort
            // 条件：savedContactPort 有效，并且
            //        (之前未使用 localListenPort 尝试 或 savedContactPort 与 localListenPort 不同)
            if (savedContactPort > 0)
            {
                if (attemptMadeWithLocalListenPortAsTarget && savedContactPort == localListenPort)
                {
                    // 如果 localListenPort 和 savedContactPort 相同，并且已经尝试过，则不再尝试
                }
                else
                {
                    // 如果 localListenPort 未尝试过 (例如 localListenPort 为 0)，或者 savedContactPort 与 localListenPort 不同
                    updateNetworkStatus(tr("Attempting reconnect to %1 (UUID: %2) at %3:%4 (using last known port)...")
                                            .arg(name).arg(uuid).arg(ip).arg(savedContactPort));
                    networkManager->connectToHost(name, uuid, ip, savedContactPort);
                }
            }
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
