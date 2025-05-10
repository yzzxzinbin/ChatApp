#include "chathistorymanager.h"

ChatHistoryManager::ChatHistoryManager(const QString& appName, QObject *parent)
    : QObject(parent), applicationName(appName)
{
    initializeChatHistoryDir();
}

void ChatHistoryManager::initializeChatHistoryDir()
{
    // Use QStandardPaths with the applicationName passed to the constructor
    chatHistoryBaseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    if (chatHistoryBaseDir.isEmpty()) {
        qWarning() << "ChatHistoryManager: Could not determine AppLocalDataLocation for chat history using appName:" << applicationName;
        // Fallback to application directory if AppLocalDataLocation is not available
        // This path might not be specific to the instance if applicationName is just "ChatApp"
        // but QCoreApplication::applicationName() should be instance-specific (e.g., "ChatApp_A")
        chatHistoryBaseDir = QCoreApplication::applicationDirPath();
        qWarning() << "ChatHistoryManager: Falling back to applicationDirPath:" << chatHistoryBaseDir;
    }
    chatHistoryBaseDir += "/ChatHistory";

    QDir dir(chatHistoryBaseDir);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            qInfo() << "ChatHistoryManager: Created chat history directory:" << chatHistoryBaseDir;
        } else {
            qWarning() << "ChatHistoryManager: Could not create chat history directory:" << chatHistoryBaseDir;
        }
    } else {
        qInfo() << "ChatHistoryManager: Chat history directory already exists:" << chatHistoryBaseDir;
    }
}

QString ChatHistoryManager::getPeerChatHistoryFilePath(const QString& peerUuid) const
{
    if (chatHistoryBaseDir.isEmpty() || peerUuid.isEmpty()) {
        return QString(); // Return empty string for invalid path
    }
    return chatHistoryBaseDir + "/" + peerUuid + ".chdat";
}

bool ChatHistoryManager::saveChatHistory(const QString& peerUuid, const QStringList& history)
{
    if (peerUuid.isEmpty()) {
        qWarning() << "ChatHistoryManager::saveChatHistory: Invalid peerUuid.";
        return false;
    }

    QString filePath = getPeerChatHistoryFilePath(peerUuid);
    if (filePath.isEmpty()) {
        qWarning() << "ChatHistoryManager::saveChatHistory: Could not get valid file path for peer" << peerUuid;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "ChatHistoryManager::saveChatHistory: Could not open file for writing:" << filePath << "Error:" << file.errorString();
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_5); // Or your Qt version
    out << history;

    file.close();
    qInfo() << "ChatHistoryManager: Chat history saved for peer" << peerUuid << "to" << filePath;
    return true;
}

QStringList ChatHistoryManager::loadChatHistory(const QString& peerUuid)
{
    if (peerUuid.isEmpty()) {
        return QStringList();
    }

    QString filePath = getPeerChatHistoryFilePath(peerUuid);
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        qInfo() << "ChatHistoryManager::loadChatHistory: No history file found for peer" << peerUuid << "at" << filePath;
        return QStringList(); // File doesn't exist, return empty list
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ChatHistoryManager::loadChatHistory: Could not open file for reading:" << filePath << "Error:" << file.errorString();
        return QStringList(); // Open failed, return empty list
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_5); // Or your Qt version
    QStringList historyList;
    in >> historyList;

    file.close();

    if (in.status() != QDataStream::Ok) {
        qWarning() << "ChatHistoryManager::loadChatHistory: Error reading data stream for peer" << peerUuid << "from" << filePath;
        return QStringList(); // Read error, return empty list
    }

    qInfo() << "ChatHistoryManager: Chat history loaded for peer" << peerUuid << "from" << filePath << "Messages count:" << historyList.count();
    return historyList;
}
