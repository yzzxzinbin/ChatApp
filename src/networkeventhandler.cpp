#include "networkeventhandler.h"
#include "networkmanager.h"
#include "mainwindow.h" // Include to access MainWindow methods/members
#include "chatmessagedisplay.h"
#include "peerinfowidget.h"

#include <QListWidget>
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

void NetworkEventHandler::handleNetworkConnected()
{
    if (!mainWindowPtr || !networkManager || !peerInfoDisplayWidget || !contactListWidget || !chatStackedWidget || !messageInputEdit) return;

    mainWindowPtr->updateNetworkStatus(tr("Connected."));
    if (!networkManager->getPeerInfo().first.isEmpty())
    {
        QPair<QString, quint16> peerInfo = networkManager->getPeerInfo();
        QString peerUuid = networkManager->getCurrentPeerUuid();
        QString peerIpAddress = networkManager->getCurrentPeerIpAddress();
        if (peerUuid.isEmpty())
            peerUuid = tr("N/A (UUID not received)");

        peerInfoDisplayWidget->updateDisplay(peerInfo.first, peerUuid, peerIpAddress, peerInfo.second);

        QString peerName = peerInfo.first;

        QListWidgetItem *itemToSelect = nullptr;
        if (!peerUuid.isEmpty())
        {
            for (int i = 0; i < contactListWidget->count(); ++i)
            {
                QListWidgetItem *existingItem = contactListWidget->item(i);
                if (existingItem->data(Qt::UserRole).toString() == peerUuid)
                {
                    itemToSelect = existingItem;
                    if (itemToSelect->text() != peerName)
                    {
                        itemToSelect->setText(peerName);
                    }
                    break;
                }
            }
        }

        if (!itemToSelect)
        {
            // Call MainWindow's method to handle adding/updating contact in the list
            mainWindowPtr->handleContactAdded(peerName, peerUuid);
            for (int i = 0; i < contactListWidget->count(); ++i)
            {
                if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
                {
                    itemToSelect = contactListWidget->item(i);
                    break;
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

void NetworkEventHandler::handleNetworkDisconnected()
{
    if (!mainWindowPtr || !peerInfoDisplayWidget) return;

    mainWindowPtr->updateNetworkStatus(tr("Disconnected."));
    if (networkManager)
    {
        QString lastName = networkManager->getPeerInfo().first;
        QString lastUuid = networkManager->getCurrentPeerUuid();
        peerInfoDisplayWidget->setDisconnectedState(lastName, lastUuid);
    }
    else
    {
        peerInfoDisplayWidget->setDisconnectedState(tr("N/A"), tr("N/A"));
    }
}

void NetworkEventHandler::handleNewMessageReceived(const QString &message)
{
    if (!mainWindowPtr || !networkManager || !chatHistories || !contactListWidget || !messageDisplay) return;

    QString peerName = "";
    QString peerUuid = "";

    peerName = networkManager->getPeerInfo().first;
    peerUuid = networkManager->getCurrentPeerUuid();

    if (peerName.isEmpty() || peerUuid.isEmpty())
    {
        mainWindowPtr->updateNetworkStatus(tr("Received message from unknown peer or peer without UUID."));
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
        mainWindowPtr->handleContactAdded(peerName, peerUuid); // Use MainWindow's method
        for (int i = 0; i < contactListWidget->count(); ++i)
        {
            if (contactListWidget->item(i)->data(Qt::UserRole).toString() == peerUuid)
            {
                contactItem = contactListWidget->item(i);
                break;
            }
        }
    }
    
    // This logic was slightly problematic if contactItem is still null after trying to add.
    // Ensure contactItem is valid before proceeding or setCurrentItem.
    if (!contactItem) {
        qWarning() << "Could not find or add contact for incoming message from UUID:" << peerUuid;
        mainWindowPtr->updateNetworkStatus(tr("Error: Could not associate message with a contact."));
        return; // Cannot proceed without a valid contact item
    }

    if (contactListWidget->currentItem() != contactItem)
    {
        contactListWidget->setCurrentItem(contactItem);
        // Note: setCurrentItem will trigger onContactSelected in MainWindow,
        // which will load history. So, adding the message to history first is important.
    }
    
    QString activeContactDisplayName = contactItem->text(); // Use the name from the list item

    QString receivedMessageHtml = QString(
                                      "<div style=\"text-align: left; margin-bottom: 2px;\">"
                                      "<p style=\"margin:0; padding:0; text-align: left;\">"
                                      "<span style=\"font-weight: bold; background-color: #97c5f5; padding: 2px 6px; margin-right: 4px; border-radius: 3px;\">%1:</span> %2"
                                      "</p>"
                                      "</div>")
                                      .arg(activeContactDisplayName.toHtmlEscaped())
                                      .arg(message);

    (*chatHistories)[peerUuid].append(receivedMessageHtml);

    QListWidgetItem *currentUiItem = contactListWidget->currentItem();
    if (currentUiItem && currentUiItem->data(Qt::UserRole).toString() == peerUuid)
    {
        messageDisplay->addMessage(receivedMessageHtml);
    }
    else
    {
        // This case should ideally be less frequent if setCurrentItem above works as expected
        // and onContactSelected loads the history correctly.
        mainWindowPtr->updateNetworkStatus(tr("New message from %1").arg(activeContactDisplayName));
        contactItem->setBackground(Qt::lightGray); // Visual cue for unread message in a non-active chat
    }
}

void NetworkEventHandler::handleNetworkError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    if (!mainWindowPtr || !networkManager) return;
    mainWindowPtr->updateNetworkStatus(tr("Network Error: %1").arg(networkManager->getLastError()));
}
