#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QString> 

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QLabel;
class QSpinBox;
class QCheckBox;
class QTabWidget; // Added
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const QString &currentUserName,
                            const QString &currentUserUuid,
                            quint16 currentListenPort,
                            bool currentEnableListening,
                            quint16 currentOutgoingPort, 
                            bool currentUseSpecificOutgoingPort,
                            bool currentEnableUdpDiscovery,
                            QWidget *parent = nullptr);

    QString getUserName() const;
    quint16 getListenPort() const;
    bool isListeningEnabled() const;
    quint16 getOutgoingPort() const;
    bool isSpecificOutgoingPortSelected() const;
    bool isUdpDiscoveryEnabled() const;

    void updateFields(const QString &userName,
                      const QString &uuid,
                      quint16 listenPort,
                      bool enableListening,
                      quint16 outgoingPort, 
                      bool useSpecificOutgoingPort,
                      bool enableUdpDiscovery);

signals:
    void settingsApplied(const QString &userName,
                         quint16 listenPort,
                         bool enableListening,
                         quint16 outgoingPort, 
                         bool useSpecificOutgoingPort,
                         bool enableUdpDiscovery);
    void retryListenNowRequested();
    void manualUdpBroadcastRequested();

private slots:
    void onSaveButtonClicked();
    void onOutgoingPortSettingsChanged();
    void onEnableListeningChanged(bool checked);
    void onRetryListenNowClicked();
    void onUdpDiscoveryEnableChanged(bool checked);
    void onManualBroadcastClicked();

private:
    void setupUI();

    QTabWidget *tabWidget; // Added

    // UI Elements - will be placed in tabs
    QLineEdit *userNameEdit;
    QLineEdit *userUuidEdit;

    QCheckBox *enableListeningCheckBox;
    QSpinBox *listenPortSpinBox;
    QPushButton *retryListenButton; 

    QCheckBox *specifyOutgoingPortCheckBox;
    QSpinBox *outgoingPortSpinBox;

    QCheckBox *udpDiscoveryCheckBox; 
    QPushButton *manualBroadcastButton; 

    QPushButton *saveButton;
    QPushButton *cancelButton;

    // Store initial values
    QString initialUserName;
    QString initialUserUuid;
    quint16 initialListenPort;
    bool initialEnableListening;
    quint16 initialOutgoingPort;
    bool initialUseSpecificOutgoingPort;
    bool initialEnableUdpDiscovery;
};

#endif // SETTINGSDIALOG_H
