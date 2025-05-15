#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QObject>
#include <QMap>
#include <QFile>
#include <QTimer>
#include <QSet> // For receivedOutOfOrderChunks keys
#include <QElapsedTimer> // Include QElapsedTimer
#include "fileiomanager.h" // <-- Include FileIOManager

class NetworkManager; // Forward declaration

// Sliding Window Configuration
const int DEFAULT_SEND_WINDOW_SIZE = 24; // Send up to 5 chunks before waiting for ACK for the first one
const int DEFAULT_RECEIVE_WINDOW_SIZE = 32; // Receiver can buffer up to 10 out-of-order chunks
const int FT_CHUNK_RETRANSMISSION_TIMEOUT_MS = 10000; // Timeout for retransmitting the base of the send window
const int MAX_CONCURRENT_READS_PER_TRANSFER = 8;  // Example limit
const int MAX_CONCURRENT_WRITES_PER_TRANSFER = 12; // Example limit

struct FileTransferSession {
    QString transferID;
    QString peerUuid;
    QString fileName;
    qint64 fileSize;
    QString localFilePath; // Full path for sending, or chosen save path for receiving
    bool isSender;
    enum State { Idle, Offered, Accepted, Transferring, WaitingForAck, Completed, Rejected, Error, Paused } state; // Added Paused
    qint64 bytesTransferred; // For sender: bytes ACKed. For receiver: bytes written to disk.
    qint64 totalChunks;    // Calculated when starting transfer

    // Sender specific for Sliding Window
    qint64 sendWindowBase;          // Sequence number of the oldest unacknowledged chunk
    qint64 nextChunkToSendInWindow; // Sequence number of the next new chunk to send within the current window pass
    QTimer* retransmissionTimer;    // Timer for retransmitting sendWindowBase if not ACKed

    // Receiver specific for Sliding Window
    qint64 highestContiguousChunkReceived; // Highest chunk ID received and written in order
    QMap<qint64, QByteArray> receivedOutOfOrderChunks; // Buffer for out-of-order chunks

    FileTransferSession() : 
        fileSize(0), isSender(false), state(Idle), bytesTransferred(0), 
        totalChunks(0), sendWindowBase(0), nextChunkToSendInWindow(0), 
        retransmissionTimer(nullptr), highestContiguousChunkReceived(-1) {}

    // Helper to clean up timer
    void stopAndClearRetransmissionTimer() {
        if (retransmissionTimer) {
            retransmissionTimer->stop();
            delete retransmissionTimer;
            retransmissionTimer = nullptr;
        }
    }
    ~FileTransferSession() {
        stopAndClearRetransmissionTimer();
        // QFile* file is no longer part of the session directly
    }
};

class FileTransferManager : public QObject
{
    Q_OBJECT
public:
    explicit FileTransferManager(NetworkManager* networkManager, FileIOManager* fileIOManager, const QString& localUserUuid, QObject *parent = nullptr); // Added FileIOManager
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
    void processSendQueue(const QString& transferID); // Renamed from processNextChunk, drives sending multiple chunks
    void handleChunkRetransmissionTimeout(const QString& transferID); // Renamed from handleTransferTimeout

    // New slots for FileIOManager signals
    void handleChunkReadForSending(const QString& transferID, qint64 chunkID, const QByteArray& data, bool success, const QString& error);
    void handleChunkWritten(const QString& transferID, qint64 chunkID, qint64 bytesWritten, bool success, const QString& error);

private:
    NetworkManager* m_networkManager;
    FileIOManager* m_fileIOManager; // <-- Add FileIOManager instance
    QString m_localUserUuid;
    QMap<QString, FileTransferSession> m_sessions; // Key: TransferID
    QMap<QString, int> m_outstandingReadRequests; // transferID -> count
    QMap<QString, int> m_outstandingWriteRequests; // transferID -> count

    // 集中ACK相关成员
    QMap<QString, int> m_pendingAckCount; // transferID -> 当前未发送的ACK计数
    QMap<QString, QTimer*> m_ackDelayTimers; // transferID -> ACK延迟定时器

    // 新增：传输计时器
    QMap<QString, QElapsedTimer*> m_transferTimers; // transferID -> QElapsedTimer*

    QString generateTransferID() const;
    void sendFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize);
    void sendAcceptMessage(const QString& peerUuid, const QString& transferID, const QString& savePathHint); // Modified
    void sendRejectMessage(const QString& peerUuid, const QString& transferID, const QString& reason);
    void sendChunkData(const QString& transferID, qint64 chunkID, const QByteArray& chunkData); // New helper
    void sendDataAck(const QString& peerUuid, const QString& transferID, qint64 ackedChunkID); // ackedChunkID is the highest contiguous received
    void sendEOF(const QString& transferID);
    void sendEOFAck(const QString& peerUuid, const QString& transferID);
    void sendError(const QString& peerUuid, const QString& transferID, const QString& errorCode, const QString& errorMessage);

    void handleFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize);
    void handleFileAccept(const QString& peerUuid, const QString& transferID, const QString& savePathHint); // Modified
    void handleFileReject(const QString& peerUuid, const QString& transferID, const QString& reason);
    void handleFileChunk(const QString& peerUuid, const QString& transferID, qint64 chunkID, qint64 chunkSize, const QByteArray& data);
    void handleDataAck(const QString& peerUuid, const QString& transferID, qint64 ackedChunkID); // ackedChunkID is the highest contiguous received by peer
    void handleEOF(const QString& peerUuid, const QString& transferID, qint64 totalChunks, const QString& finalChecksum);
    void handleEOFAck(const QString& peerUuid, const QString& transferID);
    void handleFileError(const QString& peerUuid, const QString& transferID, const QString& errorCode, const QString& message);
    
    // Placeholder for actual data sending/receiving logic
    void startActualFileSend(const QString& transferID);
    void prepareToReceiveFile(const QString& transferID, const QString& savePath);

    // Helper to extract attribute from simple XML-like string (can be moved to a utility class later)
    QString extractMessageAttribute(const QString& message, const QString& attributeName) const;
    void cleanupSession(const QString& transferID, bool success, const QString& message);
    void startRetransmissionTimer(const QString& transferID);
    void stopRetransmissionTimer(const QString& transferID);

    // Helper for receiver to process buffered chunks
    void processBufferedChunks(const QString& transferID);
};

#endif // FILETRANSFERMANAGER_H
