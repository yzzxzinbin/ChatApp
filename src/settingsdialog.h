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
                            quint16 currentListenPort,
                            quint16 currentOutgoingPort, bool useSpecificOutgoingPort,
                            QWidget *parent = nullptr);

    QString getUserName() const;
    quint16 getListenPort() const;
    quint16 getOutgoingPort() const;
    bool isSpecificOutgoingPortSelected() const;

signals:
    void settingsApplied(const QString &userName,
                         quint16 listenPort,
                         quint16 outgoingPort, bool useSpecificOutgoingPort);

private slots:
    void onSaveButtonClicked();
    void onOutgoingPortSettingsChanged();

private:
    void setupUI();

    QLineEdit *userNameEdit;

    // 监听端口设置
    QSpinBox *listenPortSpinBox;

    // 传出端口设置
    QCheckBox *specifyOutgoingPortCheckBox;
    QSpinBox *outgoingPortSpinBox;

    QPushButton *saveButton;
    QPushButton *cancelButton;

    // Store initial values
    QString initialUserName;
    quint16 initialListenPort;
    quint16 initialOutgoingPort;
    bool initialUseSpecificOutgoingPort;
};

#endif // SETTINGSDIALOG_H
