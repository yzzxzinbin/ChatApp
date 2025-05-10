#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QString> // Ensure QString is included

QT_BEGIN_NAMESPACE
class QLineEdit;
class QRadioButton;
class QPushButton;
class QLabel;
class QSpinBox;
class QGroupBox;
class QCheckBox;
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
                            bool currentEnableUdpDiscovery, // 新增
                            QWidget *parent = nullptr);

    QString getUserName() const;
    quint16 getListenPort() const;
    bool isListeningEnabled() const;
    quint16 getOutgoingPort() const;
    bool isSpecificOutgoingPortSelected() const;
    bool isUdpDiscoveryEnabled() const; // 新增声明

    void updateFields(const QString &userName,
                      const QString &uuid,
                      quint16 listenPort,
                      bool enableListening,
                      quint16 outgoingPort, 
                      bool useSpecificOutgoingPort,
                      bool enableUdpDiscovery); // 新增

signals:
    void settingsApplied(const QString &userName,
                         quint16 listenPort,
                         bool enableListening,
                         quint16 outgoingPort, 
                         bool useSpecificOutgoingPort,
                         bool enableUdpDiscovery); // 新增
    void retryListenNowRequested(); // 新增信号
    void manualUdpBroadcastRequested(); // 新增信号

private slots:
    void onSaveButtonClicked();
    void onOutgoingPortSettingsChanged();
    void onEnableListeningChanged(bool checked);
    void onRetryListenNowClicked(); // 新增槽函数
    void onUdpDiscoveryEnableChanged(bool checked); // 新增槽
    void onManualBroadcastClicked(); // 新增槽

private:
    void setupUI();

    QLineEdit *userNameEdit;
    QLineEdit *userUuidEdit;

    // 监听端口设置
    QCheckBox *enableListeningCheckBox;
    QSpinBox *listenPortSpinBox;
    QPushButton *retryListenButton; // 新增按钮

    // 传出端口设置
    QCheckBox *specifyOutgoingPortCheckBox;
    QSpinBox *outgoingPortSpinBox;

    QCheckBox *udpDiscoveryCheckBox; // 新增：UDP发现复选框
    QPushButton *manualBroadcastButton; // 新增手动广播按钮

    QPushButton *saveButton;
    QPushButton *cancelButton;

    // Store initial values
    QString initialUserName;
    QString initialUserUuid;
    quint16 initialListenPort;
    bool initialEnableListening;
    quint16 initialOutgoingPort;
    bool initialUseSpecificOutgoingPort;
    bool initialEnableUdpDiscovery; // 新增声明
};

#endif // SETTINGSDIALOG_H
