#include "filetransfermanager.h"
#include "networkmanager.h"
#include "fileiomanager.h" // Make sure this is included
#include <QUuid>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QBuffer>
#include <QElapsedTimer>

// Helper function to extract attribute (can be moved to a shared utility if NetworkManager's one is not accessible/suitable)
QString FileTransferManager::extractMessageAttribute(const QString& message, const QString& attributeName) const {
    QRegularExpression regex(QStringLiteral("%1=\\\"([^\\\"]*)\\\"").arg(attributeName));
    QRegularExpressionMatch match = regex.match(message);
    if (match.hasMatch() && match.capturedTexts().size() > 1) {
        return match.captured(1);
    }
    return QString();
}

// 集中ACK参数
const int ACK_BATCH_SIZE = 4;      // 每收到4个新chunk就ACK一次
const int ACK_DELAY_MS = 10;      // 或每100ms至少ACK一次

FileTransferManager::FileTransferManager(NetworkManager* networkManager, FileIOManager* fileIOManager, const QString& localUserUuid, QObject *parent)
    : QObject(parent), m_networkManager(networkManager), m_fileIOManager(fileIOManager), m_localUserUuid(localUserUuid)
{
    if (!m_networkManager) {
        qCritical() << "FileTransferManager initialized with a null NetworkManager!";
    }
    if (!m_fileIOManager) {
        qCritical() << "FileTransferManager initialized with a null FileIOManager!";
    }

    // Connect signals from FileIOManager
    // 更新以匹配新的信号签名
    connect(m_fileIOManager, &FileIOManager::chunkReadCompleted, this, &FileTransferManager::handleChunkReadForSending);
    connect(m_fileIOManager, &FileIOManager::chunkWrittenCompleted, this, &FileTransferManager::handleChunkWritten);
}

FileTransferManager::~FileTransferManager()
{
    // Clean up any active sessions
    m_sessions.clear();
    m_outstandingReadRequests.clear();
    m_outstandingWriteRequests.clear();

    // 清理ACK定时器
    for (auto timer : m_ackDelayTimers) {
        timer->stop();
        timer->deleteLater();
    }
    m_ackDelayTimers.clear();
    m_pendingAckCount.clear();

    // 清理传输计时器
    for (auto timer : m_transferTimers) {
        delete timer;
    }
    m_transferTimers.clear();
}

QString FileTransferManager::generateTransferID() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString FileTransferManager::requestSendFile(const QString& peerUuid, const QString& filePath)
{
    if (!m_networkManager) {
        qWarning() << "FileTransferManager::requestSendFile: NetworkManager is not available.";
        return QString();
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "FileTransferManager::requestSendFile: File does not exist or is not a file:" << filePath;
        emit fileTransferError("", peerUuid, tr("File not found or is invalid: %1").arg(filePath));
        return QString();
    }

    QString transferID = generateTransferID();
    FileTransferSession session;
    session.transferID = transferID;
    session.peerUuid = peerUuid;
    session.fileName = fileInfo.fileName();
    session.fileSize = fileInfo.size();
    session.isSender = true;
    session.state = FileTransferSession::Offered;
    session.localFilePath = filePath;
    session.totalChunks = (session.fileSize + DEFAULT_CHUNK_SIZE - 1) / DEFAULT_CHUNK_SIZE;

    m_sessions.insert(transferID, session);

    sendFileOffer(peerUuid, transferID, session.fileName, session.fileSize);
    qInfo() << "FileTransferManager: Requested to send file" << session.fileName << "to" << peerUuid << "TransferID:" << transferID;
    return transferID;
}

void FileTransferManager::sendFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize)
{
    QString offerMessage = FT_MSG_OFFER_FORMAT.arg(transferID).arg(fileName).arg(fileSize).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, offerMessage);
    qDebug() << "FileTransferManager: Sent file offer to" << peerUuid << "TransferID:" << transferID << "FileName:" << fileName << "Size:" << fileSize;
}

void FileTransferManager::handleIncomingFileMessage(const QString& peerUuid, const QString& message)
{
    
    qDebug() << "FileTransferManager::handleIncomingFileMessage from" << peerUuid << "Type:" << (message.left(20));

    if (message.startsWith("<FT_OFFER")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        QString fileName = extractMessageAttribute(message, "FileName");
        qint64 fileSize = extractMessageAttribute(message, "FileSize").toLongLong();
        QString senderUuid = extractMessageAttribute(message, "SenderUUID");

        if (transferID.isEmpty() || fileName.isEmpty() || senderUuid.isEmpty() || senderUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_OFFER received:" << message;
            return;
        }
        handleFileOffer(peerUuid, transferID, fileName, fileSize);

    } else if (message.startsWith("<FT_ACCEPT")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        QString receiverUuid = extractMessageAttribute(message, "ReceiverUUID");
        QString savePathHint = extractMessageAttribute(message, "SavePathHint");
        if (transferID.isEmpty() || receiverUuid.isEmpty() || receiverUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_ACCEPT received:" << message;
            return;
        }
        handleFileAccept(peerUuid, transferID, savePathHint);

    } else if (message.startsWith("<FT_REJECT")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        QString reason = extractMessageAttribute(message, "Reason");
        QString receiverUuid = extractMessageAttribute(message, "ReceiverUUID");
        if (transferID.isEmpty() || receiverUuid.isEmpty() || receiverUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_REJECT received:" << message;
            return;
        }
        handleFileReject(peerUuid, transferID, reason);
    } else if (message.startsWith("<FT_CHUNK")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        qint64 chunkID = extractMessageAttribute(message, "ChunkID").toLongLong();
        qint64 originalChunkSize = extractMessageAttribute(message, "Size").toLongLong(); // This is the original binary size
        QString dataB64 = extractMessageAttribute(message, "Data");
        // QByteArray data = QByteArray::fromBase64(dataB64.toUtf8()); // 解码移至FileIOManager

        if (transferID.isEmpty() || dataB64.isEmpty()) { // 移除了 data.size() != chunkSize 的检查
            qWarning() << "FileTransferManager: Invalid FT_CHUNK received (empty ID or dataB64):" << message;
            sendError(peerUuid, transferID, "CHUNK_INVALID", "Received invalid chunk data (empty ID or data).");
            return;
        }
        handleFileChunk(peerUuid, transferID, chunkID, originalChunkSize, dataB64); // 传递QString dataB64 和 originalChunkSize
    } else if (message.startsWith("<FT_ACK_DATA")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        qint64 ackedChunkID = extractMessageAttribute(message, "ChunkID").toLongLong();
        QString ackingPeerUuid = extractMessageAttribute(message, "ReceiverUUID");
        if (transferID.isEmpty() || ackingPeerUuid.isEmpty() || ackingPeerUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_ACK_DATA received:" << message;
            return;
        }
        handleDataAck(peerUuid, transferID, ackedChunkID);
    } else if (message.startsWith("<FT_EOF")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        qint64 totalChunks = extractMessageAttribute(message, "TotalChunks").toLongLong();
        QString finalChecksum = extractMessageAttribute(message, "FinalChecksum");
        if (transferID.isEmpty()) {
            qWarning() << "FileTransferManager: Invalid FT_EOF received:" << message;
            return;
        }
        handleEOF(peerUuid, transferID, totalChunks, finalChecksum);
    } else if (message.startsWith("<FT_ACK_EOF")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        QString ackingPeerUuid = extractMessageAttribute(message, "ReceiverUUID");
        if (transferID.isEmpty() || ackingPeerUuid.isEmpty() || ackingPeerUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_ACK_EOF received:" << message;
            return;
        }
        handleEOFAck(peerUuid, transferID);
    } else if (message.startsWith("<FT_ERROR")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        QString errorCode = extractMessageAttribute(message, "Code");
        QString errorMsg = extractMessageAttribute(message, "Message");
        QString originatorUuid = extractMessageAttribute(message, "OriginatorUUID");
        if (transferID.isEmpty() || originatorUuid.isEmpty() || originatorUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_ERROR received:" << message;
            return;
        }
        handleFileError(peerUuid, transferID, errorCode, errorMsg);
    }
}

void FileTransferManager::handleFileOffer(const QString& peerUuid, const QString& transferID, const QString& fileName, qint64 fileSize)
{
    if (m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager: Duplicate file offer for TransferID" << transferID << ". Ignoring.";
        return;
    }

    FileTransferSession session;
    session.transferID = transferID;
    session.peerUuid = peerUuid;
    session.fileName = fileName;
    session.fileSize = fileSize;
    session.isSender = false;
    session.state = FileTransferSession::Offered;
    session.totalChunks = (fileSize + DEFAULT_CHUNK_SIZE - 1) / DEFAULT_CHUNK_SIZE;
    m_sessions.insert(transferID, session);

    qInfo() << "FileTransferManager: Received file offer for" << fileName << "from" << peerUuid << "TransferID:" << transferID;
    emit incomingFileOffer(transferID, peerUuid, fileName, fileSize);
}

void FileTransferManager::acceptFileOffer(const QString& transferID, const QString& savePath)
{
    if (!m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager::acceptFileOffer: Unknown TransferID" << transferID;
        return;
    }
    FileTransferSession& session = m_sessions[transferID];
    if (session.isSender || session.state != FileTransferSession::Offered) {
        qWarning() << "FileTransferManager::acceptFileOffer: Invalid state for TransferID" << transferID;
        return;
    }
    session.localFilePath = savePath;
    session.state = FileTransferSession::Accepted;
    sendAcceptMessage(session.peerUuid, transferID, savePath);
    qInfo() << "FileTransferManager: Accepted file offer for TransferID" << transferID << "from" << session.peerUuid << "Saving to:" << savePath;

    prepareToReceiveFile(transferID, savePath);
}

void FileTransferManager::rejectFileOffer(const QString& transferID, const QString& reason)
{
    if (!m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager::rejectFileOffer: Unknown TransferID" << transferID;
        return;
    }
    FileTransferSession& session = m_sessions[transferID];
    if (session.isSender || session.state != FileTransferSession::Offered) {
        qWarning() << "FileTransferManager::rejectFileOffer: Invalid state for TransferID" << transferID;
        return;
    }

    session.state = FileTransferSession::Rejected;
    sendRejectMessage(session.peerUuid, transferID, reason);
    qInfo() << "FileTransferManager: Rejected file offer for TransferID" << transferID << "from" << session.peerUuid << "Reason:" << reason;
    cleanupSession(transferID, false, tr("Rejected by user: %1").arg(reason));
}

void FileTransferManager::sendAcceptMessage(const QString& peerUuid, const QString& transferID, const QString& savePathHint)
{
    QString acceptMessage = FT_MSG_ACCEPT_FORMAT.arg(transferID).arg(m_localUserUuid).arg(savePathHint);
    m_networkManager->sendMessage(peerUuid, acceptMessage);
    qDebug() << "FileTransferManager: Sent file accept to" << peerUuid << "TransferID:" << transferID;
}

void FileTransferManager::sendRejectMessage(const QString& peerUuid, const QString& transferID, const QString& reason)
{
    QString rejectMessage = FT_MSG_REJECT_FORMAT.arg(transferID).arg(reason).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, rejectMessage);
    qDebug() << "FileTransferManager: Sent file reject to" << peerUuid << "TransferID:" << transferID << "Reason:" << reason;
}

void FileTransferManager::handleFileAccept(const QString& peerUuid, const QString& transferID, const QString& savePathHint)
{
    Q_UNUSED(savePathHint);
    if (!m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager::handleFileAccept: Unknown TransferID" << transferID;
        return;
    }
    FileTransferSession& session = m_sessions[transferID];
    if (!session.isSender || session.state != FileTransferSession::Offered) {
        qWarning() << "FileTransferManager::handleFileAccept: Invalid state for TransferID" << transferID;
        return;
    }
    if (session.peerUuid != peerUuid) {
        qWarning() << "FileTransferManager::handleFileAccept: Peer UUID mismatch for TransferID" << transferID;
        return;
    }

    session.state = FileTransferSession::Accepted;
    qInfo() << "FileTransferManager: File offer accepted by" << peerUuid << "for TransferID" << transferID;

    startActualFileSend(transferID);
}

void FileTransferManager::handleFileReject(const QString& peerUuid, const QString& transferID, const QString& reason)
{
    if (!m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager::handleFileReject: Unknown TransferID" << transferID;
        return;
    }
    FileTransferSession& session = m_sessions[transferID];
    if (!session.isSender || session.state != FileTransferSession::Offered) {
        qWarning() << "FileTransferManager::handleFileReject: Invalid state for TransferID" << transferID;
        return;
    }
    if (session.peerUuid != peerUuid) {
        qWarning() << "FileTransferManager::handleFileReject: Peer UUID mismatch for TransferID" << transferID;
        return;
    }

    session.state = FileTransferSession::Rejected;
    qInfo() << "FileTransferManager: File offer rejected by" << peerUuid << "for TransferID" << transferID << "Reason:" << reason;
    cleanupSession(transferID, false, tr("Rejected by peer: %1").arg(reason));
}

void FileTransferManager::startActualFileSend(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (session.localFilePath.isEmpty()) {
        qWarning() << "FileTransferManager: No local file path for sending session" << transferID;
        cleanupSession(transferID, false, tr("Internal error: File path missing."));
        sendError(session.peerUuid, transferID, "INTERNAL_ERROR", "File path missing for sender.");
        return;
    }

    session.state = FileTransferSession::Transferring;
    session.sendWindowBase = 0;
    session.nextChunkToSendInWindow = 0;
    session.bytesTransferred = 0;
    m_outstandingReadRequests[transferID] = 0;

    // 启动传输计时器
    if (!m_transferTimers.contains(transferID)) {
        QElapsedTimer* timer = new QElapsedTimer();
        timer->start();
        m_transferTimers[transferID] = timer;
        qInfo() << "[FTM] Transfer" << transferID << "timer started.";
    }

    qInfo() << "FileTransferManager: Starting to send file" << session.fileName << "for TransferID" << transferID;
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, true);
    processSendQueue(transferID);
}

void FileTransferManager::prepareToReceiveFile(const QString& transferID, const QString& savePath) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    session.state = FileTransferSession::Transferring;
    session.highestContiguousChunkReceived = -1;
    session.receivedOutOfOrderChunks.clear();
    session.bytesTransferred = 0;
    m_outstandingWriteRequests[transferID] = 0;

    m_pendingAckCount[transferID] = 0; // 初始化ACK计数器

    // 启动传输计时器
    if (!m_transferTimers.contains(transferID)) {
        QElapsedTimer* timer = new QElapsedTimer();
        timer->start();
        m_transferTimers[transferID] = timer;
        qInfo() << "[FTM] Transfer" << transferID << "timer started (receiver).";
    }

    qInfo() << "FileTransferManager: Preparing to receive file" << session.fileName << "for TransferID" << transferID << "to" << session.localFilePath;
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, false);
}

void FileTransferManager::processSendQueue(const QString& transferID) {
    if (!m_sessions.contains(transferID) || !m_fileIOManager) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender || session.state != FileTransferSession::Transferring) {
        return;
    }

    while (session.nextChunkToSendInWindow < session.sendWindowBase + DEFAULT_SEND_WINDOW_SIZE &&
           session.nextChunkToSendInWindow < session.totalChunks &&
           m_outstandingReadRequests.value(transferID, 0) < MAX_CONCURRENT_READS_PER_TRANSFER) {
        
        qint64 currentChunkID = session.nextChunkToSendInWindow;
        qint64 offset = currentChunkID * DEFAULT_CHUNK_SIZE;
        
        qDebug() << "FileTransferManager: Requesting read for chunk" << currentChunkID << "for" << transferID;
        m_fileIOManager->requestReadFileChunk(transferID, currentChunkID, session.localFilePath, offset, DEFAULT_CHUNK_SIZE);
        
        m_outstandingReadRequests[transferID]++;
        session.nextChunkToSendInWindow++;
    }

    qInfo() << "[FTM] processSendQueue: transferID=" << transferID
            << "sendWindowBase=" << session.sendWindowBase
            << "nextChunkToSendInWindow=" << session.nextChunkToSendInWindow
            << "outstandingReads=" << m_outstandingReadRequests.value(transferID, 0);
}

void FileTransferManager::handleChunkReadForSending(const QString& transferID, qint64 chunkID, const QString& dataB64, qint64 originalSize, bool success, const QString& error) {
    if (!m_sessions.contains(transferID)) {
        if (m_outstandingReadRequests.contains(transferID)) m_outstandingReadRequests[transferID]--;
        return;
    }
    m_outstandingReadRequests[transferID]--;
    FileTransferSession& session = m_sessions[transferID];

    if (!success) {
        qWarning() << "FileTransferManager: Failed to read chunk" << chunkID << "for" << transferID << ":" << error;
        sendError(session.peerUuid, transferID, "FILE_READ_ERROR_ASYNC", error);
        cleanupSession(transferID, false, tr("File read error: %1").arg(error));
        return;
    }

    if (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::WaitingForAck) {
         qWarning() << "FileTransferManager::handleChunkReadForSending: Session" << transferID << "not in transferable state. Chunk" << chunkID;
         return;
    }
    
    if (chunkID < session.sendWindowBase) {
        qDebug() << "FileTransferManager: Ignoring stale read for chunk" << chunkID << "(sendWindowBase is" << session.sendWindowBase << ")";
        processSendQueue(transferID);
        return;
    }

    sendChunkData(transferID, chunkID, dataB64, originalSize); // 传递 dataB64 和 originalSize

    if (chunkID == session.sendWindowBase) {
        startRetransmissionTimer(transferID);
    }

    processSendQueue(transferID);
}

void FileTransferManager::sendChunkData(const QString& transferID, qint64 chunkID, const QString& dataB64, qint64 originalChunkSize) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    // dataB64 已经是 QString 格式的Base64编码数据
    // originalChunkSize 是原始二进制数据的大小
    QString chunkMessage = FT_MSG_CHUNK_FORMAT.arg(transferID)
                                           .arg(chunkID)
                                           .arg(originalChunkSize) // 使用原始大小
                                           .arg(dataB64);          // 使用Base64编码的QString
    m_networkManager->sendMessage(session.peerUuid, chunkMessage);
    qDebug() << "FileTransferManager: Sent chunk" << chunkID << "for" << transferID << "OriginalSize:" << originalChunkSize;
}

void FileTransferManager::handleFileChunk(const QString& peerUuid, const QString& transferID, qint64 chunkID, qint64 originalChunkSize, const QString& dataB64) {
    // 在此处立即记录接收到块的信息
    qInfo() << "[FTM] Received chunk on network thread: " << " <IMPORTANT> "
            << "ChunkID=" << chunkID 
            << "OriginalSize=" << originalChunkSize 
            << "FromPeer=" << peerUuid;

    if (!m_sessions.contains(transferID) || !m_fileIOManager) {
        qWarning() << "FileTransferManager::handleFileChunk: Unknown TransferID" << transferID << "or no FileIOManager";
        if (m_sessions.contains(transferID)) sendError(peerUuid, transferID, "INVALID_TRANSFER_ID", "Unknown transfer ID or internal error.");
        return;
    }
    FileTransferSession& session = m_sessions[transferID];

    if (session.isSender || 
        (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::Accepted)) {
        qWarning() << "FileTransferManager::handleFileChunk: Invalid state for receiving chunk" << transferID << "State:" << session.state;
        return;
    }
    if (session.state == FileTransferSession::Accepted) session.state = FileTransferSession::Transferring;

    qInfo() << "[FTM] handleFileChunk: transferID=" << transferID << "chunkID=" << chunkID << "originalSize=" << originalChunkSize
            << "in-order=" << (chunkID == session.highestContiguousChunkReceived + 1)
            << "outstandingWrites=" << m_outstandingWriteRequests.value(transferID, 0)
            << "bufferedChunks=" << session.receivedOutOfOrderChunks.size();

    if (chunkID < session.highestContiguousChunkReceived + 1 || 
        chunkID >= session.highestContiguousChunkReceived + 1 + DEFAULT_RECEIVE_WINDOW_SIZE) {
        qWarning() << "FileTransferManager::handleFileChunk: Chunk" << chunkID << "out of window for" << transferID
                   << ". Expected range: [" << (session.highestContiguousChunkReceived + 1)
                   << "-" << (session.highestContiguousChunkReceived + DEFAULT_RECEIVE_WINDOW_SIZE) << "]";
        sendDataAck(peerUuid, transferID, session.highestContiguousChunkReceived); 
        return;
    }

    if (chunkID == session.highestContiguousChunkReceived + 1) {
        if (m_outstandingWriteRequests.value(transferID, 0) >= MAX_CONCURRENT_WRITES_PER_TRANSFER) {
            qDebug() << "FileTransferManager: Max concurrent writes reached for" << transferID << ". Buffering chunk" << chunkID;
            if (!session.receivedOutOfOrderChunks.contains(chunkID)) {
                 session.receivedOutOfOrderChunks.insert(chunkID, qMakePair(dataB64, originalChunkSize)); // 存储 QPair
            }
            sendDataAck(peerUuid, transferID, session.highestContiguousChunkReceived);
        } else {
            qint64 expectedOffset = session.bytesTransferred;
            qDebug() << "FileTransferManager: Requesting write for chunk" << chunkID << "at offset" << expectedOffset;
            m_fileIOManager->requestWriteFileChunk(transferID, chunkID, session.localFilePath, expectedOffset, dataB64, originalChunkSize); // 传递 dataB64 和 originalChunkSize
            m_outstandingWriteRequests[transferID]++;
        }

        // 集中ACK计数
        m_pendingAckCount[transferID] += 1;
        // 启动/重启ACK延迟定时器
        if (!m_ackDelayTimers.contains(transferID)) {
            QTimer* timer = new QTimer(this);
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, [this, peerUuid, transferID]() {
                sendDataAck(peerUuid, transferID, m_sessions[transferID].highestContiguousChunkReceived);
                m_pendingAckCount[transferID] = 0;
                if (m_ackDelayTimers.contains(transferID)) {
                    m_ackDelayTimers[transferID]->stop();
                }
            });
            m_ackDelayTimers[transferID] = timer;
        }
        if (!m_ackDelayTimers[transferID]->isActive()) {
            m_ackDelayTimers[transferID]->start(ACK_DELAY_MS);
        }
        // 如果累计到批量阈值，立即ACK
        if (m_pendingAckCount[transferID] >= ACK_BATCH_SIZE) {
            sendDataAck(peerUuid, transferID, session.highestContiguousChunkReceived);
            m_pendingAckCount[transferID] = 0;
            m_ackDelayTimers[transferID]->stop();
        }
    } else {
        if (!session.receivedOutOfOrderChunks.contains(chunkID)) {
            session.receivedOutOfOrderChunks.insert(chunkID, qMakePair(dataB64, originalChunkSize)); // 存储 QPair
            qDebug() << "FileTransferManager: Buffered out-of-order chunk" << chunkID << "for" << transferID;
        } else {
            qDebug() << "FileTransferManager: Received duplicate out-of-order chunk" << chunkID << "for" << transferID;
        }
        sendDataAck(peerUuid, transferID, session.highestContiguousChunkReceived);
    }
}

void FileTransferManager::handleChunkWritten(const QString& transferID, qint64 chunkID, qint64 bytesWritten, bool success, const QString& error) {
    if (!m_sessions.contains(transferID)) {
        m_outstandingWriteRequests[transferID]--;
        return;
    }
    m_outstandingWriteRequests[transferID]--;
    FileTransferSession& session = m_sessions[transferID];

    qInfo() << "[FTM] handleChunkWritten: transferID=" << transferID << "chunkID=" << chunkID << "bytesWritten=" << bytesWritten
            << "success=" << success << "outstandingWrites=" << m_outstandingWriteRequests.value(transferID, 0);

    if (!success) {
        qWarning() << "FileTransferManager: Failed to write chunk" << chunkID << "for" << transferID << ":" << error;
        sendError(session.peerUuid, transferID, "FILE_WRITE_ERROR_ASYNC", error);
        cleanupSession(transferID, false, tr("File write error: %1").arg(error));
        return;
    }
    
    if (chunkID == session.highestContiguousChunkReceived + 1) {
        session.bytesTransferred += bytesWritten;
        session.highestContiguousChunkReceived = chunkID;
        emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
        qDebug() << "FileTransferManager: Successfully wrote chunk" << chunkID << "for" << transferID << ". Total written:" << session.bytesTransferred;

        processBufferedChunks(transferID);

        if (session.highestContiguousChunkReceived == session.totalChunks - 1) {
            qInfo() << "FileTransferManager: All chunks appear to be written for receiver " << transferID;
            // 传输完成，立即ACK最后一次
            if (m_pendingAckCount[transferID] > 0) {
                sendDataAck(session.peerUuid, transferID, session.highestContiguousChunkReceived);
                m_pendingAckCount[transferID] = 0;
            }
            if (m_ackDelayTimers.contains(transferID)) {
                m_ackDelayTimers[transferID]->stop();
                m_ackDelayTimers[transferID]->deleteLater();
                m_ackDelayTimers.remove(transferID);
            }
        }
    } else {
        qWarning() << "FileTransferManager: Wrote chunk" << chunkID << "but expected" << (session.highestContiguousChunkReceived + 1);
        sendDataAck(session.peerUuid, transferID, session.highestContiguousChunkReceived);
    }
}

void FileTransferManager::processBufferedChunks(const QString& transferID) {
    if (!m_sessions.contains(transferID) || !m_fileIOManager) return;
    FileTransferSession& session = m_sessions[transferID];

    qint64 nextExpectedChunk = session.highestContiguousChunkReceived + 1;
    if (session.receivedOutOfOrderChunks.contains(nextExpectedChunk) &&
        m_outstandingWriteRequests.value(transferID, 0) < MAX_CONCURRENT_WRITES_PER_TRANSFER) {

        QPair<QString, qint64> chunkInfo = session.receivedOutOfOrderChunks.take(nextExpectedChunk); 
        QString dataB64 = chunkInfo.first;
        qint64 originalSize = chunkInfo.second;
        qint64 expectedOffset = session.bytesTransferred; 

        qDebug() << "FileTransferManager: Requesting write for buffered chunk" << nextExpectedChunk << "for" << transferID << "at offset" << expectedOffset;
        m_fileIOManager->requestWriteFileChunk(transferID, nextExpectedChunk, session.localFilePath, expectedOffset, dataB64, originalSize); // 传递 dataB64 和 originalSize
        m_outstandingWriteRequests[transferID]++;
    }

    qInfo() << "[FTM] processBufferedChunks: transferID=" << transferID
            << "nextExpectedChunk=" << nextExpectedChunk
            << "buffered=" << session.receivedOutOfOrderChunks.size()
            << "outstandingWrites=" << m_outstandingWriteRequests.value(transferID, 0);
}

void FileTransferManager::sendDataAck(const QString& peerUuid, const QString& transferID, qint64 ackedChunkID) {
    QString ackMsg = FT_MSG_DATA_ACK_FORMAT.arg(transferID).arg(ackedChunkID).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, ackMsg);
    qDebug() << "FileTransferManager: Sent ACK for highest contiguous chunk" << ackedChunkID << "for" << transferID;

    qInfo() << "[FTM] sendDataAck: transferID=" << transferID << "ackedChunkID=" << ackedChunkID;
}

void FileTransferManager::handleDataAck(const QString& peerUuid, const QString& transferID, qint64 ackedChunkID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender || (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::WaitingForAck)) {
        qWarning() << "FileTransferManager::handleDataAck: Received ACK in invalid state for" << transferID << "State:" << session.state;
        return;
    }
    
    qDebug() << "FileTransferManager: Received ACK for chunk up to" << ackedChunkID << "for" << transferID << ". Current sendWindowBase:" << session.sendWindowBase;

    if (ackedChunkID >= session.sendWindowBase) {
        stopRetransmissionTimer(transferID);

        qint64 oldSendWindowBase = session.sendWindowBase;
        session.sendWindowBase = ackedChunkID + 1;

        qInfo() << "[FTM] handleDataAck: transferID=" << transferID << "ackedChunkID=" << ackedChunkID
                << "oldSendWindowBase=" << oldSendWindowBase << "newSendWindowBase=" << session.sendWindowBase;

        if (session.sendWindowBase > oldSendWindowBase) {
            session.bytesTransferred = qMin(session.fileSize, session.sendWindowBase * DEFAULT_CHUNK_SIZE);
            if (session.sendWindowBase >= session.totalChunks) {
                session.bytesTransferred = session.fileSize;
            }
            emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
        }
        
        if (session.sendWindowBase >= session.totalChunks) {
            qInfo() << "FileTransferManager: All chunks ACKed for" << transferID;
            sendEOF(transferID);
        } else {
            session.state = FileTransferSession::Transferring;
            processSendQueue(transferID);
        }
    } else {
        qDebug() << "FileTransferManager: Received old/duplicate ACK for" << ackedChunkID << "(current base" << session.sendWindowBase << ")";
    }
}

void FileTransferManager::sendEOF(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    
    stopRetransmissionTimer(transferID);

    QString finalChecksum = "NOT_IMPLEMENTED";
    QString eofMsg = FT_MSG_EOF_FORMAT.arg(transferID).arg(session.totalChunks).arg(finalChecksum); 
    m_networkManager->sendMessage(session.peerUuid, eofMsg);
    session.state = FileTransferSession::WaitingForAck;
    
    session.sendWindowBase = session.totalChunks;
    startRetransmissionTimer(transferID);

    qInfo() << "FileTransferManager: Sent EOF for" << transferID << "Total Chunks:" << session.totalChunks;
}

void FileTransferManager::handleEOF(const QString& peerUuid, const QString& transferID, qint64 totalChunksReported, const QString& finalChecksum) {
    Q_UNUSED(finalChecksum);
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (session.isSender || (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::Accepted)) {
        qWarning() << "FileTransferManager::handleEOF: Received EOF in invalid state for" << transferID;
        return;
    }
    
    if (m_outstandingWriteRequests.value(transferID, 0) > 0) {
        qWarning() << "FileTransferManager::handleEOF: Received EOF for" << transferID << "but" << m_outstandingWriteRequests.value(transferID, 0) << "writes are still outstanding. Deferring EOF processing.";
        return;
    }

    if (session.highestContiguousChunkReceived != session.totalChunks - 1) {
        qWarning() << "FileTransferManager::handleEOF: Received EOF for" << transferID 
                   << "but not all chunks are contiguously received. Last received:" << session.highestContiguousChunkReceived
                   << "Total expected:" << (session.totalChunks -1);
        processBufferedChunks(transferID);
        if (session.highestContiguousChunkReceived != session.totalChunks -1 && m_outstandingWriteRequests.value(transferID, 0) == 0) {
            sendError(peerUuid, transferID, "EOF_WITH_MISSING_CHUNKS", "Received EOF but chunks are missing.");
            cleanupSession(transferID, false, tr("Transfer incomplete: EOF received with missing chunks."));
            return;
        } else if (session.highestContiguousChunkReceived != session.totalChunks -1) {
            qInfo() << "FileTransferManager::handleEOF: EOF for" << transferID << "received, but waiting for pending writes to complete for missing chunks.";
            return; 
        }
    }
    
    if (totalChunksReported != session.totalChunks) {
        qWarning() << "FileTransferManager::handleEOF: Total chunks mismatch for" << transferID 
                   << ". Peer reported:" << totalChunksReported << ", we calculated:" << session.totalChunks;
    }

    qInfo() << "FileTransferManager: Received EOF for" << transferID << ". File" << session.fileName << "received.";
    sendEOFAck(peerUuid, transferID);
    cleanupSession(transferID, true, tr("File received successfully."));
}

void FileTransferManager::handleEOFAck(const QString& peerUuid, const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    
    if (!session.isSender || session.state != FileTransferSession::WaitingForAck) { 
        qWarning() << "FileTransferManager::handleEOFAck: Received EOF_ACK in invalid state for" << transferID;
        return;
    }
    stopRetransmissionTimer(transferID);
    qInfo() << "FileTransferManager: Received EOF_ACK for" << transferID << ". File" << session.fileName << "sent successfully.";
    cleanupSession(transferID, true, tr("File sent successfully."));
}

void FileTransferManager::sendEOFAck(const QString& peerUuid, const QString& transferID)
{
    if (!m_networkManager) {
        qWarning() << "FileTransferManager::sendEOFAck: NetworkManager is null.";
        return;
    }
    QString eofAckMsg = FT_MSG_EOF_ACK_FORMAT.arg(transferID).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, eofAckMsg);
    qDebug() << "FileTransferManager: Sent EOF_ACK for" << transferID << "to" << peerUuid;
}

void FileTransferManager::sendError(const QString& peerUuid, const QString& transferID, const QString& errorCode, const QString& errorMessage) {
    QString errorMsg = FT_MSG_ERROR_FORMAT.arg(transferID).arg(errorCode).arg(errorMessage).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, errorMsg);
}

void FileTransferManager::handleFileError(const QString& peerUuid, const QString& transferID, const QString& errorCode, const QString& message) {
    Q_UNUSED(peerUuid);
    if (!m_sessions.contains(transferID)) return;
    
    qWarning() << "FileTransferManager: Received error for transfer" << transferID << "Code:" << errorCode << "Message:" << message;
    cleanupSession(transferID, false, tr("Transfer failed due to peer error: %1 (%2)").arg(message).arg(errorCode));
}

void FileTransferManager::cleanupSession(const QString& transferID, bool success, const QString& message) {
    if (!m_sessions.contains(transferID)) return;
    
    FileTransferSession session = m_sessions.take(transferID); 
    session.stopAndClearRetransmissionTimer();

    m_outstandingReadRequests.remove(transferID);
    m_outstandingWriteRequests.remove(transferID);

    if (m_ackDelayTimers.contains(transferID)) {
        m_ackDelayTimers[transferID]->stop();
        m_ackDelayTimers[transferID]->deleteLater();
        m_ackDelayTimers.remove(transferID);
    }
    m_pendingAckCount.remove(transferID);

    // 统计传输耗时和速度
    double speedMBps = 0.0;
    qint64 elapsedMs = 0;
    if (m_transferTimers.contains(transferID)) {
        elapsedMs = m_transferTimers[transferID]->elapsed();
        delete m_transferTimers[transferID];
        m_transferTimers.remove(transferID);
    }
    qint64 totalBytes = session.fileSize;
    if (success && elapsedMs > 0 && totalBytes > 0) {
        speedMBps = (double)totalBytes / 1024.0 / 1024.0 / ((double)elapsedMs / 1000.0);
        qInfo() << "[FTM] Transfer" << transferID << "finished in" << elapsedMs << "ms,"
                << "average speed:" << QString::number(speedMBps, 'f', 2) << "MB/s";
    }

    // 在信号中带上速度信息
    QString msgWithSpeed = message;
    if (success && speedMBps > 0.0) {
        msgWithSpeed += tr(" (Avg speed: %1 MB/s, Time: %2 ms)").arg(QString::number(speedMBps, 'f', 2)).arg(elapsedMs);
    }
    if (success) {
        emit fileTransferFinished(transferID, session.peerUuid, session.fileName, true, msgWithSpeed);
    } else {
        emit fileTransferError(transferID, session.peerUuid, message);
        emit fileTransferFinished(transferID, session.peerUuid, session.fileName, false, message);
    }
    qInfo() << "FileTransferManager: Cleaned up session" << transferID << (success ? "Successfully" : "Unsuccessfully");
}

void FileTransferManager::startRetransmissionTimer(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    
    session.stopAndClearRetransmissionTimer();

    session.retransmissionTimer = new QTimer(this);
    session.retransmissionTimer->setSingleShot(true);
    
    connect(session.retransmissionTimer, &QTimer::timeout, this, [this, transferID]() {
        handleChunkRetransmissionTimeout(transferID);
    });
    
    int timeoutDuration = FT_CHUNK_RETRANSMISSION_TIMEOUT_MS;
    if (session.state == FileTransferSession::WaitingForAck && session.sendWindowBase >= session.totalChunks) {
    }
    session.retransmissionTimer->start(timeoutDuration); 
    qDebug() << "FileTransferManager: Started retransmission timer for" << transferID << "Base:" << session.sendWindowBase << "Duration:" << timeoutDuration;
}

void FileTransferManager::stopRetransmissionTimer(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    session.stopAndClearRetransmissionTimer();
    qDebug() << "FileTransferManager: Stopped retransmission timer for" << transferID;
}

void FileTransferManager::handleChunkRetransmissionTimeout(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender) return;

    if (session.state == FileTransferSession::WaitingForAck && session.sendWindowBase >= session.totalChunks) {
        qWarning() << "FileTransferManager: Timeout waiting for EOF_ACK for transfer" << transferID;
        sendError(session.peerUuid, transferID, "EOF_ACK_TIMEOUT", "Timeout waiting for EOF acknowledgment.");
        cleanupSession(transferID, false, tr("Timeout waiting for EOF acknowledgment from peer."));
        return;
    }

    qWarning() << "FileTransferManager: Retransmission Timeout for transfer" << transferID << "ChunkID (Base):" << session.sendWindowBase;
    
    m_outstandingReadRequests[transferID] = 0; 
    session.nextChunkToSendInWindow = session.sendWindowBase;
    session.state = FileTransferSession::Transferring;
    
    qInfo() << "FileTransferManager: Retransmitting by re-requesting read for chunk" << session.sendWindowBase << "for transfer" << transferID;
    processSendQueue(transferID);
}

