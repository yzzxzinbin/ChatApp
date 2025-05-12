#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QString>
#include <QtSql/QSqlDatabase>

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool connectToDatabase(const QString &host, const QString &databaseName,
                           const QString &user, const QString &password, int port = 3306);
    void disconnectFromDatabase();
    bool isConnected() const;

    // User management
    // IMPORTANT: In a real application, passwords should be hashed.
    bool validateUser(const QString &username, const QString &password);
    bool addUser(const QString &username, const QString &password);
    bool userExists(const QString &username);


signals:
    void errorOccurred(const QString &errorMsg);

private:
    QSqlDatabase m_db;
    QString m_connectionName;
    bool createUsersTable(); // Helper to create table if it doesn't exist
};

#endif // DATABASEMANAGER_H
