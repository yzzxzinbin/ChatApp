#include "addcontactdialog.h"
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QIntValidator>

AddContactDialog::AddContactDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle(tr("Add Network Contact"));
    setMinimumWidth(350);
}

void AddContactDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    nameLineEdit = new QLineEdit(this);
    formLayout->addRow(tr("Display Name:"), nameLineEdit);

    connectionTypeComboBox = new QComboBox(this);
    connectionTypeComboBox->addItem(tr("IPv4"), "IPv4");
    connectionTypeComboBox->addItem(tr("IPv6"), "IPv6");
    formLayout->addRow(tr("Connection Type:"), connectionTypeComboBox);

    ipAddressLineEdit = new QLineEdit(this);
    ipAddressLineEdit->setPlaceholderText(tr("Enter IP Address"));
    formLayout->addRow(tr("IP Address:"), ipAddressLineEdit);

    portLineEdit = new QLineEdit(this);
    portLineEdit->setValidator(new QIntValidator(1, 65535, this));
    portLineEdit->setText("60248"); // Default port
    formLayout->addRow(tr("Port:"), portLineEdit);

    mainLayout->addLayout(formLayout);

    statusLabel = new QLabel(tr("Please fill in the details."), this);
    statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0); // Indeterminate progress
    progressBar->setVisible(false);
    mainLayout->addWidget(progressBar);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    connectButton = new QPushButton(tr("Connect"), this);
    closeButton = new QPushButton(tr("Close"), this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(connectButton, &QPushButton::clicked, this, &AddContactDialog::onConnectButtonClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject); // Or accept if you want to distinguish
}

void AddContactDialog::onConnectButtonClicked()
{
    QString name = nameLineEdit->text().trimmed();
    QString type = connectionTypeComboBox->currentData().toString();
    QString ip = ipAddressLineEdit->text().trimmed();
    bool portOk;
    quint16 port = portLineEdit->text().toUShort(&portOk);

    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Input Error"), tr("Display name cannot be empty."));
        nameLineEdit->setFocus();
        return;
    }
    if (ip.isEmpty()) {
        QMessageBox::warning(this, tr("Input Error"), tr("IP address cannot be empty."));
        ipAddressLineEdit->setFocus();
        return;
    }
    if (!portOk || port == 0) {
        QMessageBox::warning(this, tr("Input Error"), tr("Invalid port number."));
        portLineEdit->setFocus();
        return;
    }

    setStatus(tr("Attempting to connect..."), false, true);
    connectButton->setEnabled(false);
    emit connectRequested(name, type, ip, port);
}

void AddContactDialog::setStatus(const QString &status, bool success, bool connecting)
{
    statusLabel->setText(status);
    progressBar->setVisible(connecting);
    if (!connecting) {
        connectButton->setEnabled(true);
    }
    if (success) {
        // Optionally close dialog on success or change connect button to "Done"
        // For now, just update status
        statusLabel->setStyleSheet("color: green;");
    } else if (!connecting && !status.contains("Attempting", Qt::CaseInsensitive) && !status.contains("Please fill", Qt::CaseInsensitive)) {
        statusLabel->setStyleSheet("color: red;");
    } else {
        statusLabel->setStyleSheet(""); // Reset to default
    }
}

