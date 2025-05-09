#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <iostream> // For stderr output

// Global log file variable (or pass it around if preferred)
QFile* logFile = nullptr;

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
    if (logFile && logFile->isOpen()) {
        QTextStream out(logFile);
        out << logEntry << Qt::endl;
        out.flush(); // Ensure it's written immediately
    }

    if (type == QtFatalMsg) {
        if (logFile) {
            logFile->close();
            delete logFile;
            logFile = nullptr;
        }
        abort();
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv); // <-- Initialize QApplication

    // Set up log file after QApplication initialization
    QString logFilePath = QDir(QCoreApplication::applicationDirPath()).filePath("chatapp_debug.log");
    logFile = new QFile(logFilePath);
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Logging to: " << logFilePath << Qt::endl;
    } else {
        QTextStream(stderr) << "Failed to open log file: " << logFilePath << ". Error: " << logFile->errorString() << Qt::endl;
        delete logFile;
        logFile = nullptr;
    }

    qInstallMessageHandler(customMessageOutput);
    qInfo() << "Application started.";
    qInfo() << "Organization:" << QCoreApplication::organizationName() << "App Name:" << QCoreApplication::applicationName();
    qInfo() << "Settings file path for this instance will be based on the above.";

    MainWindow w;
    w.show();

    int result = a.exec();
    qInfo() << "Application finished with exit code" << result;

    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    return result;
}
