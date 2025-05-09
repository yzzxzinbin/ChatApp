#include "peerinfowidget.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QCoreApplication> // For Q_DECLARE_TR_FUNCTIONS

class PeerInfoWidgetPrivate // Q_DECLARE_TR_FUNCTIONS needs a class context
{
public:
    Q_DECLARE_TR_FUNCTIONS(PeerInfoWidget)
};


PeerInfoWidget::PeerInfoWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("peerInfoWidget"); // Keep the object name for styling
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(10);

    peerNameLabel = new QLabel(PeerInfoWidgetPrivate::tr("Peer: N/A"), this);
    peerUuidLabel = new QLabel(PeerInfoWidgetPrivate::tr("UUID: N/A"), this);
    peerAddressLabel = new QLabel(PeerInfoWidgetPrivate::tr("Addr: N/A"), this);

    peerNameLabel->setObjectName("peerInfoLabel"); // Keep for styling
    peerUuidLabel->setObjectName("peerInfoLabel"); // Keep for styling
    peerAddressLabel->setObjectName("peerInfoLabel"); // Keep for styling

    peerUuidLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(peerNameLabel);
    layout->addWidget(peerUuidLabel);
    layout->addWidget(peerAddressLabel);
    layout->addStretch();

    setLayout(layout);
    setVisible(false); // Initially hidden
}

void PeerInfoWidget::updateDisplay(const QString& name, const QString& uuid, const QString& address, quint16 port)
{
    peerNameLabel->setText(PeerInfoWidgetPrivate::tr("Peer: %1").arg(name));
    peerUuidLabel->setText(PeerInfoWidgetPrivate::tr("UUID: %1").arg(uuid));
    if (port > 0) {
        peerAddressLabel->setText(PeerInfoWidgetPrivate::tr("Addr: %1:%2").arg(address).arg(port));
    } else if (!address.isEmpty() && address != PeerInfoWidgetPrivate::tr("N/A")) {
        peerAddressLabel->setText(PeerInfoWidgetPrivate::tr("Addr: %1").arg(address));
    } else {
        peerAddressLabel->setText(PeerInfoWidgetPrivate::tr("Addr: N/A"));
    }
    setVisible(true);
}

void PeerInfoWidget::clearDisplay()
{
    peerNameLabel->setText(PeerInfoWidgetPrivate::tr("Peer: N/A"));
    peerUuidLabel->setText(PeerInfoWidgetPrivate::tr("UUID: N/A"));
    peerAddressLabel->setText(PeerInfoWidgetPrivate::tr("Addr: N/A"));
    setVisible(false);
}

void PeerInfoWidget::setDisconnectedState(const QString& currentName, const QString& currentUuid)
{
    peerNameLabel->setText(PeerInfoWidgetPrivate::tr("Peer: %1").arg(currentName.isEmpty() ? PeerInfoWidgetPrivate::tr("N/A") : currentName));
    peerUuidLabel->setText(PeerInfoWidgetPrivate::tr("UUID: %1").arg(currentUuid.isEmpty() ? PeerInfoWidgetPrivate::tr("N/A") : currentUuid));
    peerAddressLabel->setText(PeerInfoWidgetPrivate::tr("Addr: Disconnected"));
    setVisible(true); // Keep visible to show disconnected state
}
