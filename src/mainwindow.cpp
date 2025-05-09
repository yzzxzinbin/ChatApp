#include "mainwindow.h"
#include "contactmanager.h"
#include "chatmessagedisplay.h"
#include "networkmanager.h" // 确保包含了 NetworkManager
#include "settingsdialog.h" // 确保包含了 SettingsDialog
#include "peerinfowidget.h" // Include the new PeerInfoWidget header
#include "styleutils.h"     // Include the new StyleUtils header
#include "formattingtoolbarhandler.h" // Include the new FormattingToolbarHandler header
#include "networkeventhandler.h" // Include the new NetworkEventHandler header

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

    setWindowTitle("Chat Application");
    resize(1024, 768);

    // Connect NetworkManager signals to NetworkEventHandler slots
    connect(networkManager, &NetworkManager::connected, networkEventHandler, &NetworkEventHandler::handleNetworkConnected);
    connect(networkManager, &NetworkManager::disconnected, networkEventHandler, &NetworkEventHandler::handleNetworkDisconnected);
    connect(networkManager, &NetworkManager::newMessageReceived, networkEventHandler, &NetworkEventHandler::handleNewMessageReceived);
    connect(networkManager, &NetworkManager::networkError, networkEventHandler, &NetworkEventHandler::handleNetworkError);

    // serverStatusMessage still connects to MainWindow's updateNetworkStatus
    connect(networkManager, &NetworkManager::serverStatusMessage, this, &MainWindow::updateNetworkStatus);
    // incomingConnectionRequest still connects to MainWindow's slot
    connect(networkManager, &NetworkManager::incomingConnectionRequest, this, &MainWindow::handleIncomingConnectionRequest);

    // 应用启动后默认开启端口监听
    networkManager->startListening();
}

MainWindow::~MainWindow()
{
    // settingsDialog 如果被设置了父对象，会被 Qt 自动管理内存
    // formattingHandler is a child of MainWindow, so it will be deleted automatically.
    // peerInfoDisplayWidget is a child of MainWindow, so it will be deleted automatically.
    // networkEventHandler is a child of MainWindow, so it will be deleted automatically.
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

void MainWindow::updateNetworkStatus(const QString &status)
{
    if (networkStatusLabel)
    {
        networkStatusLabel->setText(status);
    }
    else
    {
        if (statusBar()) { // Ensure statusBar is valid
            statusBar()->showMessage(status, 5000);
        } else {
            qWarning() << "statusBar is null, cannot show status message:" << status;
        }
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
