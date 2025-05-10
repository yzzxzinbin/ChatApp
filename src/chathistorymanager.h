#ifndef CHATHISTORYMANAGER_H
#define CHATHISTORYMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QCoreApplication> // Required for applicationDirPath as fallback

class ChatHistoryManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatHistoryManager(const QString& appName, QObject *parent = nullptr);

    // Saves the given chat history for the peer.
    // Returns true on success, false otherwise.
    bool saveChatHistory(const QString& peerUuid, const QStringList& history);

    // Loads chat history for the peer.
    // Returns the list of messages, or an empty list if not found or error.
    QStringList loadChatHistory(const QString& peerUuid);

private:
    void initializeChatHistoryDir();
    QString getPeerChatHistoryFilePath(const QString& peerUuid) const;

    QString chatHistoryBaseDir;
    QString applicationName; // Stores the application name for path construction
};

#endif // CHATHISTORYMANAGER_H
