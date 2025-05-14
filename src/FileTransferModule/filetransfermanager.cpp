#include "filetransfermanager.h"
#include "networkmanager.h" // Include full definition
#include <QUuid>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>

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
    for (FileTransferSession& session : m_sessions) {
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
    // session.file will be opened if accepted. For now, just store path info.
    // For actual sending, you might open it here or upon acceptance.
    
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
         if (transferID.isEmpty() || receiverUuid.isEmpty() || receiverUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_ACCEPT received:" << message;
            return;
        }
        handleFileAccept(peerUuid, transferID);

    } else if (message.startsWith("<FT_REJECT")) {
        QString transferID = extractMessageAttribute(message, "TransferID");
        QString reason = extractMessageAttribute(message, "Reason");
        QString receiverUuid = extractMessageAttribute(message, "ReceiverUUID");
        if (transferID.isEmpty() || receiverUuid.isEmpty() || receiverUuid != peerUuid) {
            qWarning() << "FileTransferManager: Invalid FT_REJECT received:" << message;
            return;
        }
        handleFileReject(peerUuid, transferID, reason);
    }
    // Add handlers for other FT_MSG types later
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
    m_sessions.insert(transferID, session);

    qInfo() << "FileTransferManager: Received file offer for" << fileName << "from" << peerUuid << "TransferID:" << transferID;
    emit incomingFileOffer(transferID, peerUuid, fileName, fileSize);
}

void FileTransferManager::acceptFileOffer(const QString& transferID)
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

    session.state = FileTransferSession::Accepted;
    sendAcceptMessage(session.peerUuid, transferID);
    qInfo() << "FileTransferManager: Accepted file offer for TransferID" << transferID << "from" << session.peerUuid;
    
    prepareToReceiveFile(transferID); // Placeholder for actual receive logic
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, false);
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
    emit fileTransferFinished(transferID, session.peerUuid, session.fileName, false, tr("Rejected by user: %1").arg(reason));
    m_sessions.remove(transferID); // Clean up rejected session
}


void FileTransferManager::sendAcceptMessage(const QString& peerUuid, const QString& transferID)
{
    QString acceptMessage = FT_MSG_ACCEPT_FORMAT.arg(transferID).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, acceptMessage);
    qDebug() << "FileTransferManager: Sent file accept:" << acceptMessage << "to" << peerUuid;
}

void FileTransferManager::sendRejectMessage(const QString& peerUuid, const QString& transferID, const QString& reason)
{
    QString rejectMessage = FT_MSG_REJECT_FORMAT.arg(transferID).arg(reason).arg(m_localUserUuid);
    m_networkManager->sendMessage(peerUuid, rejectMessage);
    qDebug() << "FileTransferManager: Sent file reject:" << rejectMessage << "to" << peerUuid;
}

void FileTransferManager::handleFileAccept(const QString& peerUuid, const QString& transferID)
{
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
    
    startActualFileSend(transferID); // Placeholder for actual send logic
    emit fileTransferStarted(transferID, session.peerUuid, session.fileName, true);
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
    emit fileTransferFinished(transferID, session.peerUuid, session.fileName, false, tr("Rejected by peer: %1").arg(reason));
    m_sessions.remove(transferID); // Clean up rejected session
}

void FileTransferManager::startActualFileSend(const QString& transferID) {
    // Placeholder: This is where you'd open the file, start reading chunks, and send them.
    // For now, just log and emit progress/finish for testing.
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    session.state = FileTransferSession::Transferring;
    qDebug() << "FileTransferManager: Starting to 'send' file for TransferID" << transferID;
    
    // Simulate transfer
    emit fileTransferProgress(transferID, session.fileSize / 2, session.fileSize);
    emit fileTransferProgress(transferID, session.fileSize, session.fileSize);
    session.bytesTransferred = session.fileSize;
    session.state = FileTransferSession::Completed;
    emit fileTransferFinished(transferID, session.peerUuid, session.fileName, true, "File sent successfully (simulated).");
    // m_sessions.remove(transferID); // Or keep for history
}

void FileTransferManager::prepareToReceiveFile(const QString& transferID) {
    // Placeholder: This is where you'd prepare to receive chunks (e.g., open a temporary file).
    // For now, just log.
    if (!m_sessions.contains(transferID)) return;
    FileTransferSession& session = m_sessions[transferID];
    session.state = FileTransferSession::Transferring;
    qDebug() << "FileTransferManager: Preparing to 'receive' file for TransferID" << transferID;
    // Simulate receiving
    // In a real scenario, you'd receive chunks and update progress.
    // For now, we'll assume the sender will drive the 'completion'.
}


void FileTransferManager::processNextChunk(const QString& transferID)
{
    // This will be implemented later for actual chunk-by-chunk sending.
    Q_UNUSED(transferID);
}

