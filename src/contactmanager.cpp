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
    // NetworkManager::connected() 现在意味着会话已被对方完全接受
    QString establishedPeerName;
    if (netManager) {
        establishedPeerName = netManager->getPeerInfo().first;
    }

    if (currentAddDialog && currentAddDialog->isVisible()) {
        // 检查 NetworkManager 中最终确定的名称是否与我们尝试添加时用的名称一致
        if (!pendingContactName.isEmpty() && pendingContactName == establishedPeerName) {
             emit statusUpdate(tr("Session with %1 accepted!").arg(establishedPeerName), true, false);
             emit contactAdded(establishedPeerName); // 确保使用最终确认的名称
             currentAddDialog->accept(); // 关闭对话框
        } else if (!establishedPeerName.isEmpty()) {
            // 名称不匹配，这可能是一个逻辑问题或并发连接（不太可能在这个简单应用中）
            emit statusUpdate(tr("Session established with %1, but expected %2. Adding %1.")
                              .arg(establishedPeerName).arg(pendingContactName), true, false);
            emit contactAdded(establishedPeerName); // 添加实际连接上的名称
            currentAddDialog->accept();
        } else {
            // 连接成功了，但无法获取对方名称，或者pendingContactName为空但dialog仍在
            emit statusUpdate(tr("Session accepted, but name mismatch or missing. Please check contacts."), false, false);
        }
    } else if (!establishedPeerName.isEmpty()) {
        // AddContactDialog 已关闭，但连接成功了 (例如，用户在等待时关闭了对话框)
        // MainWindow::handleNetworkConnected 会处理将此联系人添加到列表
    }
    pendingContactName.clear();
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
