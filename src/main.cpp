#include "mainwindow.h"
#include "logindialog.h" // 新增：包含 LoginDialog
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <iostream> // For stderr output
#include <QSettings>    // To log settings file path
#include <QIcon>      // 新增：用于设置图标
#include <QMessageBox> // For showing "already running" message
#include  <QStandardPaths>

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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置组织和固定的应用程序名称
    QCoreApplication::setOrganizationName("YourOrgName"); // 替换为您的组织名
    QCoreApplication::setApplicationName("ChatApp");    // 固定应用名

    // 初始化日志记录
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs";
    if (!QDir().mkpath(logDir)) {
        std::cerr << "Failed to create log directory: " << qPrintable(logDir) << std::endl;
    }
    QString logFilePath = logDir + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + ".log";
    g_logFile = new QFile(logFilePath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(customMessageOutput);
        qInfo() << "Log file opened:" << logFilePath;
    } else {
        qCritical("Failed to open log file: %s. Error: %s", qPrintable(logFilePath), qPrintable(g_logFile->errorString()));
        delete g_logFile; 
        g_logFile = nullptr;
    }

    qInfo() << "Application instance starting. Effective Name:" << QCoreApplication::applicationName()
            << ", Organization:" << QCoreApplication::organizationName();
    QSettings settings; 
    qInfo() << "Settings file for this instance will be at:" << settings.fileName();

    // 显示登录对话框
    LoginDialog loginDialog;
    loginDialog.setWindowIcon(QIcon(":/icons/app_logo.ico")); 
    
    QString loggedInUserIdStr; // 用于存储成功登录的用户ID

    if (loginDialog.exec() == QDialog::Accepted) {
        loggedInUserIdStr = loginDialog.getLoggedInUserId(); // 从LoginDialog获取用户ID
        if (loggedInUserIdStr.isEmpty()) {
            qCritical() << "Login was accepted, but no User ID was returned. Exiting.";
            // 清理操作
            if (g_logFile) {
                if (g_logFile->isOpen()) {
                    g_logFile->close();
                }
                delete g_logFile;
                g_logFile = nullptr;
            }
            return 1;
        }
        qInfo() << "Login successful for User ID:" << loggedInUserIdStr;
    } else {
        qInfo() << "Login cancelled or failed. Exiting application.";
        // 清理操作与应用程序正常退出时相同
        if (g_logFile) {
            if (g_logFile->isOpen()) {
                g_logFile->close();
            }
            delete g_logFile;
            g_logFile = nullptr;
        }
        return 0; // 用户未登录，退出应用
    }

    qInfo() << "Login successful. Attempting to construct MainWindow...";
    MainWindow w(loggedInUserIdStr); // 将登录的用户ID传递给MainWindow
    qInfo() << "MainWindow constructed. Attempting to show...";
    w.setWindowIcon(QIcon(":/icons/app_logo.ico")); 
    w.setWindowTitle(QString("%1 (User: %2) - By CCZU_ZX").arg(QCoreApplication::applicationName()).arg(loggedInUserIdStr)); // 标题栏显示用户名
    w.show();
    qInfo() << "MainWindow show() called.";

    int result = a.exec();

    qInfo() << "Application instance" << QCoreApplication::applicationName() << "finished with exit code" << result;

    // Cleanup
    qInstallMessageHandler(nullptr);
    if (g_logFile) {
        if (g_logFile->isOpen()) {
            g_logFile->close();
        }
        delete g_logFile;
        g_logFile = nullptr;
    }
    
    return result;
}
