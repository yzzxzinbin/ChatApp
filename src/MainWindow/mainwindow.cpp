#include "mainwindow.h"
#include "contactmanager.h"
#include "chatmessagedisplay.h"
#include "networkmanager.h"
#include "settingsdialog.h"
#include "peerinfowidget.h"
#include "mainwindowstyle.h"
#include "formattingtoolbarhandler.h"
#include "networkeventhandler.h"
#include "chathistorymanager.h"

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
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QUuid>
#include <QSettings>
#include <QDebug>
#include <QEvent>
#include <QDateTime>

MainWindow::MainWindow(const QString &currentUserId, QWidget *parent)
    : QMainWindow(parent),
      m_currentUserIdStr(currentUserId),
      networkManager(nullptr),
      settingsDialog(nullptr),
      localUserName(tr("Me")),
      localUserUuid(QString()),
      localListenPort(60248),
      autoNetworkListeningEnabled(true),
      udpDiscoveryEnabled(true),
      localUdpDiscoveryPort(60249),
      udpContinuousBroadcastEnabled(true),
      udpBroadcastIntervalSeconds(5),
      localOutgoingPort(0),
      useSpecificOutgoingPort(false)
{
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    loadCurrentUserIdentity();

    chatHistoryManager = new ChatHistoryManager(QCoreApplication::applicationName() + "/" + m_currentUserIdStr, this);

    networkManager = new NetworkManager(this);
    networkManager->setLocalUserDetails(localUserUuid, localUserName);
    networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled);
    networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);

    contactManager = new ContactManager(networkManager, this);
    connect(contactManager, &ContactManager::contactAdded, this, &MainWindow::handleContactAdded);

    currentTextColor = QColor(Qt::black);
    currentBgColor = QColor(Qt::transparent);

    setupUI();

    networkEventHandler = new NetworkEventHandler(
        networkManager,
        contactListWidget,
        messageDisplay,
        peerInfoDisplayWidget,
        chatStackedWidget,
        messageInputEdit,
        emptyChatPlaceholderLabel,
        activeChatContentsWidget,
        &chatHistories,
        this,
        this);

    setWindowTitle("ChatApp - " + localUserName + "By CCZU_ZX");
    resize(1024, 768);

    connect(networkManager, &NetworkManager::peerConnected, networkEventHandler, &NetworkEventHandler::handlePeerConnected);
    connect(networkManager, &NetworkManager::peerDisconnected, networkEventHandler, &NetworkEventHandler::handlePeerDisconnected);
    connect(networkManager, &NetworkManager::newMessageReceived, networkEventHandler, &NetworkEventHandler::handleNewMessageReceived);
    connect(networkManager, &NetworkManager::peerNetworkError, networkEventHandler, &NetworkEventHandler::handlePeerNetworkError);
    connect(networkManager, &NetworkManager::serverStatusMessage, this, &MainWindow::updateNetworkStatus);
    connect(networkManager, &NetworkManager::incomingSessionRequest, this, &MainWindow::handleIncomingConnectionRequest);

    loadCurrentUserContacts();

    if (autoNetworkListeningEnabled)
    {
        networkManager->startListening(); // TCP服务器在此处尝试启动
    }
    else
    {
        updateNetworkStatus(tr("Network listening is disabled in settings."));
    }

    // 然后设置并启动UDP发现。此时 tcpServer->isListening() 的状态是正确的。
    // setUdpDiscoveryPreferences 内部会在启用时调用 startUdpDiscovery -> sendUdpBroadcast
    networkManager->setUdpDiscoveryPreferences(udpDiscoveryEnabled, localUdpDiscoveryPort, udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds);
}

MainWindow::~MainWindow()
{
    saveCurrentUserContacts();

    QSettings settings;
    if (!m_currentUserIdStr.isEmpty()) {
        settings.remove(QString("ActiveSessions/%1").arg(m_currentUserIdStr));
        settings.sync();
        qInfo() << "Cleared active session flag for user:" << m_currentUserIdStr;
    }

    if (networkManager)
    {
        networkManager->stopListening();
        networkManager->stopUdpDiscovery();
        disconnect(networkManager, nullptr, nullptr, nullptr);
    }

    qDebug() << "MainWindow::~MainWindow() - Destruction finished.";
}

void MainWindow::loadCurrentUserIdentity()
{
    if (m_currentUserIdStr.isEmpty())
    {
        qCritical() << "Cannot load user identity: Current User ID is empty.";
        localUserUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        localUserName = "Guest";
        return;
    }

    QSettings settings;
    QString profileGroup = "UserAccounts/" + m_currentUserIdStr + "/Profile";
    settings.beginGroup(profileGroup);
    localUserUuid = settings.value("uuid").toString();
    localUserName = settings.value("localUserName", m_currentUserIdStr).toString();
    if (localUserUuid.isEmpty())
    {
        qWarning() << "UUID not found in settings for user" << m_currentUserIdStr << ". Generating a new one.";
        localUserUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("uuid", localUserUuid);
    }
    settings.endGroup();

    QString settingsGroup = "UserAccounts/" + m_currentUserIdStr + "/Settings";
    settings.beginGroup(settingsGroup);
    localListenPort = settings.value("ListenPort", 60248).toUInt();
    autoNetworkListeningEnabled = settings.value("AutoNetworkListeningEnabled", true).toBool();
    udpDiscoveryEnabled = settings.value("UdpDiscoveryEnabled", true).toBool();
    localUdpDiscoveryPort = settings.value("UdpDiscoveryPort", 60249).toUInt();
    udpContinuousBroadcastEnabled = settings.value("UdpContinuousBroadcastEnabled", true).toBool();
    udpBroadcastIntervalSeconds = settings.value("UdpBroadcastIntervalSeconds", 5).toInt();
    localOutgoingPort = settings.value("OutgoingPort", 0).toUInt();
    useSpecificOutgoingPort = settings.value("UseSpecificOutgoingPort", false).toBool();
    settings.endGroup();

    qInfo() << "Loaded identity for User ID:" << m_currentUserIdStr << "- UUID:" << localUserUuid << ", Name:" << localUserName;
}

void MainWindow::saveCurrentUserContacts()
{
    if (m_currentUserIdStr.isEmpty())
        return;
    QSettings settings;
    settings.beginGroup("UserAccounts/" + m_currentUserIdStr);
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
    settings.endGroup();
    settings.sync();
}

void MainWindow::loadCurrentUserContacts()
{
    if (m_currentUserIdStr.isEmpty())
        return;
    contactListWidget->clear();
    QSettings settings;
    settings.beginGroup("UserAccounts/" + m_currentUserIdStr);
    int size = settings.beginReadArray("Contacts");
    for (int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);
        QString uuid = settings.value("uuid").toString();
        QString name = settings.value("name").toString();
        QString ip = settings.value("ip").toString();
        quint16 port = settings.value("port").toUInt();
        if (!uuid.isEmpty() && !name.isEmpty())
        {
            handleContactAdded(name, uuid, ip, port);
        }
    }
    settings.endArray();
    settings.endGroup();
}

void MainWindow::handleSettingsApplied(const QString &userName,
                                       quint16 listenPort,
                                       bool enableListening,
                                       quint16 outgoingPort, bool useSpecificOutgoingPortVal,
                                       bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                                       bool enableContinuousUdpBroadcast, int udpBroadcastInterval)
{
    if (m_currentUserIdStr.isEmpty())
        return;

    bool settingsChanged = false;
    QSettings settings;
    QString profileGroup = "UserAccounts/" + m_currentUserIdStr + "/Profile";
    QString userSettingsGroup = "UserAccounts/" + m_currentUserIdStr + "/Settings";

    bool listeningPrefsChanged = false;
    bool udpDiscoveryPrefsChanged = false;

    settings.beginGroup(profileGroup);
    if (localUserName != userName)
    {
        localUserName = userName;
        settings.setValue("localUserName", localUserName);
        if (networkManager)
        {
            networkManager->setLocalUserDetails(localUserUuid, localUserName);
        }
        settingsChanged = true;
    }
    settings.endGroup();

    settings.beginGroup(userSettingsGroup);
    if (localListenPort != listenPort)
    {
        localListenPort = listenPort;
        settings.setValue("ListenPort", localListenPort);
        settingsChanged = true;
        listeningPrefsChanged = true;
    }
    if (autoNetworkListeningEnabled != enableListening)
    {
        autoNetworkListeningEnabled = enableListening;
        settings.setValue("AutoNetworkListeningEnabled", autoNetworkListeningEnabled);
        settingsChanged = true;
        listeningPrefsChanged = true;
    }

    if (udpDiscoveryEnabled != enableUdpDiscovery ||
        localUdpDiscoveryPort != udpDiscoveryPort ||
        udpContinuousBroadcastEnabled != enableContinuousUdpBroadcast ||
        udpBroadcastIntervalSeconds != udpBroadcastInterval)
    {
        udpDiscoveryEnabled = enableUdpDiscovery;
        localUdpDiscoveryPort = udpDiscoveryPort;
        udpContinuousBroadcastEnabled = enableContinuousUdpBroadcast;
        udpBroadcastIntervalSeconds = udpBroadcastInterval;
        settings.setValue("UdpDiscoveryEnabled", udpDiscoveryEnabled);
        settings.setValue("UdpDiscoveryPort", localUdpDiscoveryPort);
        settings.setValue("UdpContinuousBroadcastEnabled", udpContinuousBroadcastEnabled);
        settings.setValue("UdpBroadcastIntervalSeconds", udpBroadcastIntervalSeconds);
        settingsChanged = true;
        udpDiscoveryPrefsChanged = true;
    }

    if (localOutgoingPort != outgoingPort || useSpecificOutgoingPort != useSpecificOutgoingPortVal)
    {
        localOutgoingPort = outgoingPort;
        useSpecificOutgoingPort = useSpecificOutgoingPortVal;
        settings.setValue("OutgoingPort", localOutgoingPort);
        settings.setValue("UseSpecificOutgoingPort", useSpecificOutgoingPort);
        settingsChanged = true;
        if (networkManager)
        {
            networkManager->setOutgoingConnectionPreferences(localOutgoingPort, useSpecificOutgoingPort);
        }
    }
    settings.endGroup();

    if (settingsChanged)
    {
        settings.sync();
    }

    if (listeningPrefsChanged && networkManager)
    {
        networkManager->setListenPreferences(localListenPort, autoNetworkListeningEnabled);
    }
    else if (!settingsChanged && !udpDiscoveryPrefsChanged)
    {
        if (!listeningPrefsChanged && !udpDiscoveryPrefsChanged && !settingsChanged) {
            updateNetworkStatus(tr("Settings unchanged."));
        }
    }

    if (udpDiscoveryPrefsChanged && networkManager)
    {
        networkManager->setUdpDiscoveryPreferences(udpDiscoveryEnabled, localUdpDiscoveryPort,
                                                   udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds);
    }

    if (settingsChanged) {
        updateNetworkStatus(tr("Settings have been saved. Network status will update based on changes."));
    }
}

void MainWindow::onSettingsButtonClicked()
{
    if (m_currentUserIdStr.isEmpty())
        return;

    // 使用 MainWindow 当前的成员变量来初始化或更新 SettingsDialog
    // 这是旧版稳定代码的行为，确保对话框反映应用的当前状态。
    if (!settingsDialog)
    {
        settingsDialog = new SettingsDialog(localUserName, localUserUuid,
                                            localListenPort, autoNetworkListeningEnabled,
                                            localOutgoingPort, useSpecificOutgoingPort,
                                            udpDiscoveryEnabled, localUdpDiscoveryPort,
                                            udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds,
                                            this);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::handleSettingsApplied);
        connect(settingsDialog, &SettingsDialog::retryListenNowRequested, this, &MainWindow::handleRetryListenNowRequested);
        connect(settingsDialog, &SettingsDialog::manualUdpBroadcastRequested, this, &MainWindow::handleManualUdpBroadcastRequested);
    }
    else
    {
        // 如果对话框已存在，用当前 MainWindow 的成员变量更新其字段
        settingsDialog->updateFields(localUserName, localUserUuid,
                                     localListenPort, autoNetworkListeningEnabled,
                                     localOutgoingPort, useSpecificOutgoingPort,
                                     udpDiscoveryEnabled, localUdpDiscoveryPort,
                                     udpContinuousBroadcastEnabled, udpBroadcastIntervalSeconds);
    }
    settingsDialog->exec();
}

void MainWindow::saveChatHistory(const QString &peerUuid)
{
    if (m_currentUserIdStr.isEmpty())
    {
        qWarning() << "MainWindow::saveChatHistory: Current user ID is empty. Cannot save history.";
        return;
    }
    if (!chatHistoryManager)
    {
        qWarning() << "MainWindow::saveChatHistory: ChatHistoryManager is null. Cannot save history for peer" << peerUuid;
        return;
    }

    if (chatHistories.contains(peerUuid))
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
        qWarning() << "MainWindow::saveChatHistory: No history in memory for peer" << peerUuid;
    }
}

void MainWindow::handleRetryListenNowRequested()
{
    if (networkManager) {
        updateNetworkStatus(tr("Attempting to start listening manually..."));
        networkManager->startListening(); // 这个方法应该会发出成功或失败的状态消息
    } else {
        updateNetworkStatus(tr("NetworkManager is not available. Cannot start listening."));
    }
}

void MainWindow::handleManualUdpBroadcastRequested()
{
    if (networkManager) {
        updateNetworkStatus(tr("Attempting to send manual UDP discovery broadcast..."));
        networkManager->triggerManualUdpBroadcast();
    } else {
        updateNetworkStatus(tr("NetworkManager is not available. Cannot send UDP broadcast."));
    }
}

void MainWindow::onMessageInputTextChanged()
{
    if (clearMessageButton && messageInputEdit) {
        clearMessageButton->setVisible(!messageInputEdit->toPlainText().isEmpty());
    }
}

void MainWindow::onClearMessageInputClicked()
{
    if (messageInputEdit) {
        messageInputEdit->clear();
        messageInputEdit->setFocus();
    }
}

void MainWindow::onContactSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current)
    {
        currentOpenChatContactName = current->text();
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

        if (networkManager && peerInfoDisplayWidget)
        {
            QAbstractSocket::SocketState state = networkManager->getPeerSocketState(peerUuid);
            if (state == QAbstractSocket::ConnectedState)
            {
                QPair<QString, quint16> netInfo = networkManager->getPeerInfo(peerUuid);
                QString ipAddr = networkManager->getPeerIpAddress(peerUuid);
                peerInfoDisplayWidget->updateDisplay(netInfo.first, peerUuid, ipAddr, netInfo.second);
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
                peerInfoDisplayWidget->updateDisplay(currentOpenChatContactName, peerUuid, tr("Not Connected"), 0);
                messageInputEdit->setEnabled(false);
            }
        }

        QStringList fullHistory;
        if (chatHistories.contains(peerUuid))
        {
            fullHistory = chatHistories.value(peerUuid);
            qDebug() << "onContactSelected: Using in-memory history for" << peerUuid << "Count:" << fullHistory.count();
        }
        else
        {
            if (chatHistoryManager)
            {
                fullHistory = chatHistoryManager->loadChatHistory(peerUuid);
                chatHistories[peerUuid] = fullHistory;
                qDebug() << "onContactSelected: Loaded history using ChatHistoryManager for" << peerUuid << "Count:" << fullHistory.count();
            }
            else
            {
                qWarning() << "onContactSelected: ChatHistoryManager is null. Cannot load history for" << peerUuid;
                chatHistories[peerUuid] = QStringList();
            }
        }

        messageDisplay->setMessages(fullHistory);

        current->setBackground(QBrush());

        messageInputEdit->clear();
        messageInputEdit->setFocus();
        if (chatStackedWidget->currentWidget() != activeChatContentsWidget)
        {
            chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
        }
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

        // 获取当前时间并格式化
        QString currentTime = QDateTime::currentDateTime().toString("HH:mm");
        QString timestampHtml = QString(
                                    "<div style=\"text-align: center; margin-bottom: 5px;\">"
                                    "<span style=\"background-color: #aaaaaa; color: white; padding: 2px 8px; border-radius: 10px; font-size: 9pt;\">%1</span>"
                                    "</div>")
                                    .arg(currentTime);

        QString userMessageHtml = QString(
                                      "<div style=\"text-align: right; margin-bottom: 2px;\">"
                                      "<p style=\"margin:0; padding:0; text-align: right;\">"
                                      "<span style=\"font-weight: bold; background-color: #a7dcb2; padding: 2px 6px; margin-left: 4px; border-radius: 3px;\">%1:</span> %2"
                                      "</p>"
                                      "</div>")
                                      .arg(localUserName.toHtmlEscaped())
                                      .arg(coreContent);

        QString activeContactUuid = targetPeerUuid; 

        if (!activeContactUuid.isEmpty())
        {
            chatHistories[activeContactUuid].append(timestampHtml);
            chatHistories[activeContactUuid].append(userMessageHtml); 
            saveChatHistory(activeContactUuid);                       
        }
        else
        {
            qWarning() << "Sending message: Active contact" << currentOpenChatContactName << "has no UUID. Using name as fallback for history.";
            chatHistories[currentOpenChatContactName].append(timestampHtml);
            chatHistories[currentOpenChatContactName].append(userMessageHtml);
        }

        messageDisplay->addMessage(timestampHtml); // 先显示时间戳
        messageDisplay->addMessage(userMessageHtml); // 再显示消息

        networkManager->sendMessage(activeContactUuid, coreContent); 

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
        quint16 savedContactPort = settings.value("port").toUInt();

        if (uuid.isEmpty() || name.isEmpty())
            continue;

        bool found = false;
        for (int j = 0; j < contactListWidget->count(); ++j)
        {
            if (contactListWidget->item(j)->data(Qt::UserRole).toString() == uuid)
            {
                contactListWidget->item(j)->setText(name);
                contactListWidget->item(j)->setData(Qt::UserRole + 1, ip);
                contactListWidget->item(j)->setData(Qt::UserRole + 2, savedContactPort);
                contactListWidget->item(j)->setIcon(QIcon(":/icons/offline.svg"));
                found = true;
                break;
            }
        }
        if (!found)
        {
            QListWidgetItem *item = new QListWidgetItem(name, contactListWidget);
            item->setData(Qt::UserRole, uuid);     // Store UUID
            item->setData(Qt::UserRole + 1, ip);   // Store IP
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
                                        .arg(name)
                                        .arg(uuid)
                                        .arg(ip)
                                        .arg(localListenPort));
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
                                            .arg(name)
                                            .arg(uuid)
                                            .arg(ip)
                                            .arg(savedContactPort));
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

// 实现 getLocalListenPort
quint16 MainWindow::getLocalListenPort() const
{
    return localListenPort; // localListenPort 是 MainWindow 的成员变量
}

// 实现 handleContactAdded
void MainWindow::handleContactAdded(const QString &name, const QString &uuid, const QString &ip, quint16 port)
{
    if (uuid.isEmpty() || name.isEmpty())
    {
        qWarning() << "MainWindow::handleContactAdded: Attempted to add contact with empty name or UUID.";
        return;
    }

    // 检查联系人是否已存在 (基于UUID)
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *item = contactListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == uuid)
        {
            // 更新现有联系人信息
            item->setText(name);
            item->setData(Qt::UserRole + 1, ip);   // Store IP
            item->setData(Qt::UserRole + 2, port); // Store Port
            // 可以根据网络状态更新图标，例如默认为离线
            if (networkManager && networkManager->getPeerSocketState(uuid) == QAbstractSocket::ConnectedState)
            {
                item->setIcon(QIcon(":/icons/online.svg"));
            }
            else
            {
                item->setIcon(QIcon(":/icons/offline.svg"));
            }
            qInfo() << "Contact updated:" << name << "UUID:" << uuid;
            return;
        }
    }

    // 添加新联系人
    QListWidgetItem *newItem = new QListWidgetItem(name, contactListWidget);
    newItem->setData(Qt::UserRole, uuid);     // Store UUID
    newItem->setData(Qt::UserRole + 1, ip);   // Store IP
    newItem->setData(Qt::UserRole + 2, port); // Store Port
    // 根据网络状态设置初始图标
    if (networkManager && networkManager->getPeerSocketState(uuid) == QAbstractSocket::ConnectedState)
    {
        newItem->setIcon(QIcon(":/icons/online.svg"));
    }
    else
    {
        newItem->setIcon(QIcon(":/icons/offline.svg"));
    }
    contactListWidget->addItem(newItem);
    qInfo() << "Contact added:" << name << "UUID:" << uuid;

    // 联系人添加后，可以选择保存联系人列表
    // saveCurrentUserContacts(); // 取决于您希望何时保存
}

// 实现 onAddContactButtonClicked
void MainWindow::onAddContactButtonClicked()
{
    if (contactManager)
    {
        contactManager->showAddContactDialog(this);
    }
    else
    {
        qWarning() << "MainWindow::onAddContactButtonClicked: ContactManager is null!";
        QMessageBox::critical(this, tr("Error"), tr("Contact management service is not available."));
    }
}

void MainWindow::onClearButtonClicked()
{
    QListWidgetItem *currentItem = contactListWidget->currentItem();
    if (!currentItem)
    {
        updateNetworkStatus(tr("No active chat selected to clear."));
        return;
    }

    QString peerUuid = currentItem->data(Qt::UserRole).toString();
    QString peerName = currentItem->text();

    if (peerUuid.isEmpty())
    {
        updateNetworkStatus(tr("Selected contact has no UUID. Cannot clear history."));
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Clear Chat History"),
                                  tr("Are you sure you want to clear the chat history with %1? This action cannot be undone.").arg(peerName),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // 清除UI显示
        if (messageDisplay && chatStackedWidget->currentWidget() == activeChatContentsWidget)
        {
            // 确保当前聊天窗口是选中的联系人
            QListWidgetItem *currentSelection = contactListWidget->currentItem();
            if (currentSelection && currentSelection->data(Qt::UserRole).toString() == peerUuid)
            {
                messageDisplay->clear();
            }
        }

        // 清除内存中的历史记录
        if (chatHistories.contains(peerUuid))
        {
            chatHistories[peerUuid].clear();
        }

        // 通过ChatHistoryManager删除持久化存储
        if (chatHistoryManager)
        {
            chatHistoryManager->clearChatHistory(peerUuid);
        }
        updateNetworkStatus(tr("Chat history with %1 cleared.").arg(peerName));
        qInfo() << "Chat history cleared for peer UUID:" << peerUuid;
    }
}
