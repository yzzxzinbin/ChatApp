#ifndef ADDCONTACTDIALOG_H
#define ADDCONTACTDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QVBoxLayout;
class QHBoxLayout;
class QFormLayout;
QT_END_NAMESPACE

class AddContactDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddContactDialog(QWidget *parent = nullptr);

    void setStatus(const QString &status, bool success = false, bool connecting = false);

signals:
    void connectRequested(const QString &name, const QString &connectionType, const QString &ipAddress, quint16 port);

private slots:
    void onConnectButtonClicked();

private:
    void setupUI();

    QComboBox *connectionTypeComboBox;
    QLineEdit *nameLineEdit;
    QLineEdit *ipAddressLineEdit;
    QLineEdit *portLineEdit;

    QPushButton *connectButton;
    QPushButton *closeButton;

    QProgressBar *progressBar;
    QLabel *statusLabel;
};

#endif // ADDCONTACTDIALOG_H
