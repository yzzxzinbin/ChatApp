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
class QTabWidget;
class QFileDialog; // 新增
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
                            bool currentContinuousUdpBroadcastEnabled,
                            int currentUdpBroadcastInterval,
                            // 新增参数
                            const QString &currentDefaultDownloadDir = QString(),
                            bool currentRequireFileAccept = true,
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
    bool isContinuousUdpBroadcastEnabled() const;
    int getUdpBroadcastInterval() const;
    // 新增getter
    QString getDefaultDownloadDir() const;
    bool isRequireFileAccept() const;

    void updateFields(const QString &userName, const QString &uuid,
                      quint16 listenPort, bool enableListening,
                      quint16 outgoingPort, bool useSpecificOutgoing,
                      bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                      bool enableContinuousUdpBroadcast, int udpBroadcastInterval,
                      // 新增参数
                      const QString &defaultDownloadDir,
                      bool requireFileAccept);

signals:
    void settingsApplied(const QString &userName,
                         quint16 listenPort,
                         bool enableListening,
                         quint16 outgoingPort, bool useSpecificOutgoing,
                         bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                         bool enableContinuousUdpBroadcast, int udpBroadcastInterval,
                         // 新增参数
                         const QString &defaultDownloadDir,
                         bool requireFileAccept);
    void retryListenNowRequested();
    void manualUdpBroadcastRequested();

private slots:
    void onSaveButtonClicked();
    void onOutgoingPortSettingsChanged();
    void onEnableListeningChanged(bool checked);
    void onRetryListenNowClicked();
    void onUdpDiscoveryEnableChanged(bool checked);
    void onManualBroadcastClicked();
    void onUdpContinuousBroadcastChanged(bool checked);
    void onSelectDownloadDirClicked(); // 新增槽

private:
    void setupUI();

    QTabWidget *tabWidget;

    // UI Elements - will be placed in tabs
    QLineEdit *userNameEdit;
    QLineEdit *userUuidEdit;

    QCheckBox *enableListeningCheckBox;
    QSpinBox *listenPortSpinBox;
    QPushButton *retryListenButton; 

    QCheckBox *specifyOutgoingPortCheckBox;
    QSpinBox *outgoingPortSpinBox;

    QCheckBox *udpDiscoveryCheckBox; 
    QSpinBox *udpDiscoveryPortSpinBox; 
    QPushButton *manualBroadcastButton; 
    QCheckBox *enableContinuousUdpBroadcastCheckBox;
    QSpinBox *udpBroadcastIntervalSpinBox;

    // 新增UI元素
    QLineEdit *downloadDirEdit;
    QPushButton *selectDownloadDirButton;
    QCheckBox *requireFileAcceptCheckBox;

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
    bool initialContinuousUdpBroadcastEnabled;
    int initialUdpBroadcastInterval;
    // 新增初始值
    QString initialDefaultDownloadDir;
    bool initialRequireFileAccept;
};

#endif // SETTINGSDIALOG_H
