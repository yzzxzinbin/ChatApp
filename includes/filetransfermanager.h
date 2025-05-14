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
    QFile* file; // Represents the open file handle
    QString localFilePath; // Full path for sending, or chosen save path for receiving
    bool isSender;
    enum State { Idle, Offered, Accepted, Transferring, WaitingForAck, Completed, Rejected, Error } state;
    qint64 bytesTransferred;
    qint64 currentChunkID; // For sender: next chunk to send. For receiver: next chunk expected.
    qint64 totalChunks;    // Calculated when starting transfer

    FileTransferSession() : fileSize(0), file(nullptr), isSender(false), state(Idle), bytesTransferred(0), currentChunkID(0), totalChunks(0) {}
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
    void acceptFileOffer(const QString& transferID, const QString& savePath); // Modified to include savePath
    // Called by UI to reject an incoming file offer
    void rejectFileOffer(const QString& transferID, const QString& reason);

signals:
    // UI Signals
    void incomingFileOffer(const QString& transferID, const QString& peerUuid, const QString& fileName, qint64 fileSize);
    void fileTransferStarted(const QString& transferID, const QString& peerUuid, const QString& fileName, bool isSending);
    void fileTransferProgress(const QString& transferID, qint64 bytesTransferred, qint64 totalSize);
    void fileTransferFinished(const QString& transferID, const QString& peerUuid, const QString& fileName, bool success, const QString& message);
    void fileTransferError(const QString& transferID, const QString& peerUuid, const QString& errorMsg);
    void requestSavePath(const QString& transferID, const QString& fileName, qint64 fileSize, const QString& peerUuid); // New signal

private slots:
    // Internal slots for managing transfers, e.g., sending chunks, timeouts
    void processNextChunk(const QString& transferID);
    void handleTransferTimeout(const QString& transferID); // New for timeouts

private:
    NetworkManager* m_networkManager;
    QString m_localUserUuid;
    QMap<QString, FileTransferSession> m_sessions; // Key: TransferID
    QMap<QString, QTimer*> m_transferTimers; // Key: TransferID, for ACK timeouts

    QString generateTransferID() const;
    void sendFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize);
    void sendAcceptMessage(const QString& peerUuid, const QString& transferID, const QString& savePathHint); // Modified
    void sendRejectMessage(const QString& peerUuid, const QString& transferID, const QString& reason);
    void sendChunk(const QString& transferID);
    void sendDataAck(const QString& peerUuid, const QString& transferID, qint64 chunkID);
    void sendEOF(const QString& transferID);
    void sendEOFAck(const QString& peerUuid, const QString& transferID);
    void sendError(const QString& peerUuid, const QString& transferID, const QString& errorCode, const QString& errorMessage);

    void handleFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize);
    void handleFileAccept(const QString& peerUuid, const QString& transferID, const QString& savePathHint); // Modified
    void handleFileReject(const QString& peerUuid, const QString& transferID, const QString& reason);
    void handleFileChunk(const QString& peerUuid, const QString& transferID, qint64 chunkID, qint64 chunkSize, const QByteArray& data);
    void handleDataAck(const QString& peerUuid, const QString& transferID, qint64 chunkID);
    void handleEOF(const QString& peerUuid, const QString& transferID, qint64 totalChunks, const QString& finalChecksum);
    void handleEOFAck(const QString& peerUuid, const QString& transferID);
    void handleFileError(const QString& peerUuid, const QString& transferID, const QString& errorCode, const QString& message);
    
    // Placeholder for actual data sending/receiving logic
    void startActualFileSend(const QString& transferID);
    void prepareToReceiveFile(const QString& transferID, const QString& savePath);

    // Helper to extract attribute from simple XML-like string (can be moved to a utility class later)
    QString extractMessageAttribute(const QString& message, const QString& attributeName) const;
    void cleanupSession(const QString& transferID, bool success, const QString& message);
    void startAckTimer(const QString& transferID);
    void stopAckTimer(const QString& transferID);
};

#endif // FILETRANSFERMANAGER_H
