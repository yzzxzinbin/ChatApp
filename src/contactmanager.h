#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <QObject>
#include <QString>
#include "addcontactdialog.h" // Include the new dialog

// Forward declaration
class NetworkManager;

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
    // Slots to handle NetworkManager's connection results
    void onConnectionSuccess();
    void onConnectionFailed();
    void onTcpLinkEstablished(const QString& tentativePeerName); // 新槽

private:
    NetworkManager* netManager; // Store a pointer to NetworkManager
    AddContactDialog* currentAddDialog; // Pointer to the currently open dialog
    QString pendingContactName; // Name of the contact being added
};

#endif // CONTACTMANAGER_H
