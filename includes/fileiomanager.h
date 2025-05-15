#ifndef FILEIOMANAGER_H
#define FILEIOMANAGER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QtConcurrent/QtConcurrent> // Required for QtConcurrent
#include <QFutureWatcher>          // Required for QFutureWatcher

// 用于从 QtConcurrent::run 返回包含多个值的结构体
struct FileReadResult {
    QString transferID;
    qint64 chunkID;
    QByteArray data;
    bool success;
    QString errorString;
};

struct FileWriteResult {
    QString transferID;
    qint64 chunkID;
    qint64 bytesWritten;
    bool success;
    QString errorString;
};

// Q_DECLARE_METATYPE(FileReadResult) // 如果要在信号槽中直接传递自定义结构体，需要注册
// Q_DECLARE_METATYPE(FileWriteResult)

class FileIOManager : public QObject
{
    Q_OBJECT
public:
    explicit FileIOManager(QObject *parent = nullptr);
    ~FileIOManager();

    // 请求异步读取文件块
    void requestReadFileChunk(const QString& transferID, qint64 chunkID, const QString& filePath, qint64 offset, int size);

    // 请求异步写入文件块
    void requestWriteFileChunk(const QString& transferID, qint64 chunkID, const QString& filePath, qint64 offset, const QByteArray& data);

signals:
    // 文件块读取完成信号
    void chunkReadCompleted(const QString& transferID, qint64 chunkID, const QByteArray& data, bool success, const QString& error);

    // 文件块写入完成信号
    void chunkWrittenCompleted(const QString& transferID, qint64 chunkID, qint64 bytesWritten, bool success, const QString& error);

private:
    // 辅助函数，实际在工作线程中执行读取
    static FileReadResult performRead(QString transferID, qint64 chunkID, QString filePath, qint64 offset, int size);
    // 辅助函数，实际在工作线程中执行写入
    static FileWriteResult performWrite(QString transferID, qint64 chunkID, QString filePath, qint64 offset, QByteArray data);

    // QMap to hold future watchers if needed for cancellation, though not strictly necessary for this simple model
    // QMap<QFuture<FileReadResult>, QFutureWatcher<FileReadResult>*> m_readWatchers;
    // QMap<QFuture<FileWriteResult>, QFutureWatcher<FileWriteResult>*> m_writeWatchers;
};

#endif // FILEIOMANAGER_H
