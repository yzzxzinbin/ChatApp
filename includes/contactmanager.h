#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <QObject>
#include <QString>
#include <QPointer> // 新增：包含 QPointer
// #include "addcontactdialog.h" // AddContactDialog 的完整定义现在不需要在这里，前向声明足够

// Forward declaration
class NetworkManager;
class AddContactDialog; // 前向声明 AddContactDialog

class ContactManager : public QObject
{
    Q_OBJECT
public:
    // Pass NetworkManager for connection attempts
    explicit ContactManager(NetworkManager* networkManager, QObject *parent = nullptr);

    void showAddContactDialog(QWidget *parentWidget);

signals:
    void contactAdded(const QString &name, const QString &uuid, const QString &ipAddress, quint16 port); // New: name, uuid, last ip, last port
    void statusUpdate(const QString &message, bool success, bool connecting); // To update AddContactDialog

private slots:
    void handleConnectRequested(const QString &name, const QString &connectionType, const QString &ipAddress, quint16 port);
    // 新的槽函数，用于处理来自 NetworkManager 的信号
    void handlePeerSessionEstablished(const QString &peerUuid, const QString &peerName, const QString& peerAddress, quint16 peerPort);
    void handleOutgoingConnectionAttemptFailed(const QString& peerNameAttempted, const QString& reason);
    void onDialogFinished(int result); // 新增槽：处理对话框关闭事件

private:
    NetworkManager* netManager; // Store a pointer to NetworkManager
    QPointer<AddContactDialog> currentAddDialog; // 修改类型为 QPointer
    QString pendingContactName; // Name of the contact being added
};

#endif // CONTACTMANAGER_H
