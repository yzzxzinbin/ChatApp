#include "fileiomanager.h"
#include <QFile>
#include <QDebug>
#include <QThread> // For QThread::currentThreadId()

// 如果要在信号槽中直接传递自定义结构体，需要注册
// Q_DECLARE_METATYPE(FileReadResult)
// Q_DECLARE_METATYPE(FileWriteResult)
// int id = qRegisterMetaType<FileReadResult>();
// id = qRegisterMetaType<FileWriteResult>();


FileIOManager::FileIOManager(QObject *parent) : QObject(parent)
{
    // qRegisterMetaType<FileReadResult>(); // 确保类型已注册
    // qRegisterMetaType<FileWriteResult>();
}

FileIOManager::~FileIOManager()
{
    // Cleanup any pending watchers if they were used and stored
}

FileReadResult FileIOManager::performRead(QString transferID, qint64 chunkID, QString filePath, qint64 offset, int size)
{
    // qDebug() << "FileIOManager::performRead on thread:" << QThread::currentThreadId();
    QFile file(filePath);
    FileReadResult result;
    result.transferID = transferID;
    result.chunkID = chunkID;
    result.success = false;
    result.originalSize = 0;

    if (!file.open(QIODevice::ReadOnly)) {
        result.errorString = QString("Failed to open file %1: %2").arg(filePath).arg(file.errorString());
        return result;
    }

    if (!file.seek(offset)) {
        result.errorString = QString("Failed to seek to offset %1 in file %2: %3").arg(offset).arg(filePath).arg(file.errorString());
        file.close();
        return result;
    }

    QByteArray rawData = file.read(size);
    if (rawData.isNull() && file.error() != QFileDevice::NoError) { // read() returns null on error
        result.errorString = QString("Failed to read from file %1: %2").arg(filePath).arg(file.errorString());
    } else {
        result.originalSize = rawData.size();
        result.dataB64 = QString::fromUtf8(rawData.toBase64());
        result.success = true;
    }

    file.close();
    return result;
}

FileWriteResult FileIOManager::performWrite(QString transferID, qint64 chunkID, QString filePath, qint64 offset, QString dataB64, qint64 originalChunkSize)
{
    // qDebug() << "FileIOManager::performWrite on thread:" << QThread::currentThreadId();
    QFile file(filePath);
    FileWriteResult result;
    result.transferID = transferID;
    result.chunkID = chunkID;
    result.success = false;
    result.bytesWritten = 0;

    QByteArray decodedData = QByteArray::fromBase64(dataB64.toUtf8());

    if (decodedData.size() != originalChunkSize) {
        result.errorString = QString("Decoded data size mismatch for chunk %1. Expected %2, got %3.")
                                 .arg(chunkID).arg(originalChunkSize).arg(decodedData.size());
        return result;
    }

    if (!file.open(QIODevice::ReadWrite)) { // Use ReadWrite to allow seek
        result.errorString = QString("Failed to open file %1 for writing: %2").arg(filePath).arg(file.errorString());
        return result;
    }

    if (!file.seek(offset)) {
        result.errorString = QString("Failed to seek to offset %1 for writing in file %2: %3").arg(offset).arg(filePath).arg(file.errorString());
        file.close();
        return result;
    }

    qint64 bytesWrittenToFile = file.write(decodedData);
    if (bytesWrittenToFile != decodedData.size()) {
        result.errorString = QString("Failed to write complete data to file %1 (wrote %2 of %3 bytes): %4")
                                 .arg(filePath).arg(bytesWrittenToFile).arg(decodedData.size()).arg(file.errorString());
        // result.success remains false
    } else {
        result.bytesWritten = bytesWrittenToFile;
        result.success = true;
    }

    file.close();
    return result;
}

void FileIOManager::requestReadFileChunk(const QString& transferID, qint64 chunkID, const QString& filePath, qint64 offset, int size)
{
    // Use a QFutureWatcher to manage the asynchronous task and get results on the main thread
    QFutureWatcher<FileReadResult> *watcher = new QFutureWatcher<FileReadResult>(this);
    connect(watcher, &QFutureWatcher<FileReadResult>::finished, this, [this, watcher]() {
        FileReadResult result = watcher->result();
        emit chunkReadCompleted(result.transferID, result.chunkID, result.dataB64, result.originalSize, result.success, result.errorString);
        watcher->deleteLater(); // Clean up the watcher
    });

    QFuture<FileReadResult> future = QtConcurrent::run(&FileIOManager::performRead, transferID, chunkID, filePath, offset, size);
    watcher->setFuture(future);
}

void FileIOManager::requestWriteFileChunk(const QString& transferID, qint64 chunkID, const QString& filePath, qint64 offset, const QString& dataB64, qint64 originalChunkSize)
{
    QFutureWatcher<FileWriteResult> *watcher = new QFutureWatcher<FileWriteResult>(this);
    connect(watcher, &QFutureWatcher<FileWriteResult>::finished, this, [this, watcher]() {
        FileWriteResult result = watcher->result();
        emit chunkWrittenCompleted(result.transferID, result.chunkID, result.bytesWritten, result.success, result.errorString);
        watcher->deleteLater();
    });

    QFuture<FileWriteResult> future = QtConcurrent::run(&FileIOManager::performWrite, transferID, chunkID, filePath, offset, dataB64, originalChunkSize);
    watcher->setFuture(future);
}
