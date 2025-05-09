#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

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
                            bool currentEnableListening, // 新增：当前是否启用监听
                            quint16 currentOutgoingPort, bool useSpecificOutgoingPort,
                            QWidget *parent = nullptr);

    QString getUserName() const;
    quint16 getListenPort() const;
    bool isListeningEnabled() const; // 新增：获取是否启用监听
    quint16 getOutgoingPort() const;
    bool isSpecificOutgoingPortSelected() const;

    void updateFields(const QString &userName,
                      const QString &uuid,
                      quint16 listenPort,
                      bool enableListening, // 新增
                      quint16 outgoingPort, bool useSpecificOutgoing);

signals:
    void settingsApplied(const QString &userName,
                         quint16 listenPort,
                         bool enableListening, // 新增
                         quint16 outgoingPort, bool useSpecificOutgoingPort);
    void retryListenNowRequested(); // 新增信号

private slots:
    void onSaveButtonClicked();
    void onOutgoingPortSettingsChanged();
    void onEnableListeningChanged(bool checked); // 新增
    void onRetryListenNowClicked(); // 新增槽函数

private:
    void setupUI();

    QLineEdit *userNameEdit;
    QLineEdit *userUuidEdit;

    // 监听端口设置
    QCheckBox *enableListeningCheckBox; // 新增：启用监听的复选框
    QSpinBox *listenPortSpinBox;
    QPushButton *retryListenButton; // 新增按钮

    // 传出端口设置
    QCheckBox *specifyOutgoingPortCheckBox;
    QSpinBox *outgoingPortSpinBox;

    QPushButton *saveButton;
    QPushButton *cancelButton;

    // Store initial values
    QString initialUserName;
    QString initialUserUuid;
    quint16 initialListenPort;
    bool initialEnableListening; // 新增
    quint16 initialOutgoingPort;
    bool initialUseSpecificOutgoingPort;
};

#endif // SETTINGSDIALOG_H
