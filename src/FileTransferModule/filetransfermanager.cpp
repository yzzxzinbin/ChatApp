#include "filetransfermanager.h"
#include "networkmanager.h" // Include full definition
#include <QUuid>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths> // For default save location
#include <QBuffer> // For Base64 encoding/decoding

// Helper function to extract attribute (can be moved to a shared utility if NetworkManager's one is not accessible/suitable)
QString FileTransferManager::extractMessageAttribute(const QString& message, const QString& attributeName) const {
    QRegularExpression regex(QStringLiteral("%1=\\\"([^\\\"]*)\\\"").arg(attributeName));
    QRegularExpressionMatch match = regex.match(message);
    if (match.hasMatch() && match.capturedTexts().size() > 1) {
        return match.captured(1);
    }
    return QString();
}

FileTransferManager::FileTransferManager(NetworkManager* networkManager, const QString& localUserUuid, QObject *parent)
    : QObject(parent), m_networkManager(networkManager), m_localUserUuid(localUserUuid)
{
    if (!m_networkManager) {
        qCritical() << "FileTransferManager initialized with a null NetworkManager!";
    }
}

FileTransferManager::~FileTransferManager()
{
    // Clean up any active sessions, close files, etc.
    for (const QString& transferID : m_sessions.keys()) {
        FileTransferSession& session = m_sessions[transferID];
        session.stopAndClearRetransmissionTimer(); // Explicitly stop timer before map clear
        if (session.file && session.file->isOpen()) {
            session.file->close();
        }
        delete session.file;
        session.file = nullptr;
    }
    m_sessions.clear();
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
    session.localFilePath = filePath; // Store the full path of the file to send
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
    qDebug() << "FileTransferManager: Sent file offer:" << offerMessage << "to" << peerUuid;
}

void FileTransferManager::handleIncomingFileMessage(const QString& peerUuid, const QString& message)
{
    qDebug() << "FileTransferManager::handleIncomingFileMessage from" << peerUuid << ":" << message;

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
        QString savePathHint = extractMessageAttribute(message, "SavePathHint"); // Not strictly used by sender but good to parse
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
        qint64 chunkSize = extractMessageAttribute(message, "Size").toLongLong();
        QString dataB64 = extractMessageAttribute(message, "Data");
        QByteArray data = QByteArray::fromBase64(dataB64.toUtf8());

        if (transferID.isEmpty() || dataB64.isEmpty() || data.size() != chunkSize) {
            qWarning() << "FileTransferManager: Invalid FT_CHUNK received:" << message;
            sendError(peerUuid, transferID, "CHUNK_INVALID", "Received invalid chunk data.");
            return;
        }
        handleFileChunk(peerUuid, transferID, chunkID, chunkSize, data);
    } else if (message.startsWith("<FT_ACK_DATA")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        qint64 ackedChunkID = extractMessageAttribute(message, "ChunkID").toLongLong(); // This is highest contiguous received by peer
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
    session.localFilePath = savePath; // Store the save path
    session.state = FileTransferSession::Accepted;
    sendAcceptMessage(session.peerUuid, transferID, savePath); // Pass savePath (or a hint)
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
    // savePathHint is optional, could be empty. Receiver might not need it.
    QString acceptMessage = FT_MSG_ACCEPT_FORMAT.arg(transferID).arg(m_localUserUuid).arg(savePathHint);
    m_networkManager->sendMessage(peerUuid, acceptMessage);
    qDebug() << "FileTransferManager: Sent file accept:" << acceptMessage << "to" << peerUuid;
}

void FileTransferManager::sendRejectMessage(const QString& peerUuid, const QString& transferID, const QString& reason)
{
    QString rejectMessage = FT_MSG_REJECT_FORMAT.arg(transferID).arg(reason).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, rejectMessage);
    qDebug() << "FileTransferManager: Sent file reject:" << rejectMessage << "to" << peerUuid;
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
        // Optionally send an error back to peerUuid
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
        return;
    }

    session.file = new QFile(session.localFilePath);
    if (!session.file->open(QIODevice::ReadOnly)) {
        qWarning() << "FileTransferManager: Could not open file for sending:" << session.localFilePath << session.file->errorString();
        cleanupSession(transferID, false, tr("Could not open file: %1").arg(session.file->errorString()));
        delete session.file;
        session.file = nullptr;
        return;
    }
    
    session.state = FileTransferSession::Transferring;
    session.sendWindowBase = 0;
    session.nextChunkToSendInWindow = 0;
    session.bytesTransferred = 0; // Bytes ACKed by peer
    qInfo() << "FileTransferManager: Starting to send file" << session.fileName << "for TransferID" << transferID;
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, true);
    processSendQueue(transferID); // Start sending chunks
}

void FileTransferManager::prepareToReceiveFile(const QString& transferID, const QString& savePath) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    session.file = new QFile(savePath);
    if (!session.file->open(QIODevice::WriteOnly)) {
        qWarning() << "FileTransferManager: Could not open file for receiving:" << savePath << session.file->errorString();
        cleanupSession(transferID, false, tr("Could not create/open file for saving: %1").arg(session.file->errorString()));
        sendError(session.peerUuid, transferID, "FILE_OPEN_ERROR_RECEIVER", "Could not open file for saving.");
        delete session.file;
        session.file = nullptr;
        return;
    }
    
    session.state = FileTransferSession::Transferring;
    session.highestContiguousChunkReceived = -1; // No chunks received yet
    session.receivedOutOfOrderChunks.clear();
    session.bytesTransferred = 0; // Bytes written to disk
    qInfo() << "FileTransferManager: Preparing to receive file" << session.fileName << "for TransferID" << transferID;
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, false);
    // Receiver waits for the first chunk.
}

void FileTransferManager::processSendQueue(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender || !session.file || session.state != FileTransferSession::Transferring) {
        // If state is Paused, don't send. If it's WaitingForAck, the window is full or an ACK is pending.
        return;
    }

    // Send new chunks as long as the window is not full
    // and we haven't sent all chunks for the current window pass or all file chunks.
    while (session.nextChunkToSendInWindow < session.sendWindowBase + DEFAULT_SEND_WINDOW_SIZE &&
           session.nextChunkToSendInWindow < session.totalChunks) {
        
        qint64 currentChunkID = session.nextChunkToSendInWindow;
        
        // Seek to the correct position for the current chunk
        qint64 offset = currentChunkID * DEFAULT_CHUNK_SIZE;
        if (!session.file->seek(offset)) {
            qWarning() << "FileTransferManager::processSendQueue: Seek failed for chunk" << currentChunkID << "Offset:" << offset;
            cleanupSession(transferID, false, tr("File seek error during transfer."));
            sendError(session.peerUuid, transferID, "FILE_SEEK_ERROR", "Error seeking file for chunk.");
            return;
        }

        QByteArray chunkData = session.file->read(DEFAULT_CHUNK_SIZE);
        if (chunkData.isEmpty() && !session.file->atEnd()) { // Read error if not at EOF but got empty
             qWarning() << "FileTransferManager::processSendQueue: Read empty chunk before EOF for" << transferID << "ChunkID:" << currentChunkID;
             cleanupSession(transferID, false, tr("File read error during transfer."));
             sendError(session.peerUuid, transferID, "FILE_READ_ERROR", "Error reading file chunk.");
             return;
        }
        if (chunkData.isEmpty() && session.file->atEnd() && (currentChunkID * DEFAULT_CHUNK_SIZE < session.fileSize)) {
            // Should not happen if totalChunks is calculated correctly based on fileSize
            qWarning() << "FileTransferManager::processSendQueue: Read empty chunk at EOF but not all bytes sent for" << transferID << "ChunkID:" << currentChunkID;
            // This might indicate an issue with fileSize or totalChunks calculation.
            // Proceeding might send an empty chunk if it's the last one and size is 0.
        }


        sendChunkData(transferID, currentChunkID, chunkData);
        session.nextChunkToSendInWindow++;

        // If this is the first chunk in the window being sent (or re-sent), start its timer
        if (currentChunkID == session.sendWindowBase) {
            startRetransmissionTimer(transferID);
        }
    }

    // If all chunks have been sent and ACKed (which handleDataAck will determine by comparing sendWindowBase to totalChunks)
    // then handleDataAck will call sendEOF.
    // If nextChunkToSendInWindow has reached totalChunks, all data is "in flight" or ACKed.
    // We wait for ACKs.
}


void FileTransferManager::sendChunkData(const QString& transferID, qint64 chunkID, const QByteArray& chunkData) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    QString dataB64 = QString::fromUtf8(chunkData.toBase64());
    QString chunkMessage = FT_MSG_CHUNK_FORMAT.arg(transferID)
                                           .arg(chunkID)
                                           .arg(chunkData.size())
                                           .arg(dataB64);
    m_networkManager->sendMessage(session.peerUuid, chunkMessage);
    qDebug() << "FileTransferManager: Sent chunk" << chunkID << "for" << transferID << "Size:" << chunkData.size();
    // Progress for sender is updated upon receiving ACKs in handleDataAck
}


void FileTransferManager::handleFileChunk(const QString& peerUuid, const QString& transferID, qint64 chunkID, qint64 chunkSize, const QByteArray& data) {
    if (!m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager::handleFileChunk: Unknown TransferID" << transferID;
        sendError(peerUuid, transferID, "INVALID_TRANSFER_ID", "Unknown transfer ID.");
        return;
    }
    FileTransferSession& session = m_sessions[transferID];

    if (session.isSender || !session.file || !session.file->isOpen() || 
        (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::Accepted)) { // Accept initial chunks in Accepted state too
        qWarning() << "FileTransferManager::handleFileChunk: Invalid state for receiving chunk" << transferID << "State:" << session.state;
        return;
    }
    if (session.state == FileTransferSession::Accepted) session.state = FileTransferSession::Transferring;


    // Check if chunk is within the expected receive window or is the next expected chunk
    if (chunkID < session.highestContiguousChunkReceived + 1 || 
        chunkID >= session.highestContiguousChunkReceived + 1 + DEFAULT_RECEIVE_WINDOW_SIZE) {
        // This chunk is either a duplicate of an already processed one, or too far ahead.
        // If it's an old duplicate, we still ACK our current highestContiguous to help sender.
        // If it's too far ahead, we also just ACK our current highest.
        qWarning() << "FileTransferManager::handleFileChunk: Chunk" << chunkID << "out of window for" << transferID
                   << ". Expected range: [" << (session.highestContiguousChunkReceived + 1)
                   << "-" << (session.highestContiguousChunkReceived + DEFAULT_RECEIVE_WINDOW_SIZE) << "]";
        sendDataAck(peerUuid, transferID, session.highestContiguousChunkReceived); // ACK what we have
        return;
    }

    if (chunkID == session.highestContiguousChunkReceived + 1) {
        // This is the next expected chunk in sequence
        qint64 bytesWritten = session.file->write(data);
        if (bytesWritten != chunkSize) {
            qWarning() << "FileTransferManager::handleFileChunk: File write error for" << transferID << session.file->errorString();
            cleanupSession(transferID, false, tr("Error writing to file: %1").arg(session.file->errorString()));
            sendError(peerUuid, transferID, "FILE_WRITE_ERROR", "Error writing received chunk to file.");
            return;
        }
        session.bytesTransferred += bytesWritten;
        session.highestContiguousChunkReceived = chunkID;
        emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
        qDebug() << "FileTransferManager: Received and wrote chunk" << chunkID << "for" << transferID;

        // Try to process any buffered chunks that are now contiguous
        processBufferedChunks(transferID);

    } else { // Chunk is within the window but out of order
        if (!session.receivedOutOfOrderChunks.contains(chunkID)) {
            session.receivedOutOfOrderChunks.insert(chunkID, data);
            qDebug() << "FileTransferManager: Buffered out-of-order chunk" << chunkID << "for" << transferID;
        } else {
            qDebug() << "FileTransferManager: Received duplicate out-of-order chunk" << chunkID << "for" << transferID;
            // Already buffered, or a duplicate of a future chunk.
        }
    }
    
    // Always ACK the highest contiguous chunk received so far
    sendDataAck(peerUuid, transferID, session.highestContiguousChunkReceived);
}

void FileTransferManager::processBufferedChunks(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    bool chunkProcessed = true;
    while(chunkProcessed) {
        chunkProcessed = false;
        qint64 nextExpectedChunk = session.highestContiguousChunkReceived + 1;
        if (session.receivedOutOfOrderChunks.contains(nextExpectedChunk)) {
            QByteArray data = session.receivedOutOfOrderChunks.take(nextExpectedChunk);
            qint64 bytesWritten = session.file->write(data);
            if (bytesWritten != data.size()) {
                 qWarning() << "FileTransferManager::processBufferedChunks: File write error for buffered chunk" << nextExpectedChunk << session.file->errorString();
                 cleanupSession(transferID, false, tr("Error writing buffered chunk to file: %1").arg(session.file->errorString()));
                 sendError(session.peerUuid, transferID, "FILE_WRITE_ERROR_BUFFERED", "Error writing buffered chunk.");
                 return; // Critical error
            }
            session.bytesTransferred += bytesWritten;
            session.highestContiguousChunkReceived = nextExpectedChunk;
            emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
            qDebug() << "FileTransferManager: Wrote buffered chunk" << nextExpectedChunk << "for" << transferID;
            chunkProcessed = true; // A chunk was processed, loop again
        }
    }
}


void FileTransferManager::sendDataAck(const QString& peerUuid, const QString& transferID, qint64 ackedChunkID) {
    // ackedChunkID is the highest *contiguous* chunk ID successfully received and processed.
    QString ackMsg = FT_MSG_DATA_ACK_FORMAT.arg(transferID).arg(ackedChunkID).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, ackMsg);
    qDebug() << "FileTransferManager: Sent ACK for highest contiguous chunk" << ackedChunkID << "for" << transferID;
}

void FileTransferManager::handleDataAck(const QString& peerUuid, const QString& transferID, qint64 ackedChunkID) {
    // ackedChunkID is the highest contiguous chunk ID the peer has received.
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender || (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::WaitingForAck)) {
        qWarning() << "FileTransferManager::handleDataAck: Received ACK in invalid state for" << transferID << "State:" << session.state;
        return;
    }
    
    qDebug() << "FileTransferManager: Received ACK for chunk up to" << ackedChunkID << "for" << transferID << ". Current sendWindowBase:" << session.sendWindowBase;

    if (ackedChunkID >= session.sendWindowBase) {
        stopRetransmissionTimer(transferID); // Stop timer for the old base

        // Update bytesTransferred based on newly ACKed chunks
        qint64 newlyAckedChunksCount = (ackedChunkID + 1) - session.sendWindowBase;
        if (newlyAckedChunksCount > 0) {
            session.bytesTransferred = qMin(session.fileSize, (ackedChunkID + 1) * DEFAULT_CHUNK_SIZE);
            emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
        }
        
        session.sendWindowBase = ackedChunkID + 1; // Slide window

        if (session.sendWindowBase >= session.totalChunks) {
            // All chunks have been ACKed
            qInfo() << "FileTransferManager: All chunks ACKed for" << transferID;
            sendEOF(transferID);
        } else {
            session.nextChunkToSendInWindow = qMax(session.nextChunkToSendInWindow, session.sendWindowBase);
            session.state = FileTransferSession::Transferring; // Ensure state is ready for more sends
            processSendQueue(transferID); // Try to send more chunks from the new window base

            if (session.sendWindowBase < session.nextChunkToSendInWindow) { 
                 startRetransmissionTimer(transferID);
            }
        }
    } else {
        qDebug() << "FileTransferManager: Received old/duplicate ACK for" << ackedChunkID << "(current base" << session.sendWindowBase << ")";
    }
}


void FileTransferManager::sendEOF(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    
    stopRetransmissionTimer(transferID); // Stop any chunk retransmission timers

    QString finalChecksum = "NOT_IMPLEMENTED"; // Placeholder
    QString eofMsg = FT_MSG_EOF_FORMAT.arg(transferID).arg(session.totalChunks).arg(finalChecksum); 
    m_networkManager->sendMessage(session.peerUuid, eofMsg);
    session.state = FileTransferSession::WaitingForAck; // Waiting for EOF_ACK
    
    session.sendWindowBase = session.totalChunks; // Mark as if we are waiting for an ACK for a "virtual" chunk past the last one.
    startRetransmissionTimer(transferID); // This timer will now timeout if EOF_ACK is not received.

    qInfo() << "FileTransferManager: Sent EOF for" << transferID << "Total Chunks:" << session.totalChunks;
    if (session.file && session.file->isOpen()) {
        session.file->close(); // Sender closes file after sending EOF
    }
}

void FileTransferManager::handleEOF(const QString& peerUuid, const QString& transferID, qint64 totalChunksReported, const QString& finalChecksum) {
    Q_UNUSED(finalChecksum);
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (session.isSender || (session.state != FileTransferSession::Transferring && session.state != FileTransferSession::Accepted)) {
        qWarning() << "FileTransferManager::handleEOF: Received EOF in invalid state for" << transferID;
        return;
    }
    
    if (session.highestContiguousChunkReceived != session.totalChunks - 1) {
        qWarning() << "FileTransferManager::handleEOF: Received EOF for" << transferID 
                   << "but not all chunks are contiguously received. Last received:" << session.highestContiguousChunkReceived
                   << "Total expected:" << (session.totalChunks -1);
        processBufferedChunks(transferID);
        if (session.highestContiguousChunkReceived != session.totalChunks -1) {
            sendError(peerUuid, transferID, "EOF_WITH_MISSING_CHUNKS", "Received EOF but chunks are missing.");
            cleanupSession(transferID, false, tr("Transfer incomplete: EOF received with missing chunks."));
            return;
        }
    }
    
    if (totalChunksReported != session.totalChunks) {
        qWarning() << "FileTransferManager::handleEOF: Total chunks mismatch for" << transferID 
                   << ". Peer reported:" << totalChunksReported << ", we calculated:" << session.totalChunks;
    }

    qInfo() << "FileTransferManager: Received EOF for" << transferID << ". File" << session.fileName << "received.";
    if (session.file && session.file->isOpen()) {
        session.file->close(); // Receiver closes file after processing EOF
    }
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
    stopRetransmissionTimer(transferID); // Stop the EOF_ACK timer
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
    session.stopAndClearRetransmissionTimer(); // Ensure timer is stopped and deleted

    if (session.file) {
        if (session.file->isOpen()) {
            session.file->close();
        }
        delete session.file;
    }
    
    if (success) {
        emit fileTransferFinished(transferID, session.peerUuid, session.fileName, true, message);
    } else {
        emit fileTransferError(transferID, session.peerUuid, message);
        emit fileTransferFinished(transferID, session.peerUuid, session.fileName, false, message);
    }
    qInfo() << "FileTransferManager: Cleaned up session" << transferID << (success ? "Successfully" : "Unsuccessfully");
}


void FileTransferManager::startRetransmissionTimer(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    
    session.stopAndClearRetransmissionTimer(); // Stop and delete any existing timer for this session

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
    
    session.nextChunkToSendInWindow = session.sendWindowBase; // Reset to resend from the base
    session.state = FileTransferSession::Transferring; // Ensure state allows sending
    
    qInfo() << "FileTransferManager: Retransmitting from chunk" << session.sendWindowBase << "for transfer" << transferID;
    processSendQueue(transferID); 
}

