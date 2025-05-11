#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QString>
#include <QSpinBox>
#include <QCheckBox>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QLabel;
class QSpinBox;
class QCheckBox;
class QTabWidget;
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const QString &currentUserName,
                            const QString &currentUserUuid,
                            quint16 currentListenPort,
                            bool currentAutoListenEnabled,
                            quint16 currentOutgoingPort,
                            bool currentUseSpecificOutgoing,
                            bool currentUdpDiscoveryEnabled,
                            quint16 currentUdpDiscoveryPort,
                            bool currentContinuousUdpBroadcastEnabled, // Added
                            int currentBroadcastIntervalSeconds,      // Added
                            QWidget *parent = nullptr);
    ~SettingsDialog();

    // Getters for settings
    QString getUserName() const;
    quint16 getListenPort() const;
    bool isListeningEnabled() const;
    quint16 getOutgoingPort() const;
    bool isSpecificOutgoingPortSelected() const;
    bool isUdpDiscoveryEnabled() const;
    quint16 getUdpDiscoveryPort() const;
    bool isContinuousUdpBroadcastEnabled() const; // Added
    int getBroadcastIntervalSeconds() const;      // Added

    void updateFields(const QString &userName, const QString &uuid,
                      quint16 listenPort, bool enableListening,
                      quint16 outgoingPort, bool useSpecificOutgoing,
                      bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                      bool continuousBroadcast, int intervalSeconds); // Added

signals:
    void settingsApplied(const QString &userName,
                         quint16 listenPort,
                         bool enableListening,
                         quint16 outgoingPort, bool useSpecificOutgoing,
                         bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                         bool continuousBroadcast, int intervalSeconds); // Added
    void retryListenNowRequested();
    void manualUdpBroadcastRequested();

private slots:
    void onSaveButtonClicked();
    void onOutgoingPortSettingsChanged();
    void onEnableListeningChanged(bool checked);
    void onRetryListenNowClicked();
    void onUdpDiscoveryEnableChanged(bool checked);
    void onManualBroadcastClicked();
    void onContinuousUdpBroadcastEnableChanged(bool checked); // Added

private:
    void setupUI();

    QTabWidget *tabWidget;

    // UI Elements - General Tab
    QLineEdit *userNameEdit;
    QLineEdit *userUuidEdit;

    // UI Elements - Network Tab (TCP)
    QCheckBox *enableListeningCheckBox;
    QSpinBox *listenPortSpinBox;
    QPushButton *retryListenButton;

    QCheckBox *specifyOutgoingPortCheckBox;
    QSpinBox *outgoingPortSpinBox;

    // UI Elements - Network Tab (UDP Discovery)
    QCheckBox *udpDiscoveryCheckBox;
    QSpinBox *udpDiscoveryPortSpinBox;
    QCheckBox *continuousUdpBroadcastCheckBox; // Added
    QSpinBox *broadcastIntervalSpinBox;      // Added
    QLabel *broadcastIntervalLabel;          // Added for clarity
    QPushButton *manualBroadcastButton;

    QPushButton *saveButton;
    QPushButton *cancelButton;

    // Store initial values
    QString initialUserName;
    QString initialUserUuid;
    quint16 initialListenPort;
    bool initialAutoListenEnabled;
    quint16 initialOutgoingPort;
    bool initialUseSpecificOutgoing;
    bool initialUdpDiscoveryEnabled;
    quint16 initialUdpDiscoveryPort;
    bool initialContinuousUdpBroadcastEnabled; // Added
    int initialBroadcastIntervalSeconds;      // Added
};

#endif // SETTINGSDIALOG_H
