#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <iostream> // For stderr output
#include <QSharedMemory> // Required for instance management using shared memory
#include <QSettings>    // To log settings file path

// Global shared memory object to lock the instance name
static QSharedMemory* g_instanceLockMemory = nullptr;
// Global log file variable
static QFile* g_logFile = nullptr;

// Custom message handler function
void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString logEntry;
    QTextStream consoleErr(stderr);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    logEntry.append(timestamp);

    switch (type) {
    case QtDebugMsg:
        logEntry.append(" [DEBUG] ");
        break;
    case QtInfoMsg:
        logEntry.append(" [INFO] ");
        break;
    case QtWarningMsg:
        logEntry.append(" [WARNING] ");
        break;
    case QtCriticalMsg:
        logEntry.append(" [CRITICAL] ");
        break;
    case QtFatalMsg:
        logEntry.append(" [FATAL] ");
        break;
    }

    logEntry.append(msg);
    if (context.file && !QString(context.file).isEmpty()) {
        logEntry.append(QString(" (%1:%2, %3)").arg(context.file).arg(context.line).arg(context.function));
    }

    // Output to stderr as well
    consoleErr << logEntry << Qt::endl;

    // Output to log file
    if (g_logFile && g_logFile->isOpen()) { // Use g_logFile
        QTextStream out(g_logFile);
        out << logEntry << Qt::endl;
        out.flush(); // Ensure it's written immediately
    }

    if (type == QtFatalMsg) {
        if (g_logFile) { // Use g_logFile
            g_logFile->close();
            delete g_logFile;
            g_logFile = nullptr;
        }
        abort();
    }
}

// Function to determine the instance name and acquire a lock using QSharedMemory
QString determineApplicationInstanceName(QString& outInstanceSuffix) {
    QFile instLogFile(QDir(QCoreApplication::applicationDirPath()).filePath("instance_check_sm.log"));
    if (!instLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "CRITICAL: Could not open instance_check_sm.log for writing." << Qt::endl;
    }
    QTextStream instLog(&instLogFile);
    instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Starting instance check with QSharedMemory.\n";

    const QString baseAppName = QStringLiteral("ChatApp");
    // IMPORTANT: QSharedMemory keys must be unique system-wide.
    // Using a UUID or a very specific string is recommended.
    const QString baseSharedMemoryKey = QStringLiteral("ChatAppSharedMemoryKey_d7b3f8a0_c1e5_4b9f_8f2a_1b9c7d8e0f3a");
    outInstanceSuffix = QStringLiteral("");
    QTextStream errStream(stderr);

    errStream << "determineApplicationInstanceName (SharedMemory): Starting. BaseKey: " << baseSharedMemoryKey << Qt::endl;
    instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] BaseKey: " << baseSharedMemoryKey << "\n";

    g_instanceLockMemory = new QSharedMemory(baseSharedMemoryKey);
    instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Attempting to create shared memory with base key: " << baseSharedMemoryKey << "\n";

    // Attempt to create the shared memory segment (size 1 is enough for a lock)
    if (g_instanceLockMemory->create(1, QSharedMemory::ReadWrite)) {
        errStream << "determineApplicationInstanceName (SharedMemory): Successfully created shared memory with base key. This is MAIN instance." << Qt::endl;
        instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Successfully created base key. MAIN instance. Key: " << g_instanceLockMemory->key() << "\n";
        // qInfo() might not be caught yet if handler not installed
        // qInfo() << "This is the first instance (main). SharedMemory lock acquired:" << g_instanceLockMemory->key();
        if (instLogFile.isOpen()) instLogFile.close();
        return baseAppName;
    } else {
        // Creation failed, check why
        errStream << "determineApplicationInstanceName (SharedMemory): Failed to create shared memory with base key. Error: "
                  << g_instanceLockMemory->errorString() << " (Code: " << g_instanceLockMemory->error() << ")" << Qt::endl;
        instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Failed to create with base key. Error: "
                  << g_instanceLockMemory->errorString() << " (Code: " << g_instanceLockMemory->error() << ")\n";

        if (g_instanceLockMemory->error() != QSharedMemory::AlreadyExists) {
            errStream << "determineApplicationInstanceName (SharedMemory): Error is NOT AlreadyExists. Proceeding as main, potential conflict." << Qt::endl;
            instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Error is NOT AlreadyExists (" << g_instanceLockMemory->errorString() << "). Proceeding as main.\n";
            // qCritical() might not be caught yet
            // qCritical() << "Could not create/check shared memory" << baseSharedMemoryKey << ":" << g_instanceLockMemory->errorString()
            //             << ".Proceeding as main instance, but conflicts are possible.";
            delete g_instanceLockMemory; // Clean up the failed attempt
            g_instanceLockMemory = nullptr; // No lock acquired
            if (instLogFile.isOpen()) instLogFile.close();
            return baseAppName; // Fallback to main instance name
        }
        // AlreadyExists: Main instance is (likely) running.
        errStream << "determineApplicationInstanceName (SharedMemory): Base key in use. Main instance detected. Trying suffixes." << Qt::endl;
        instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Base key in use. Trying suffixes.\n";
        delete g_instanceLockMemory; // Delete the QSharedMemory object that failed to create
        g_instanceLockMemory = nullptr;
        // qInfo() << "Main instance detected (shared memory). Attempting to start as a suffixed instance.";
    }

    for (char c = 'A'; c <= 'Z'; ++c) {
        outInstanceSuffix = QString("_%1").arg(c);
        QString currentSharedMemoryKey = baseSharedMemoryKey + outInstanceSuffix;
        errStream << "determineApplicationInstanceName (SharedMemory): Trying suffix " << outInstanceSuffix << " with key " << currentSharedMemoryKey << Qt::endl;
        instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Trying suffix " << outInstanceSuffix << " with key " << currentSharedMemoryKey << "\n";
        
        g_instanceLockMemory = new QSharedMemory(currentSharedMemoryKey);
        instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Attempting to create shared memory with key: " << currentSharedMemoryKey << "\n";

        if (g_instanceLockMemory->create(1, QSharedMemory::ReadWrite)) {
            errStream << "determineApplicationInstanceName (SharedMemory): Successfully created shared memory with key " << currentSharedMemoryKey << ". This is instance " << outInstanceSuffix << Qt::endl;
            instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Successfully created suffixed key. Instance " << outInstanceSuffix << ". Key: " << g_instanceLockMemory->key() << "\n";
            // qInfo() << "This is instance" << outInstanceSuffix << ". SharedMemory lock acquired:" << g_instanceLockMemory->key();
            if (instLogFile.isOpen()) instLogFile.close();
            return baseAppName + outInstanceSuffix;
        } else {
            errStream << "determineApplicationInstanceName (SharedMemory): Failed to create shared memory with key " << currentSharedMemoryKey << ". Error: "
                      << g_instanceLockMemory->errorString() << " (Code: " << g_instanceLockMemory->error() << ")" << Qt::endl;
            instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Failed to create with key " << currentSharedMemoryKey << ". Error: "
                      << g_instanceLockMemory->errorString() << " (Code: " << g_instanceLockMemory->error() << ")\n";

            if (g_instanceLockMemory->error() != QSharedMemory::AlreadyExists) {
                errStream << "determineApplicationInstanceName (SharedMemory): Error for suffix " << outInstanceSuffix << " is NOT AlreadyExists. Proceeding as this instance, potential conflict." << Qt::endl;
                instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] Error for suffix " << outInstanceSuffix << " is NOT AlreadyExists. Proceeding as this instance.\n";
                // qCritical() << "Could not create/check shared memory" << currentSharedMemoryKey << ":" << g_instanceLockMemory->errorString()
                //             << ".Proceeding as instance" << outInstanceSuffix << ", but conflicts are possible.";
                delete g_instanceLockMemory;
                g_instanceLockMemory = nullptr; // No lock acquired
                if (instLogFile.isOpen()) instLogFile.close();
                return baseAppName + outInstanceSuffix; // Return with suffix despite error
            }
            // AlreadyExists: This suffix is also taken. Clean up and try next.
            delete g_instanceLockMemory;
            g_instanceLockMemory = nullptr;
        }
    }

    errStream << "determineApplicationInstanceName (SharedMemory): All suffixes A-Z taken. Running as _Overflow." << Qt::endl;
    instLog << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [DEBUG_SM_INST] All suffixes A-Z taken. Running as _Overflow.\n";
    // qWarning() << "All instance suffixes (_A to _Z) are taken using SharedMemory. Running with _Overflow suffix. Conflicts are highly probable.";
    outInstanceSuffix = QStringLiteral("_Overflow");
    // No shared memory segment is held for _Overflow, making it prone to conflicts if multiple overflows occur.
    if (instLogFile.isOpen()) instLogFile.close();
    return baseAppName + outInstanceSuffix;
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv); // QApplication must be created first.

    QString instanceSuffix;
    QString effectiveAppName = determineApplicationInstanceName(instanceSuffix);

    qInstallMessageHandler(customMessageOutput);

    QCoreApplication::setOrganizationName("YourCompany");
    QCoreApplication::setApplicationName(effectiveAppName); 

    QString logFileSuffixForName = instanceSuffix.isEmpty() ? "" : instanceSuffix;
    QString logFileName = QString("chatapp%1_debug.log").arg(logFileSuffixForName);
    QString logFilePath = QDir(QCoreApplication::applicationDirPath()).filePath(logFileName);
    
    if (g_logFile) {
        if (g_logFile->isOpen()) {
            g_logFile->close();
        }
        delete g_logFile;
        g_logFile = nullptr;
    }

    g_logFile = new QFile(logFilePath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInfo() << "Successfully opened log file:" << logFilePath;
    } else {
        qCritical("Failed to open log file: %s. Error: %s", qPrintable(logFilePath), qPrintable(g_logFile->errorString()));
        delete g_logFile; 
        g_logFile = nullptr;
    }

    qInfo() << "Application instance starting. Effective Name:" << QCoreApplication::applicationName()
            << ", Organization:" << QCoreApplication::organizationName();
    QSettings settings; 
    qInfo() << "Settings file for this instance will be at:" << settings.fileName();

    MainWindow w;
    w.setWindowTitle(QString("%1 - ChatApp").arg(QCoreApplication::applicationName()));
    w.show();

    int result = a.exec();

    qInfo() << "Application instance" << QCoreApplication::applicationName() << "finished with exit code" << result;

    // Cleanup
    if (g_instanceLockMemory) {
        // Detach from the shared memory segment. If this was the last process
        // attached to it, the OS might release it. However, explicit destruction
        // is not directly done via QSharedMemory unless you are the creator and
        // no one else is attached, or using native APIs.
        // For a lock, detaching is usually sufficient.
        if (g_instanceLockMemory->isAttached()) {
            g_instanceLockMemory->detach();
        }
        delete g_instanceLockMemory;
        g_instanceLockMemory = nullptr;
        qInfo() << "Instance lock (SharedMemory) released.";
    }
    if (g_logFile) {
        qInfo() << "Closing main log file.";
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
    return result;
}
