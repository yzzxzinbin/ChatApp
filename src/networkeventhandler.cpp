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

    mainWindowPtr->updateNetworkStatus(tr("Peer '%1' (UUID: %2) connected.").arg(peerName).arg(peerUuid));

    mainWindowPtr->handleContactAdded(peerName, peerUuid);

    QListWidgetItem *itemToSelect = nullptr;
    for (int i = 0; i < contactListWidget->count(); ++i) {
        QListWidgetItem *existingItem = contactListWidget->item(i);
        if (existingItem->data(Qt::UserRole).toString() == peerUuid) {
            itemToSelect = existingItem;
            if (itemToSelect->text() != peerName) {
                itemToSelect->setText(peerName);
            }
            break;
        }
    }

    if (itemToSelect) {
        if (!contactListWidget->currentItem() || contactListWidget->currentItem()->data(Qt::UserRole).toString() == peerUuid) {
            contactListWidget->setCurrentItem(itemToSelect);
            messageInputEdit->setFocus();
        } else {
            itemToSelect->setIcon(QIcon(":/icons/online.png"));
        }
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
        disconnectedItem->setIcon(QIcon(":/icons/offline.png"));
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

    for (int i = 0; i < contactListWidget->count(); ++i) {
        if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid) {
            contactItem = contactListWidget->item(i);
            contactName = contactItem->text();
            break;
        }
    }

    if (!contactItem) {
        QPair<QString, quint16> info = networkManager->getPeerInfo(peerUuid);
        if (!info.first.isEmpty()) {
            contactName = info.first;
            mainWindowPtr->handleContactAdded(contactName, peerUuid);
             for (int i = 0; i < contactListWidget->count(); ++i) {
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
