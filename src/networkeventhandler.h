#ifndef NETWORKEVENTHANDLER_H
#define NETWORKEVENTHANDLER_H

#include <QObject>
#include <QAbstractSocket> // For SocketError

QT_BEGIN_NAMESPACE
class NetworkManager;
class QListWidget;
class QListWidgetItem;
class ChatMessageDisplay;
class PeerInfoWidget;
class QStackedWidget;
class QTextEdit;
class QLabel;
template <class Key, class T> class QMap; // Forward declare QMap
class QStringList;
class MainWindow; // Forward declaration
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
        QObject *parent = nullptr);

public slots:
    void handleNetworkConnected();
    void handleNetworkDisconnected();
    void handleNewMessageReceived(const QString &message);
    void handleNetworkError(QAbstractSocket::SocketError socketError);

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
};

#endif // NETWORKEVENTHANDLER_H
