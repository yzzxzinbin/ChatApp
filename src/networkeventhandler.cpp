#include "networkeventhandler.h"
#include "networkmanager.h"
#include "mainwindow.h" // Include to access MainWindow methods/members
#include "chatmessagedisplay.h"
#include "peerinfowidget.h"

#include <QListWidget>
#include <QListWidgetItem> // Ensure QListWidgetItem is included
#include <QStackedWidget>
#include <QTextEdit>
#include <QLabel>
#include <QMap>
#include <QStringList>
#include <QDebug> // For qWarning

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
      mainWindowPtr(mainWindow)
{
}

void NetworkEventHandler::handlePeerConnected(const QString &peerUuid, const QString &peerName, const QString& peerAddress, quint16 peerPort)
{
    if (!mainWindowPtr || !networkManager || !peerInfoDisplayWidget || !contactListWidget || !chatStackedWidget || !messageInputEdit) return;

    mainWindowPtr->updateNetworkStatus(tr("Peer '%1' (UUID: %2) connected from %3:%4.").arg(peerName).arg(peerUuid).arg(peerAddress).arg(peerPort));

    mainWindowPtr->handleContactAdded(peerName, peerUuid, peerAddress, peerPort);

    QListWidgetItem *itemToSelect = nullptr;
    for (int i = 0; i < contactListWidget->count(); ++i) {
        QListWidgetItem *existingItem = contactListWidget->item(i);
        if (existingItem->data(Qt::UserRole).toString() == peerUuid) {
            itemToSelect = existingItem;
            if (itemToSelect->text() != peerName) {
                itemToSelect->setText(peerName);
            }
            // 确保图标更新为在线
            itemToSelect->setIcon(QIcon(":/icons/online.svg"));
            break;
        }
    }

    if (itemToSelect) {
        // 检查是否是当前选中的项
        if (contactListWidget->currentItem() == itemToSelect) {
            // 如果是当前选中的项，并且连接成功了，则需要手动更新UI状态，
            // 因为 currentItemChanged 可能不会再次触发 onContactSelected
            peerInfoDisplayWidget->updateDisplay(peerName, peerUuid, peerAddress, peerPort);
            messageInputEdit->setEnabled(true);
            messageInputEdit->setFocus();
            if (chatStackedWidget->currentWidget() != activeChatContentsWidget) {
                 chatStackedWidget->setCurrentWidget(activeChatContentsWidget);
            }
        } else if (!contactListWidget->currentItem() || contactListWidget->currentItem()->data(Qt::UserRole).toString() == peerUuid) {
            // 如果没有选中项，或者选中的就是这个刚连接的项（但上面if已处理），则选中它
            // 这个分支主要用于首次连接或切换到此联系人时
            contactListWidget->setCurrentItem(itemToSelect); // 这会触发 onContactSelected
            // onContactSelected 会处理 messageInputEdit->setFocus() 和其他UI更新
        }
        // 如果 itemToSelect 不是当前选中的项，它的图标已经在上面设置了，
        // 当用户点击它时，onContactSelected 会处理其余的UI更新。
    }
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

    QString receivedMessageHtml = QString(
                                      "<div style=\"text-align: left; margin-bottom: 2px;\">"
                                      "<p style=\"margin:0; padding:0; text-align: left;\">"
                                      "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span> %2"
                                      "</p>"
                                      "</div>")
                                      .arg(contactName.toHtmlEscaped())
                                      .arg(message);

    (*chatHistories)[peerUuid].append(receivedMessageHtml);
    mainWindowPtr->saveChatHistory(peerUuid); // 新增：保存聊天记录

    if (contactListWidget->currentItem() == contactItem) {
        messageDisplay->addMessage(receivedMessageHtml);
    } else {
        contactItem->setBackground(Qt::lightGray);
        mainWindowPtr->updateNetworkStatus(tr("New message from %1.").arg(contactName));
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
