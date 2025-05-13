#include "databasemanager.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>
#include <QUuid> // For unique connection name

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
    // Generate a unique connection name to allow multiple instances or for testing
    m_connectionName = QString("chatapp_db_connection_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    qInfo() << "DatabaseManager instance created with connection name:" << m_connectionName;
}

DatabaseManager::~DatabaseManager()
{
    disconnectFromDatabase();
}

bool DatabaseManager::connectToDatabase(const QString &host, const QString &databaseName,
                                      const QString &user, const QString &password, int port)
{
    // Crucial Debug Output: Verify parameters received by this function
    qDebug() << "DatabaseManager::connectToDatabase CALLED WITH PARAMS for connection" << m_connectionName << ":";
    qDebug() << "  Host:" << host;
    qDebug() << "  DatabaseName:" << databaseName;
    qDebug() << "  User:" << user;
    qDebug() << "  Password:" << (password.isEmpty() ? "EMPTY" : "PROVIDED (length: " + QString::number(password.length()) + ")");
    qDebug() << "  Port:" << port;

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        if (m_db.isOpen()) {
            qInfo() << "Database connection" << m_connectionName << "is already open.";
            // If parameters are different from the currently open connection,
            // it might be better to close and reopen with new parameters.
            // For now, assume if it's open, it's with the correct (or intended previous) parameters.
            return true;
        }
    } else {
        m_db = QSqlDatabase::addDatabase("QMYSQL", m_connectionName);
    }
    
    // Set parameters on the m_db object obtained/added above
    m_db.setHostName(host);
    m_db.setDatabaseName(databaseName);
    m_db.setUserName(user);
    m_db.setPassword(password);
    m_db.setPort(port);

    if (!m_db.open()) {
        QString errorMsg = QString("Failed to connect to database '%1' as user '%2' on host '%3:%4'. Error: %5")
                               .arg(databaseName, user, host, QString::number(port), m_db.lastError().text());
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        // Clean up the connection if open failed, to allow retries with addDatabase
        // This is important because a failed QSqlDatabase::open() might leave the connection in an unusable state.
        if (QSqlDatabase::contains(m_connectionName)) { // Check again as m_db might be invalid
             QSqlDatabase::removeDatabase(m_connectionName);
        }
        return false;
    }

    qInfo() << "Successfully connected to database" << databaseName << "on" << host << "as user" << user << "with connection" << m_connectionName;
    
    // Attempt to create the chat_user table if it doesn't exist
    if (!createUsersTable()) { 
        disconnectFromDatabase(); // Disconnect if table creation fails
        return false;
    }
    return true;
}

void DatabaseManager::disconnectFromDatabase()
{
    if (m_db.isValid() && m_db.isOpen()) { // Check if m_db is valid before calling isOpen
        QString connNameToClose = m_db.connectionName(); // Get actual connection name from m_db
        m_db.close();
        qInfo() << "Database connection" << connNameToClose << "closed.";
    }
    // Always try to remove the connection by the name we intended to use,
    // as m_db might become invalid or its connectionName might change unexpectedly in rare cases.
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
        qInfo() << "Database connection" << m_connectionName << "removed.";
    }
}

bool DatabaseManager::isConnected() const
{
    // It's safer to check m_db.isValid() before m_db.isOpen()
    // as isOpen() on an invalid QSqlDatabase object can lead to issues.
    return m_db.isValid() && m_db.isOpen();
}

bool DatabaseManager::createUsersTable()
{
    if (!isConnected()) { 
        emit errorOccurred("Database not connected. Cannot create chat_user table.");
        return false;
    }
    QSqlQuery query(m_db); 
    // Table: chat_user
    // Fields: user_id (INT PK), user_pwd (VARCHAR)
    // IMPORTANT: user_pwd stores plaintext here for simplicity. HASH IT IN PRODUCTION!
    // As per new requirement: user_id is the primary key and an INT.
    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS chat_user (
            user_id INT PRIMARY KEY,
            user_pwd VARCHAR(255) NOT NULL
        );
    )";
    if (!query.exec(createTableSQL)) {
        QString errorMsg = QString("Failed to create chat_user table: %1").arg(query.lastError().text());
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }
    qInfo() << "chat_user table checked/created successfully with user_id INT PRIMARY KEY.";
    return true;
}

// The 'username' parameter now represents the user_id as a string.
bool DatabaseManager::validateUser(const QString &userIdStr, const QString &password)
{
    if (!isConnected()) {
        emit errorOccurred("Database not connected. Cannot validate user.");
        return false;
    }

    bool ok;
    int userId = userIdStr.toInt(&ok);
    if (!ok) {
        QString errorMsg = QString("Invalid User ID format: '%1' is not a valid integer.").arg(userIdStr);
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    QSqlQuery query(m_db); 
    query.prepare("SELECT user_pwd FROM chat_user WHERE user_id = :user_id");
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        QString errorMsg = QString("User validation query failed for user_id %1: %2").arg(userId).arg(query.lastError().text());
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    if (query.next()) {
        QString storedPassword = query.value(0).toString();
        // IMPORTANT: In a real application, compare hashed passwords.
        if (storedPassword == password) {
            qInfo() << "User ID" << userId << "validated successfully.";
            return true;
        } else {
            qInfo() << "Invalid password for User ID" << userId;
            return false;
        }
    }
    qInfo() << "User ID" << userId << "not found.";
    return false; // User ID not found
}

// The 'username' parameter now represents the user_id as a string.
bool DatabaseManager::addUser(const QString &userIdStr, const QString &password)
{
    if (!isConnected()) {
        emit errorOccurred("Database not connected. Cannot add user.");
        return false;
    }
    if (userIdStr.isEmpty() || password.isEmpty()) {
        emit errorOccurred("User ID or password cannot be empty.");
        return false;
    }

    bool ok;
    int userId = userIdStr.toInt(&ok);
    if (!ok) {
        QString errorMsg = QString("Invalid User ID format for add user: '%1' is not a valid integer.").arg(userIdStr);
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    // IMPORTANT: In a real application, hash the password before storing.
    QString hashedPasswordToStore = password; // Placeholder for actual hashing

    QSqlQuery query(m_db); 
    query.prepare("INSERT INTO chat_user (user_id, user_pwd) VALUES (:user_id, :password)");
    query.bindValue(":user_id", userId);
    query.bindValue(":password", hashedPasswordToStore); 

    if (!query.exec()) {
        QString errorMsg;
        // MySQL error code for duplicate entry for PRIMARY KEY is typically 1062
        if (query.lastError().nativeErrorCode().contains("1062")) { 
             errorMsg = QString("Failed to add user: User ID '%1' already exists.").arg(userId);
        } else {
             errorMsg = QString("Failed to add user with User ID '%1': %2").arg(userId).arg(query.lastError().text());
        }
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    qInfo() << "User ID" << userId << "added successfully to chat_user table.";
    return true;
}

// The 'username' parameter now represents the user_id as a string.
bool DatabaseManager::userExists(const QString &userIdStr)
{
    if (!isConnected()) {
        emit errorOccurred("Database not connected. Cannot check if user exists.");
        return false;
    }

    bool ok;
    int userId = userIdStr.toInt(&ok);
    if (!ok) {
        // This case might not be an "error" for userExists, but rather means such non-int ID cannot exist.
        // Depending on desired behavior, you could log or simply return false.
        qWarning() << "User ID check: Invalid User ID format: '" << userIdStr << "' is not an integer. Assuming does not exist.";
        return false; 
    }

    QSqlQuery query(m_db); 
    query.prepare("SELECT COUNT(*) FROM chat_user WHERE user_id = :user_id");
    query.bindValue(":user_id", userId);
    if (!query.exec()) {
        QString errorMsg = QString("Failed to check if User ID '%1' exists: %2").arg(userId).arg(query.lastError().text());
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false; // Query failed, cannot determine existence
    }
    if (query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false; // Should not happen if query executes correctly
}

bool DatabaseManager::resetPassword(const QString &userIdStr, const QString &newPassword)
{
    if (!isConnected()) {
        emit errorOccurred("Database not connected. Cannot reset password.");
        return false;
    }
    if (userIdStr.isEmpty() || newPassword.isEmpty()) {
        emit errorOccurred("User ID or new password cannot be empty for password reset.");
        return false;
    }

    bool ok;
    int userId = userIdStr.toInt(&ok);
    if (!ok) {
        QString errorMsg = QString("Invalid User ID format for password reset: '%1' is not a valid integer.").arg(userIdStr);
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    // IMPORTANT: In a real application, hash the password before storing.
    QString hashedPasswordToStore = newPassword; // Placeholder for actual hashing

    QSqlQuery query(m_db);
    query.prepare("UPDATE chat_user SET user_pwd = :password WHERE user_id = :user_id");
    query.bindValue(":password", hashedPasswordToStore);
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        QString errorMsg = QString("Failed to reset password for User ID '%1': %2").arg(userId).arg(query.lastError().text());
        qCritical() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    if (query.numRowsAffected() > 0) {
        qInfo() << "Password for User ID" << userId << "reset successfully in database.";
        return true;
    } else {
        // This case means the user_id was not found in the database,
        // though the QSettings check should ideally prevent this.
        // Or, the password was the same as the old one (some DBs might report 0 affected rows).
        QString errorMsg = QString("Password reset for User ID '%1' affected 0 rows. User might not exist or password unchanged.").arg(userId);
        qWarning() << errorMsg;
        // emit errorOccurred(errorMsg); // Decide if this is an error to show to user
        return false; // Or true if "unchanged" is acceptable. For now, false.
    }
}
