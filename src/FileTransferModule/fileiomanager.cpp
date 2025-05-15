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

    if (!file.open(QIODevice::ReadOnly)) {
        result.errorString = QString("Failed to open file %1: %2").arg(filePath).arg(file.errorString());
        return result;
    }

    if (!file.seek(offset)) {
        result.errorString = QString("Failed to seek to offset %1 in file %2: %3").arg(offset).arg(filePath).arg(file.errorString());
        file.close();
        return result;
    }

    result.data = file.read(size);
    if (result.data.isNull() && file.error() != QFileDevice::NoError) { // read() returns null on error
        result.errorString = QString("Failed to read from file %1: %2").arg(filePath).arg(file.errorString());
    } else if (result.data.size() != size && !file.atEnd() && result.data.size() < size) {
        // This case (reading less than requested but not at EOF and no error) is unusual for local files
        // but could happen. For simplicity, we might treat it as an error or a partial success.
        // For now, if not atEnd and size mismatch, consider it an issue.
        // If atEnd, it's normal for the last chunk.
        // The check `chunkData.isEmpty() && !session.file->atEnd()` in FTM was for this.
        // Here, if data.isNull() it's an error. If data.size() < size and atEnd(), it's fine.
        // If data.size() < size and !atEnd(), it's potentially problematic.
        // For now, we assume read() behaves as expected or returns null on error.
        result.success = true; // Assume success if no direct error from read()
    } else {
        result.success = true;
    }

    file.close();
    return result;
}

FileWriteResult FileIOManager::performWrite(QString transferID, qint64 chunkID, QString filePath, qint64 offset, QByteArray data)
{
    // qDebug() << "FileIOManager::performWrite on thread:" << QThread::currentThreadId();
    QFile file(filePath);
    FileWriteResult result;
    result.transferID = transferID;
    result.chunkID = chunkID;
    result.success = false;

    // For writing, we typically want to ensure the file grows or we write at a specific location.
    // If using offset, QIODevice::ReadWrite might be more appropriate to allow seeking then writing.
    // If always appending sequentially based on previous writes, QIODevice::Append is fine.
    // Given our receiver logic writes contiguous chunks, and `offset` is the expected start of this chunk:
    if (!file.open(QIODevice::ReadWrite)) { // Use ReadWrite to allow seek
        result.errorString = QString("Failed to open file %1 for writing: %2").arg(filePath).arg(file.errorString());
        return result;
    }

    if (!file.seek(offset)) {
        result.errorString = QString("Failed to seek to offset %1 for writing in file %2: %3").arg(offset).arg(filePath).arg(file.errorString());
        file.close();
        return result;
    }

    result.bytesWritten = file.write(data);
    if (result.bytesWritten != data.size()) {
        result.errorString = QString("Failed to write complete data to file %1 (wrote %2 of %3 bytes): %4")
                                 .arg(filePath).arg(result.bytesWritten).arg(data.size()).arg(file.errorString());
        // result.success remains false
    } else {
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
        emit chunkReadCompleted(result.transferID, result.chunkID, result.data, result.success, result.errorString);
        watcher->deleteLater(); // Clean up the watcher
    });

    QFuture<FileReadResult> future = QtConcurrent::run(&FileIOManager::performRead, transferID, chunkID, filePath, offset, size);
    watcher->setFuture(future);
}

void FileIOManager::requestWriteFileChunk(const QString& transferID, qint64 chunkID, const QString& filePath, qint64 offset, const QByteArray& data)
{
    QFutureWatcher<FileWriteResult> *watcher = new QFutureWatcher<FileWriteResult>(this);
    connect(watcher, &QFutureWatcher<FileWriteResult>::finished, this, [this, watcher]() {
        FileWriteResult result = watcher->result();
        emit chunkWrittenCompleted(result.transferID, result.chunkID, result.bytesWritten, result.success, result.errorString);
        watcher->deleteLater();
    });

    QFuture<FileWriteResult> future = QtConcurrent::run(&FileIOManager::performWrite, transferID, chunkID, filePath, offset, data);
    watcher->setFuture(future);
}
