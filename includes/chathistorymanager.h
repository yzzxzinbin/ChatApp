#ifndef CHATHISTORYMANAGER_H
#define CHATHISTORYMANAGER_H

#include <QObject>
#include <QStringList>
#include <QString>

class ChatHistoryManager : public QObject
{
    Q_OBJECT
public:
    // 构造函数接收 appName/userId 格式的字符串
    explicit ChatHistoryManager(const QString &appNameAndUserId, QObject *parent = nullptr);

    QStringList loadChatHistory(const QString &peerUuid);
    bool saveChatHistory(const QString &peerUuid, const QStringList &history);
    void clearChatHistory(const QString &peerUuid);    // 确保声明存在
    void clearAllChatHistory(); // 确保声明存在 (如果需要)

private:
    QString m_appNameAndUserId; // 存储传入的 "AppName/UserId"
    QString m_userSpecificChatHistoryBasePath; // 用户特定的聊天记录基础路径

    void initializeChatHistoryDir();
    QString getPeerChatHistoryFilePath(const QString &peerUuid) const;
};

#endif // CHATHISTORYMANAGER_H
