#include "contactmanager.h"
#include "addcontactdialog.h"
#include "networkmanager.h" // Include NetworkManager
#include <QWidget>

ContactManager::ContactManager(NetworkManager* networkManager, QObject *parent)
    : QObject(parent), netManager(networkManager), currentAddDialog(nullptr)
{
    // Connect to NetworkManager signals for connection attempts initiated by this manager
    connect(netManager, &NetworkManager::connected, this, &ContactManager::onConnectionSuccess);
    // Assuming NetworkManager might emit a specific signal for connection failure or use a general error
    // For simplicity, we'll assume a generic error might mean connection failure for now.
    // A more specific signal like `connectionAttemptFailed()` from NetworkManager would be better.
    connect(netManager, &NetworkManager::networkError, this, &ContactManager::onConnectionFailed);

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
        netManager->connectToHost(ipAddress, port);
    }
}

void ContactManager::onConnectionSuccess()
{
    // This signal is general. We need to check if it's related to our pending contact.
    // This simple check assumes any 'connected' signal while currentAddDialog is up is for this attempt.
    // A more robust solution would involve NetworkManager passing back a context or ID.
    if (currentAddDialog && currentAddDialog->isVisible() && !pendingContactName.isEmpty()) {
        emit statusUpdate(tr("Successfully connected to %1!").arg(pendingContactName), true, false);
        emit contactAdded(pendingContactName); // This will add to MainWindow's contact list
        pendingContactName.clear();
        // currentAddDialog->accept(); // Or close it after a delay
    }
}

void ContactManager::onConnectionFailed()
{
    // Similar to onConnectionSuccess, check context
    if (currentAddDialog && currentAddDialog->isVisible()) {
        emit statusUpdate(tr("Failed to connect."), false, false);
        pendingContactName.clear();
    }
}
