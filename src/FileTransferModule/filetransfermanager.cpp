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
        stopAckTimer(transferID);
        FileTransferSession& session = m_sessions[transferID];
        if (session.file && session.file->isOpen()) {
            session.file->close();
        }
        delete session.file;
        session.file = nullptr;
    }
    m_sessions.clear();
    qDeleteAll(m_transferTimers);
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
        qint64 chunkID = extractMessageAttribute(message, "ChunkID").toLongLong();
        QString ackingPeerUuid = extractMessageAttribute(message, "ReceiverUUID");
        if (transferID.isEmpty() || ackingPeerUuid.isEmpty() || ackingPeerUuid != peerUuid) {
             qWarning() << "FileTransferManager: Invalid FT_ACK_DATA received:" << message;
            return;
        }
        handleDataAck(peerUuid, transferID, chunkID);
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
    session.currentChunkID = 0;
    session.bytesTransferred = 0;
    qInfo() << "FileTransferManager: Starting to send file" << session.fileName << "for TransferID" << transferID;
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, true);
    sendChunk(transferID); // Send the first chunk
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
    session.currentChunkID = 0;
    session.bytesTransferred = 0;
    qDebug() << "FileTransferManager: Preparing to receive file" << session.fileName << "for TransferID" << transferID;
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, false);
    // Receiver waits for the first chunk.
}

void FileTransferManager::sendChunk(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender || !session.file || !session.file->isOpen() || session.state != FileTransferSession::Transferring) {
        qWarning() << "FileTransferManager::sendChunk: Invalid state or file for transfer" << transferID;
        return;
    }

    if (session.bytesTransferred >= session.fileSize) { // All data sent
        sendEOF(transferID);
        return;
    }

    QByteArray chunkData = session.file->read(DEFAULT_CHUNK_SIZE);
    if (chunkData.isEmpty() && session.file->atEnd() && session.bytesTransferred < session.fileSize) {
        qWarning() << "FileTransferManager::sendChunk: Read empty chunk before EOF for" << transferID;
        cleanupSession(transferID, false, tr("File read error during transfer."));
        sendError(session.peerUuid, transferID, "FILE_READ_ERROR", "Error reading file chunk.");
        return;
    }
    
    QString dataB64 = QString::fromUtf8(chunkData.toBase64());
    QString chunkMessage = FT_MSG_CHUNK_FORMAT.arg(transferID)
                                           .arg(session.currentChunkID)
                                           .arg(chunkData.size())
                                           .arg(dataB64);
    m_networkManager->sendMessage(session.peerUuid, chunkMessage);
    session.state = FileTransferSession::WaitingForAck; // Wait for ACK for this chunk
    startAckTimer(transferID);
    qDebug() << "FileTransferManager: Sent chunk" << session.currentChunkID << "for" << transferID << "Size:" << chunkData.size();
}

void FileTransferManager::handleFileChunk(const QString& peerUuid, const QString& transferID, qint64 chunkID, qint64 chunkSize, const QByteArray& data) {
    if (!m_sessions.contains(transferID)) {
        qWarning() << "FileTransferManager::handleFileChunk: Unknown TransferID" << transferID;
        return;
    }
    FileTransferSession& session = m_sessions[transferID];

    if (session.isSender || !session.file || !session.file->isOpen() || session.state != FileTransferSession::Transferring) {
        qWarning() << "FileTransferManager::handleFileChunk: Invalid state for receiving chunk" << transferID;
        return;
    }

    if (chunkID != session.currentChunkID) {
        qWarning() << "FileTransferManager::handleFileChunk: ChunkID mismatch for" << transferID
                   << "Expected:" << session.currentChunkID << "Got:" << chunkID;
        // Optionally send NACK or error
        sendError(peerUuid, transferID, "CHUNK_ID_MISMATCH", QString("Expected chunk %1, got %2").arg(session.currentChunkID).arg(chunkID));
        return;
    }

    qint64 bytesWritten = session.file->write(data);
    if (bytesWritten != chunkSize) {
        qWarning() << "FileTransferManager::handleFileChunk: File write error for" << transferID << session.file->errorString();
        cleanupSession(transferID, false, tr("Error writing to file: %1").arg(session.file->errorString()));
        sendError(peerUuid, transferID, "FILE_WRITE_ERROR", "Error writing received chunk to file.");
        return;
    }
    session.file->flush();

    session.bytesTransferred += bytesWritten;
    emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
    
    sendDataAck(peerUuid, transferID, chunkID);
    session.currentChunkID++; // Expect next chunk
    qDebug() << "FileTransferManager: Received and ACKed chunk" << chunkID << "for" << transferID << ". Total bytes:" << session.bytesTransferred;
}

void FileTransferManager::sendDataAck(const QString& peerUuid, const QString& transferID, qint64 chunkID) {
    QString ackMsg = FT_MSG_DATA_ACK_FORMAT.arg(transferID).arg(chunkID).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, ackMsg);
}

void FileTransferManager::handleDataAck(const QString& peerUuid, const QString& transferID, qint64 chunkID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (!session.isSender || session.state != FileTransferSession::WaitingForAck) {
        qWarning() << "FileTransferManager::handleDataAck: Received ACK in invalid state for" << transferID;
        return;
    }
    if (chunkID != session.currentChunkID) {
         qWarning() << "FileTransferManager::handleDataAck: ACK ChunkID mismatch for" << transferID
                   << "Expected:" << session.currentChunkID << "Got:" << chunkID;
        // This could be a late ACK, or an error. For now, we'll be strict.
        return;
    }
    
    stopAckTimer(transferID);
    session.bytesTransferred += session.file->pos() - session.bytesTransferred; // More accurate way to track if read happened
    emit fileTransferProgress(transferID, session.bytesTransferred, session.fileSize);
    session.currentChunkID++;
    session.state = FileTransferSession::Transferring; // Ready to send next chunk
    sendChunk(transferID); // Send next chunk or EOF
}

void FileTransferManager::sendEOF(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    QString finalChecksum = "NOT_IMPLEMENTED";
    QString eofMsg = FT_MSG_EOF_FORMAT.arg(transferID).arg(session.currentChunkID).arg(finalChecksum);
    m_networkManager->sendMessage(session.peerUuid, eofMsg);
    session.state = FileTransferSession::WaitingForAck; // Waiting for EOF_ACK
    startAckTimer(transferID);
    qInfo() << "FileTransferManager: Sent EOF for" << transferID << "Total Chunks:" << session.currentChunkID;
    if (session.file && session.file->isOpen()) {
        session.file->close();
    }
}

void FileTransferManager::handleEOF(const QString& peerUuid, const QString& transferID, qint64 totalChunks, const QString& finalChecksum) {
    Q_UNUSED(totalChunks); Q_UNUSED(finalChecksum);
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];

    if (session.isSender || session.state != FileTransferSession::Transferring) {
        qWarning() << "FileTransferManager::handleEOF: Received EOF in invalid state for" << transferID;
        return;
    }
    
    qInfo() << "FileTransferManager: Received EOF for" << transferID << ". File" << session.fileName << "received.";
    if (session.file && session.file->isOpen()) {
        session.file->close();
    }
    sendEOFAck(peerUuid, transferID);
    cleanupSession(transferID, true, tr("File received successfully."));
}

void FileTransferManager::sendEOFAck(const QString& peerUuid, const QString& transferID) {
    QString eofAckMsg = FT_MSG_EOF_ACK_FORMAT.arg(transferID).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, eofAckMsg);
}

void FileTransferManager::handleEOFAck(const QString& peerUuid, const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    
    if (!session.isSender || session.state != FileTransferSession::WaitingForAck) {
        qWarning() << "FileTransferManager::handleEOFAck: Received EOF_ACK in invalid state for" << transferID;
        return;
    }
    stopAckTimer(transferID);
    qInfo() << "FileTransferManager: Received EOF_ACK for" << transferID << ". File" << session.fileName << "sent successfully.";
    cleanupSession(transferID, true, tr("File sent successfully."));
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
    
    stopAckTimer(transferID);
    FileTransferSession session = m_sessions.take(transferID);

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

void FileTransferManager::startAckTimer(const QString& transferID) {
    stopAckTimer(transferID);

    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, transferID]() {
        handleTransferTimeout(transferID);
    });
    timer->start(30000);
    m_transferTimers.insert(transferID, timer);
}

void FileTransferManager::stopAckTimer(const QString& transferID) {
    if (m_transferTimers.contains(transferID)) {
        QTimer* timer = m_transferTimers.take(transferID);
        timer->stop();
        timer->deleteLater();
    }
}

void FileTransferManager::handleTransferTimeout(const QString& transferID) {
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    qWarning() << "FileTransferManager: ACK Timeout for transfer" << transferID << "ChunkID:" << session.currentChunkID;
    
    sendError(session.peerUuid, transferID, "ACK_TIMEOUT", "Timeout waiting for acknowledgment.");
    cleanupSession(transferID, false, tr("Timeout waiting for acknowledgment from peer."));
}

void FileTransferManager::processNextChunk(const QString& transferID)
{
    Q_UNUSED(transferID);
}

