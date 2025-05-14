#include "networkeventhandler.h"
#include "networkmanager.h"
#include "mainwindow.h" // Include to access MainWindow methods/members
#include "chatmessagedisplay.h"
#include "peerinfowidget.h"
#include "filetransfermanager.h" // <-- Add this

#include <QListWidget>
#include <QListWidgetItem> // Ensure QListWidgetItem is included
#include <QStackedWidget>
#include <QTextEdit>
#include <QLabel>
#include <QMap>
#include <QStringList>
#include <QDebug> // For qWarning
#include <QDateTime> // For timestamp

NetworkEventHandler::NetworkEventHandler(
    NetworkManager *nm,
    QListWidget *contactList,
    ChatMessageDisplay *msgDisplay,
    PeerInfoWidget *peerInfo,
    QStackedWidget *chatStack,
    QTextEdit *msgInput,
    QLabel *emptyPlaceholder,
    QWidget *activeChatWidget,
    QMap<QString, QStringList> *histories,
    MainWindow *mainWindow,
    FileTransferManager *ftm, // <-- Add this
    QObject *parent)
    : QObject(parent),
      networkManager(nm),
      contactListWidget(contactList),
      messageDisplay(msgDisplay),
      peerInfoDisplayWidget(peerInfo),
      chatStackedWidget(chatStack),
      messageInputEdit(msgInput),
      emptyChatPlaceholderLabel(emptyPlaceholder),
      activeChatContentsWidget(activeChatWidget),
      chatHistories(histories),
      mainWindowPtr(mainWindow),
      fileTransferManager(ftm) // <-- Initialize this
{
    if (!fileTransferManager) {
        qWarning() << "NetworkEventHandler initialized with a null FileTransferManager!";
    }
}

void NetworkEventHandler::handlePeerConnected(const QString &peerUuid, const QString &peerName, const QString &peerAddress, quint16 peerPort)
{
    if (!mainWindowPtr) return; // Guard

    qDebug() << "NEH::handlePeerConnected: UUID:" << peerUuid << "Name:" << peerName << "Addr:" << peerAddress << "ConnectedOnPort:" << peerPort;

    QListWidgetItem *existingItem = nullptr;
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
        {
            existingItem = contactListWidget->item(i);
            break;
        }
    }

    quint16 portToStore;

    if (existingItem)
    {
        // Contact exists. Update its name (if different) and IP.
        qDebug() << "NEH::handlePeerConnected: Existing contact" << peerUuid << ". Current listening port stored:" << existingItem->data(Qt::UserRole + 2).toUInt();
        portToStore = existingItem->data(Qt::UserRole + 2).toUInt(); // Use existing stored listening port
        
        if (existingItem->text() != peerName) {
            existingItem->setText(peerName); // Update display name if it changed
        }
        // Update IP if it changed
        if (existingItem->data(Qt::UserRole + 1).toString() != peerAddress) {
            existingItem->setData(Qt::UserRole + 1, peerAddress);
        }
         // Call handleContactAdded to ensure contact list consistency and trigger save if needed.
         // It will update if name/ip changed, using the correct listening port.
        mainWindowPtr->handleContactAdded(peerName, peerUuid, peerAddress, portToStore);

    }
    else
    {
        // New contact.
        portToStore = mainWindowPtr->getLocalListenPort(); // Use current user's listen port as default for peer's listening port.
        qDebug() << "NEH::handlePeerConnected: New contact" << peerUuid << ". Using current user's listen port as default for peer's listening port:" << portToStore;
        mainWindowPtr->handleContactAdded(peerName, peerUuid, peerAddress, portToStore);
    }

    // Update UI for the connected peer
    for (int i = 0; i < contactListWidget->count(); ++i)
    {
        QListWidgetItem *item = contactListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == peerUuid)
        {
            item->setIcon(QIcon(":/icons/online.svg"));
            quint16 actualStoredPort = item->data(Qt::UserRole + 2).toUInt();

            // If this is the currently selected chat, refresh its info and enable input
            // This part handles updates if the item was already current.
            if (contactListWidget->currentItem() == item)
            {
                if (peerInfoDisplayWidget) {
                     // Use actualStoredPort (which is the listening port) for display
                    peerInfoDisplayWidget->updateDisplay(peerName, peerUuid, peerAddress, actualStoredPort);
                }
                if (messageInputEdit) messageInputEdit->setEnabled(true);
                if (chatStackedWidget && activeChatContentsWidget && chatStackedWidget->currentWidget() != activeChatContentsWidget) {
                    chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
                }
            }
            
            // Ensure this item becomes the current item to open/refresh the chat window.
            // This will trigger MainWindow::onContactSelected.
            contactListWidget->setCurrentItem(item);
            break;
        }
    }
    mainWindowPtr->updateNetworkStatus(tr("Connected to %1 (UUID: %2).").arg(peerName).arg(peerUuid));
}

void NetworkEventHandler::handlePeerDisconnected(const QString &peerUuid)
{
    if (!mainWindowPtr || !peerInfoDisplayWidget || !contactListWidget) return;

    QString peerName = tr("Unknown");
    QListWidgetItem *disconnectedItem = nullptr;
    for (int i = 0; i < contactListWidget->count(); ++i) {
        if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid) {
            disconnectedItem = contactListWidget->item(i);
            peerName = disconnectedItem->text();
            break;
        }
    }

    mainWindowPtr->updateNetworkStatus(tr("Peer '%1' (UUID: %2) disconnected.").arg(peerName).arg(peerUuid));

    if (disconnectedItem) {
        disconnectedItem->setIcon(QIcon(":/icons/offline.svg"));
    }

    if (contactListWidget->currentItem() && contactListWidget->currentItem()->data(Qt::UserRole).toString() == peerUuid) {
        peerInfoDisplayWidget->setDisconnectedState(peerName, peerUuid);
        messageInputEdit->clear();
        messageInputEdit->setEnabled(false);
    }
}

void NetworkEventHandler::handleNewMessageReceived(const QString &peerUuid, const QString &message)
{
    if (!mainWindowPtr || !networkManager || !chatHistories || !contactListWidget || !messageDisplay) return;

    // Check if it's a file transfer message
    if (message.startsWith("<FT_")) {
        if (fileTransferManager) {
            fileTransferManager->handleIncomingFileMessage(peerUuid, message);
        } else {
            qWarning() << "NEH: Received file transfer message but FileTransferManager is null:" << message;
        }
        return; // Message handled (or attempted to be handled) by FileTransferManager
    }

    QListWidgetItem *contactItem = nullptr;
    QString contactName = tr("Unknown");
    QString contactIp = QString(); // 用于 handleContactAdded
    quint16 contactPort = 0;    // 用于 handleContactAdded

    for (int i = 0; i < contactListWidget->count(); ++i) {
        if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid) {
            contactItem = contactListWidget->item(i);
            contactName = contactItem->text();
            // 从列表项获取IP和端口（如果之前已连接并保存）
            contactIp = contactItem->data(Qt::UserRole + 1).toString();
            contactPort = contactItem->data(Qt::UserRole + 2).toUInt();
            break;
        }
    }

    if (!contactItem) {
        QPair<QString, quint16> info = networkManager->getPeerInfo(peerUuid); // 这会返回对端名称和端口
        QString actualPeerAddress = networkManager->getPeerIpAddress(peerUuid); // 获取实际IP
        if (!info.first.isEmpty()) {
            contactName = info.first;
            contactIp = actualPeerAddress;
            contactPort = info.second;
            // 如果联系人不存在，则添加。这通常发生在消息来自一个新建立的连接，
            // 而 peerConnected 事件可能由于某种原因尚未完全处理或UI尚未更新。
            mainWindowPtr->handleContactAdded(contactName, peerUuid, contactIp, contactPort);
             for (int i = 0; i < contactListWidget->count(); ++i) { // 再次查找
                if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid) {
                    contactItem = contactListWidget->item(i);
                    break;
                }
            }
        }
    }
    
    if (!contactItem) {
        qWarning() << "Received message from unknown peer UUID:" << peerUuid << ". Message ignored.";
        mainWindowPtr->updateNetworkStatus(tr("Received message from unknown peer %1. Message ignored.").arg(peerUuid));
        return;
    }

    // 获取当前时间并格式化
    QString currentTime = QDateTime::currentDateTime().toString("HH:mm");
    QString timestampHtml = QString(
                                "<div style=\"text-align: center; margin-bottom: 5px;\">"
                                "<span style=\"background-color: #aaaaaa; color: white; padding: 2px 8px; border-radius: 10px; font-size: 9pt;\">%1</span>"
                                "</div>")
                                .arg(currentTime);

    // 准备消息内容 HTML
    QString receivedMessageHtml = QString(
                                      "<div style=\"text-align: left; margin-bottom: 2px;\">"
                                      "<p style=\"margin:0; padding:0; text-align: left;\">"
                                      "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span> %2"
                                      "</p>"
                                      "</div>")
                                      .arg(contactName.toHtmlEscaped())
                                      .arg(message);

    // 添加时间戳和消息到历史记录 (总是保存)
    (*chatHistories)[peerUuid].append(timestampHtml);
    (*chatHistories)[peerUuid].append(receivedMessageHtml);
    mainWindowPtr->saveChatHistory(peerUuid);

    if (contactListWidget->currentItem() == contactItem) { // 如果是当前聊天窗口
        // 直接将时间戳和消息都交给 messageDisplay
        messageDisplay->addMessage(timestampHtml);
        messageDisplay->addMessage(receivedMessageHtml);
    } else {
        contactItem->setBackground(Qt::lightGray);
        mainWindowPtr->updateNetworkStatus(tr("New message from %1.").arg(contactName));
        // 对于非活动聊天，当它被选中时，onContactSelected 会处理历史记录中的时间戳显示
    }
}

void NetworkEventHandler::handlePeerNetworkError(const QString &peerUuid, QAbstractSocket::SocketError socketError, const QString& errorString)
{
    Q_UNUSED(socketError);
    if (!mainWindowPtr) return;

    QString peerName = tr("Unknown");
    if (networkManager) {
         QPair<QString, quint16> info = networkManager->getPeerInfo(peerUuid);
         if (!info.first.isEmpty()) {
             peerName = info.first;
         } else if (!peerUuid.isEmpty()){
             peerName = peerUuid;
         }
    }
    
    mainWindowPtr->updateNetworkStatus(tr("Network Error with peer %1: %2").arg(peerName).arg(errorString));
}
