#include "contactmanager.h"
#include "addcontactdialog.h"
#include "networkmanager.h" // Include NetworkManager
#include <QWidget>

ContactManager::ContactManager(NetworkManager* networkManager, QObject *parent)
    : QObject(parent), netManager(networkManager), currentAddDialog(nullptr)
{
    // Connect to NetworkManager signals for connection attempts initiated by this manager
    connect(netManager, &NetworkManager::connected, this, &ContactManager::onConnectionSuccess);
    connect(netManager, &NetworkManager::networkError, this, &ContactManager::onConnectionFailed);
    connect(netManager, &NetworkManager::tcpLinkEstablished, this, &ContactManager::onTcpLinkEstablished); // 连接新信号
}

void ContactManager::showAddContactDialog(QWidget *parentWidget)
{
    // Ensure only one dialog is open or handle appropriately
    if (currentAddDialog && currentAddDialog->isVisible()) {
        currentAddDialog->activateWindow();
        return;
    }

    currentAddDialog = new AddContactDialog(parentWidget);
    connect(currentAddDialog, &AddContactDialog::connectRequested, this, &ContactManager::handleConnectRequested);
    // Connect this manager's statusUpdate to the dialog's setStatus
    connect(this, &ContactManager::statusUpdate, currentAddDialog, &AddContactDialog::setStatus);

    currentAddDialog->setAttribute(Qt::WA_DeleteOnClose); // Ensure dialog is deleted when closed
    currentAddDialog->open();
}

void ContactManager::handleConnectRequested(const QString &name, const QString &connectionType, const QString &ipAddress, quint16 port)
{
    // Store the name for when connection is successful
    pendingContactName = name;
    // Here, connectionType is available if needed for IPv6 specific logic in NetworkManager (not implemented yet)
    Q_UNUSED(connectionType); 
    
    if (netManager) {
        // Emit status to dialog BEFORE calling connectToHost
        emit statusUpdate(tr("Attempting to connect to %1...").arg(ipAddress), false, true);
        netManager->connectToHost(name, ipAddress, port); // 传递 name 作为 peerNameToSet
    }
}

void ContactManager::onConnectionSuccess()
{
    QString establishedPeerName;
    QString establishedPeerUuid;
    QString establishedPeerIpAddress;
    quint16 establishedPeerPort = 0;

    if (netManager) {
        QPair<QString, quint16> peerNetInfo = netManager->getPeerInfo();
        establishedPeerName = peerNetInfo.first;
        establishedPeerPort = peerNetInfo.second;
        establishedPeerUuid = netManager->getCurrentPeerUuid();
        establishedPeerIpAddress = netManager->getCurrentPeerIpAddress();
    }

    if (currentAddDialog && currentAddDialog->isVisible()) {
        if (!establishedPeerName.isEmpty() && !establishedPeerUuid.isEmpty()) {
             emit statusUpdate(tr("Session with %1 (UUID: %2) accepted!").arg(establishedPeerName).arg(establishedPeerUuid), true, false);
             emit contactAdded(establishedPeerName, establishedPeerUuid, establishedPeerIpAddress, establishedPeerPort);
             currentAddDialog->accept();
        } else {
            emit statusUpdate(tr("Session accepted, but peer details (name/UUID/IP) missing."), false, false);
        }
    } else if (!establishedPeerName.isEmpty() && !establishedPeerUuid.isEmpty()) {
        // Dialog was closed, but connection succeeded. MainWindow::handleNetworkConnected will use NetworkManager's info.
        // contactAdded should still be emitted so MainWindow can store the UUID if it's a new contact.
         emit contactAdded(establishedPeerName, establishedPeerUuid, establishedPeerIpAddress, establishedPeerPort);
    }
    pendingContactName.clear(); // This was for the dialog's initial name, UUID is the key now.
}

void ContactManager::onConnectionFailed()
{
    // Similar to onConnectionSuccess, check context
    if (currentAddDialog && currentAddDialog->isVisible()) {
        emit statusUpdate(tr("Failed to connect."), false, false);
        pendingContactName.clear();
    }
}

void ContactManager::onTcpLinkEstablished(const QString& tentativePeerName)
{
    if (currentAddDialog && currentAddDialog->isVisible()) {
        // 确保 tentativePeerName 与 pendingContactName 一致，因为这是我们尝试连接的名称
        if (tentativePeerName == pendingContactName) {
            emit statusUpdate(tr("TCP link with %1 established. Waiting for peer to accept session...").arg(tentativePeerName), false, true);
        } else {
            // 如果名称不一致，可能是一个内部逻辑问题
             emit statusUpdate(tr("TCP link established, but name mismatch. Waiting for peer..."), false, true);
        }
        // 保持 AddContactDialog 中的 "Connect" 按钮禁用状态，因为它由 AddContactDialog::onConnectButtonClicked 控制
    }
}
