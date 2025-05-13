#include "chathistorymanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDataStream> // 用于二进制读写历史记录
#include <QTextStream> // 如果之前用的是文本流，请保持一致或迁移
#include <QDebug>
#include <QCoreApplication>
#include <QCryptographicHash> // 用于生成备用路径

ChatHistoryManager::ChatHistoryManager(const QString &appNameAndUserId, QObject *parent)
    : QObject(parent), m_appNameAndUserId(appNameAndUserId)
{
    initializeChatHistoryDir();
}

void ChatHistoryManager::initializeChatHistoryDir()
{
    QString baseAppPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (baseAppPath.isEmpty()) {
        qWarning() << "ChatHistoryManager: Could not determine AppLocalDataLocation.";
        // Fallback, e.g. to application directory + data (less ideal for user-specific data)
        baseAppPath = QCoreApplication::applicationDirPath() + "/UserData";
    }

    QString appNameFromCore = QCoreApplication::applicationName(); // e.g., "ChatApp"
    QString userIdPart = m_appNameAndUserId;

    if (userIdPart.startsWith(appNameFromCore + "/")) {
        userIdPart.remove(0, appNameFromCore.length() + 1); // Extracts "userId" from "AppName/userId"
    } else {
        qWarning() << "ChatHistoryManager: appNameAndUserId format unexpected:" << m_appNameAndUserId << ". Using a hash for user directory.";
        // Fallback to a hash if the format is not as expected, to ensure some separation
        userIdPart = QCryptographicHash::hash(m_appNameAndUserId.toUtf8(), QCryptographicHash::Md5).toHex();
    }

    if (userIdPart.isEmpty()) {
        qWarning() << "ChatHistoryManager: Extracted User ID part is empty. Defaulting to 'default_user'.";
        userIdPart = "default_user"; // Prevent empty directory segment
    }

    // Construct user-specific path: AppLocalDataLocation/ExtractedUserId/ChatHistory
    m_userSpecificChatHistoryBasePath = baseAppPath + "/" + userIdPart + "/ChatHistory";

    QDir dir(m_userSpecificChatHistoryBasePath);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            qInfo() << "ChatHistoryManager: Created chat history directory:" << m_userSpecificChatHistoryBasePath;
        } else {
            qWarning() << "ChatHistoryManager: Could not create chat history directory:" << m_userSpecificChatHistoryBasePath;
        }
    } else {
        qInfo() << "ChatHistoryManager: Chat history directory already exists:" << m_userSpecificChatHistoryBasePath;
    }
}

QString ChatHistoryManager::getPeerChatHistoryFilePath(const QString& peerUuid) const
{
    if (m_userSpecificChatHistoryBasePath.isEmpty() || peerUuid.isEmpty()) {
        qWarning() << "ChatHistoryManager: Base path or peer UUID is empty. Cannot form file path.";
        return QString();
    }
    // 使用 .chdat 后缀，与您提供的 load/save 逻辑一致
    return m_userSpecificChatHistoryBasePath + "/" + peerUuid + ".chdat";
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
    // 使用 QIODevice::Truncate 来覆盖旧文件，如果这是期望行为
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ChatHistoryManager::saveChatHistory: Could not open file for writing:" << filePath << "Error:" << file.errorString();
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_5); // 与加载时版本保持一致
    out << history;

    file.close();
    if (out.status() != QDataStream::Ok) {
         qWarning() << "ChatHistoryManager::saveChatHistory: Error writing data stream for peer" << peerUuid << "to" << filePath;
         return false;
    }
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
        // qInfo() << "ChatHistoryManager::loadChatHistory: No history file found for peer" << peerUuid << "at" << filePath;
        return QStringList(); // File doesn't exist, return empty list
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ChatHistoryManager::loadChatHistory: Could not open file for reading:" << filePath << "Error:" << file.errorString();
        return QStringList(); // Open failed, return empty list
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_5); // 与保存时版本保持一致
    QStringList historyList;
    in >> historyList;

    file.close();

    if (in.status() != QDataStream::Ok) {
        qWarning() << "ChatHistoryManager::loadChatHistory: Error reading data stream for peer" << peerUuid << "from" << filePath;
        return QStringList(); // Read error, return empty list
    }

    // qInfo() << "ChatHistoryManager: Chat history loaded for peer" << peerUuid << "from" << filePath << "Messages count:" << historyList.count();
    return historyList;
}

// 实现 clearChatHistory
void ChatHistoryManager::clearChatHistory(const QString &peerUuid)
{
    if (peerUuid.isEmpty()) {
        qWarning() << "ChatHistoryManager::clearChatHistory: Invalid peerUuid.";
        return;
    }
    QString filePath = getPeerChatHistoryFilePath(peerUuid);
    if (filePath.isEmpty()) {
        qWarning() << "ChatHistoryManager::clearChatHistory: Could not get valid file path for peer" << peerUuid;
        return;
    }

    QFile file(filePath);
    if (file.exists()) {
        if (file.remove()) {
            qInfo() << "ChatHistoryManager: Successfully deleted chat history file for peer" << peerUuid << "at" << filePath;
        } else {
            qWarning() << "ChatHistoryManager: Failed to delete chat history file for peer" << peerUuid << "at" << filePath << "Error:" << file.errorString();
        }
    } else {
        qInfo() << "ChatHistoryManager::clearChatHistory: No history file to delete for peer" << peerUuid << "at" << filePath;
    }
}

// 实现 clearAllChatHistory (如果需要)
void ChatHistoryManager::clearAllChatHistory()
{
    if (m_userSpecificChatHistoryBasePath.isEmpty()) {
        qWarning() << "ChatHistoryManager::clearAllChatHistory: Base path is not initialized.";
        return;
    }
    QDir dir(m_userSpecificChatHistoryBasePath);
    if (dir.exists()) {
        // 获取所有 .chdat 文件并删除
        QStringList nameFilters;
        nameFilters << "*.chdat";
        QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files);
        for (const QFileInfo &fileInfo : files) {
            QFile::remove(fileInfo.absoluteFilePath());
        }
        qInfo() << "ChatHistoryManager: Cleared all chat history files from" << m_userSpecificChatHistoryBasePath;
    }
}