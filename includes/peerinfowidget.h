#ifndef PEERINFOWIDGET_H
#define PEERINFOWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QHBoxLayout;
QT_END_NAMESPACE

class PeerInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PeerInfoWidget(QWidget *parent = nullptr);
    void updateDisplay(const QString& name, const QString& uuid, const QString& address, quint16 port);
    void clearDisplay();
    void setDisconnectedState(const QString& currentName, const QString& currentUuid);

private:
    QLabel* peerNameLabel;
    QLabel* peerUuidLabel;
    QLabel* peerAddressLabel;
};

#endif // PEERINFOWIDGET_H
