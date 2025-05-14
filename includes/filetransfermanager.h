#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QObject>
#include <QMap>
#include <QFile>
#include <QTimer>

class NetworkManager; // Forward declaration

struct FileTransferSession {
    QString transferID;
    QString peerUuid;
    QString fileName;
    qint64 fileSize;
    QFile* file;
    bool isSender;
    enum State { Idle, Offered, Accepted, Transferring, Completed, Rejected, Error } state;
    qint64 bytesTransferred;
    // Add more fields as needed: checksum, chunks, etc.

    FileTransferSession() : fileSize(0), file(nullptr), isSender(false), state(Idle), bytesTransferred(0) {}
};

class FileTransferManager : public QObject
{
    Q_OBJECT
public:
    explicit FileTransferManager(NetworkManager* networkManager, const QString& localUserUuid, QObject *parent = nullptr);
    ~FileTransferManager();

    // Called by UI to initiate sending a file
    QString requestSendFile(const QString& peerUuid, const QString& filePath);

    // Called by NetworkEventHandler when a file transfer message is received
    void handleIncomingFileMessage(const QString& peerUuid, const QString& message);

    // Called by UI to accept an incoming file offer
    void acceptFileOffer(const QString& transferID);
    // Called by UI to reject an incoming file offer
    void rejectFileOffer(const QString& transferID, const QString& reason);

signals:
    // UI Signals
    void incomingFileOffer(const QString& transferID, const QString& peerUuid, const QString& fileName, qint64 fileSize);
    void fileTransferStarted(const QString& transferID, const QString& peerUuid, const QString& fileName, bool isSending);
    void fileTransferProgress(const QString& transferID, qint64 bytesTransferred, qint64 totalSize);
    void fileTransferFinished(const QString& transferID, const QString& peerUuid, const QString& fileName, bool success, const QString& message);
    void fileTransferError(const QString& transferID, const QString& peerUuid, const QString& errorMsg);

private slots:
    // Internal slots for managing transfers, e.g., sending chunks, timeouts
    void processNextChunk(const QString& transferID);

private:
    NetworkManager* m_networkManager;
    QString m_localUserUuid;
    QMap<QString, FileTransferSession> m_sessions; // Key: TransferID

    QString generateTransferID() const;
    void sendFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize);
    void sendAcceptMessage(const QString& peerUuid, const QString& transferID);
    void sendRejectMessage(const QString& peerUuid, const QString& transferID, const QString& reason);

    void handleFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize);
    void handleFileAccept(const QString& peerUuid, const QString& transferID);
    void handleFileReject(const QString& peerUuid, const QString& transferID, const QString& reason);
    
    // Placeholder for actual data sending/receiving logic
    void startActualFileSend(const QString& transferID);
    void prepareToReceiveFile(const QString& transferID);

    // Helper to extract attribute from simple XML-like string (can be moved to a utility class later)
    QString extractMessageAttribute(const QString& message, const QString& attributeName) const;
};

#endif // FILETRANSFERMANAGER_H
