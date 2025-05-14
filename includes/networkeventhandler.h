#ifndef NETWORKEVENTHANDLER_H
#define NETWORKEVENTHANDLER_H

#include <QObject>
#include <QAbstractSocket> // For SocketError
#include <QStringList>     // Include for QStringList
#include <QMap>            // Include for QMap

QT_BEGIN_NAMESPACE
class NetworkManager;
class QListWidget;
class ChatMessageDisplay;
class PeerInfoWidget;
class QStackedWidget;
class QTextEdit;
class QLabel;
class MainWindow; // Forward declaration
class FileTransferManager; // Forward declaration
QT_END_NAMESPACE

class NetworkEventHandler : public QObject
{
    Q_OBJECT

public:
    explicit NetworkEventHandler(
        NetworkManager *nm,
        QListWidget *contactList,
        ChatMessageDisplay *msgDisplay,
        PeerInfoWidget *peerInfo,
        QStackedWidget *chatStack,
        QTextEdit *msgInput,
        QLabel *emptyPlaceholder,
        QWidget *activeChatWidget,
        QMap<QString, QStringList> *histories,
        MainWindow *mainWindow, // To access certain MainWindow methods/properties
        FileTransferManager *ftm, // To handle file transfers
        QObject *parent = nullptr);

public slots:
    void handlePeerConnected(const QString &peerUuid, const QString &peerName, const QString& peerAddress, quint16 peerPort);
    void handlePeerDisconnected(const QString &peerUuid);
    void handleNewMessageReceived(const QString &peerUuid, const QString &message);
    void handlePeerNetworkError(const QString &peerUuid, QAbstractSocket::SocketError socketError, const QString& errorString);

private:
    NetworkManager *networkManager;
    QListWidget *contactListWidget;
    ChatMessageDisplay *messageDisplay;
    PeerInfoWidget *peerInfoDisplayWidget;
    QStackedWidget *chatStackedWidget;
    QTextEdit *messageInputEdit;
    QLabel *emptyChatPlaceholderLabel;
    QWidget *activeChatContentsWidget;
    QMap<QString, QStringList> *chatHistories; // Pointer to MainWindow's chatHistories
    MainWindow *mainWindowPtr; // Pointer to MainWindow instance
    FileTransferManager *fileTransferManager; // Pointer to FileTransferManager instance
};

#endif // NETWORKEVENTHANDLER_H
