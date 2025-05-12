#include "contactmanager.h"
#include "addcontactdialog.h" // 确保包含了完整定义
#include "networkmanager.h" // Include NetworkManager
#include <QWidget>
#include <QDebug> // For debugging

ContactManager::ContactManager(NetworkManager* networkManager, QObject *parent)
    : QObject(parent), netManager(networkManager) // currentAddDialog (QPointer) 默认初始化为 nullptr
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
    // 使用 QPointer，如果 currentAddDialog 指向的对象已被删除，则 currentAddDialog 会像 nullptr 一样。
    if (currentAddDialog && currentAddDialog->isVisible()) {
        currentAddDialog->activateWindow();
        return;
    }
    // 如果 currentAddDialog 为空 (之前的对话框已删除)
    // 或者 currentAddDialog 不为空但不可见 (这种情况在 WA_DeleteOnClose 下应该较少，但为了完整性处理)
    // 我们都创建一个新的对话框。

    AddContactDialog* newDialog = new AddContactDialog(parentWidget);
    currentAddDialog = newDialog; // QPointer 现在跟踪 newDialog

    connect(newDialog, &AddContactDialog::connectRequested, this, &ContactManager::handleConnectRequested);
    connect(this, &ContactManager::statusUpdate, newDialog, &AddContactDialog::setStatus);
    connect(newDialog, &QDialog::finished, this, &ContactManager::onDialogFinished);

    newDialog->setAttribute(Qt::WA_DeleteOnClose); // 确保对话框关闭时被删除
    newDialog->open();
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

    // 检查 currentAddDialog (QPointer) 是否有效且可见
    if (currentAddDialog && currentAddDialog->isVisible() && peerName == pendingContactName) {
        emit statusUpdate(tr("Session with %1 established!").arg(peerName), true, false);
        currentAddDialog->accept(); // 关闭对话框
        // pendingContactName 将在 onDialogFinished 中被清除
    } else if (peerName == pendingContactName) {
        qDebug() << "CM::handlePeerSessionEstablished: Session for" << peerName << "established, but AddContactDialog was not active/visible or was already closed.";
    }
}

void ContactManager::handleOutgoingConnectionAttemptFailed(const QString& peerNameAttempted, const QString& reason)
{
    // 检查 currentAddDialog (QPointer) 是否有效且可见
    if (currentAddDialog && currentAddDialog->isVisible() && peerNameAttempted == pendingContactName) {
        emit statusUpdate(tr("Failed to connect to %1: %2").arg(peerNameAttempted).arg(reason), false, false);
    } else if (peerNameAttempted == pendingContactName) {
        qDebug() << "CM::handleOutgoingConnectionAttemptFailed: Connection to" << peerNameAttempted << "failed, but AddContactDialog was not active/visible or was already closed.";
    }
}

void ContactManager::onDialogFinished(int result)
{
    Q_UNUSED(result);
    AddContactDialog* dialogThatFinished = qobject_cast<AddContactDialog*>(sender());

    if (dialogThatFinished) {
        // 总是断开与实际完成的对话框的连接
        disconnect(this, &ContactManager::statusUpdate, dialogThatFinished, &AddContactDialog::setStatus);

        // 检查完成的对话框是否是 QPointer 当前跟踪的那个
        // currentAddDialog.data() 获取原始指针进行比较
        if (currentAddDialog.data() == dialogThatFinished) {
            qDebug() << "CM::onDialogFinished: Tracked AddContactDialog for" << pendingContactName << "closed. Clearing pendingContactName.";
            pendingContactName.clear();
            // currentAddDialog (QPointer) 将在 dialogThatFinished 因 WA_DeleteOnClose 被删除时自动变为 nullptr。
            // 不需要显式设置 currentAddDialog = nullptr;
        } else {
            // 这可能发生在：
            // 1. dialogThatFinished 确实是 currentAddDialog 指向的对象，但它已经被删除了，所以 currentAddDialog.data() 现在是 nullptr。
            // 2. 在旧对话框的 finished 信号处理之前，一个新的对话框被创建并赋给了 currentAddDialog。
            qDebug() << "CM::onDialogFinished: A dialog finished, but it wasn't the one currentAddDialog ("
                     << currentAddDialog.data() << ") was tracking, or currentAddDialog is already null. Finished dialog:" << dialogThatFinished;
        }
    }
}
