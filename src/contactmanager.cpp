#include "contactmanager.h"
#include "addcontactdialog.h"
#include "networkmanager.h" // Include NetworkManager
#include <QWidget>
#include <QDebug> // For debugging

ContactManager::ContactManager(NetworkManager* networkManager, QObject *parent)
    : QObject(parent), netManager(networkManager), currentAddDialog(nullptr)
{
    if (netManager) {
        connect(netManager, &NetworkManager::peerConnected, this, &ContactManager::handlePeerSessionEstablished);
        connect(netManager, &NetworkManager::outgoingConnectionFailed, this, &ContactManager::handleOutgoingConnectionAttemptFailed);
    } else {
        qWarning() << "ContactManager initialized with null NetworkManager!";
    }
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
    Q_UNUSED(connectionType); 
    
    if (netManager) {
        emit statusUpdate(tr("Attempting to connect to %1 (%2:%3)...").arg(name).arg(ipAddress).arg(port), false, true);
        netManager->connectToHost(name, QString(), ipAddress, port);
    } else {
        emit statusUpdate(tr("Network service not available."), false, false);
         if (currentAddDialog) {
            currentAddDialog->setEnabled(true); // Re-enable connect button or similar
        }
    }
}

void ContactManager::handlePeerSessionEstablished(const QString &peerUuid, const QString &peerName, const QString& peerAddress, quint16 peerPort)
{
    Q_UNUSED(peerUuid);
    Q_UNUSED(peerAddress);
    Q_UNUSED(peerPort);

    if (currentAddDialog && currentAddDialog->isVisible() && peerName == pendingContactName) {
        emit statusUpdate(tr("Session with %1 established!").arg(peerName), true, false);
        // MainWindow::handleContactAdded 应该已经处理了联系人列表的更新
        // 此处不再需要 emit contactAdded，因为 MainWindow 会通过 NetworkEventHandler::handlePeerConnected 间接调用 handleContactAdded
        currentAddDialog->accept(); // 关闭对话框
        pendingContactName.clear();
    }
}

void ContactManager::handleOutgoingConnectionAttemptFailed(const QString& peerNameAttempted, const QString& reason)
{
    if (currentAddDialog && currentAddDialog->isVisible() && peerNameAttempted == pendingContactName) {
        emit statusUpdate(tr("Failed to connect to %1: %2").arg(peerNameAttempted).arg(reason), false, false);
        // 对话框保持打开，以便用户可以看到错误。AddContactDialog::setStatus 会重新启用按钮。
        pendingContactName.clear();
    }
}
