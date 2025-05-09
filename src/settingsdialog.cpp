#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QCheckBox>

SettingsDialog::SettingsDialog(const QString &currentUserName,
                               const QString &currentUserUuid,
                               quint16 currentListenPort,
                               quint16 currentOutgoingPort, bool useSpecificOutgoingPort,
                               QWidget *parent)
    : QDialog(parent),
      initialUserName(currentUserName),
      initialUserUuid(currentUserUuid),
      initialListenPort(currentListenPort),
      initialOutgoingPort(currentOutgoingPort), initialUseSpecificOutgoingPort(useSpecificOutgoingPort)
{
    setupUI();
    setWindowTitle(tr("Application Settings"));
    setMinimumWidth(400);

    userNameEdit->setText(currentUserName);
    userUuidEdit->setText(currentUserUuid);

    // 初始化监听端口设置
    if (currentListenPort > 0) {
        listenPortSpinBox->setValue(currentListenPort);
    } else {
        listenPortSpinBox->setValue(60248);
    }

    // 初始化传出端口设置
    specifyOutgoingPortCheckBox->setChecked(useSpecificOutgoingPort);
    if (useSpecificOutgoingPort && currentOutgoingPort > 0) {
        outgoingPortSpinBox->setValue(currentOutgoingPort);
    } else {
        outgoingPortSpinBox->setValue(0);
    }
    onOutgoingPortSettingsChanged();
}

void SettingsDialog::updateFields(const QString &userName, const QString &uuid, quint16 listenPort, quint16 outgoingPort, bool useSpecificOutgoing)
{
    userNameEdit->setText(userName);
    userUuidEdit->setText(uuid); // UUID is read-only, but good to refresh if it could ever change (though unlikely)

    if (listenPort > 0) {
        listenPortSpinBox->setValue(listenPort);
    } else {
        listenPortSpinBox->setValue(60248); // Default fallback
    }

    specifyOutgoingPortCheckBox->setChecked(useSpecificOutgoing);
    if (useSpecificOutgoing && outgoingPort > 0) {
        outgoingPortSpinBox->setValue(outgoingPort);
    } else {
        outgoingPortSpinBox->setValue(0); // Default for dynamic or if not specified
    }
    onOutgoingPortSettingsChanged(); // Ensure dependent UI state is correct
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // User Profile Section
    QGroupBox *userProfileGroup = new QGroupBox(tr("User Profile"), this);
    QFormLayout *userProfileLayout = new QFormLayout();
    userNameEdit = new QLineEdit(this);
    userNameEdit->setPlaceholderText(tr("Enter your display name"));
    userProfileLayout->addRow(tr("Display Name:"), userNameEdit);

    userUuidEdit = new QLineEdit(this);
    userUuidEdit->setReadOnly(true);
    userUuidEdit->setToolTip(tr("Your unique identifier. This cannot be changed."));
    userProfileLayout->addRow(tr("User UUID:"), userUuidEdit);

    userProfileGroup->setLayout(userProfileLayout);
    mainLayout->addWidget(userProfileGroup);

    // Network Listening Port Section
    QGroupBox *listenPortGroup = new QGroupBox(tr("Network Listening Port (for incoming connections)"), this);
    QFormLayout *listenPortFormLayout = new QFormLayout();

    listenPortSpinBox = new QSpinBox(this);
    listenPortSpinBox->setRange(1, 65535);
    listenPortSpinBox->setValue(60248);

    listenPortFormLayout->addRow(tr("Listen on Port:"), listenPortSpinBox);
    listenPortGroup->setLayout(listenPortFormLayout);
    mainLayout->addWidget(listenPortGroup);

    // Outgoing Connection Port Section
    QGroupBox *outgoingPortGroup = new QGroupBox(tr("Network Outgoing Port (source port for new connections)"), this);
    QVBoxLayout *outgoingPortSettingsLayout = new QVBoxLayout();
    specifyOutgoingPortCheckBox = new QCheckBox(tr("Specify source port for outgoing connections"), this);
    outgoingPortSpinBox = new QSpinBox(this);
    outgoingPortSpinBox->setRange(0, 65535);
    outgoingPortSpinBox->setValue(0);
    outgoingPortSpinBox->setEnabled(false);
    QLabel* outgoingPortInfoLabel = new QLabel(tr("Set to 0 for dynamic port allocation by OS."), this);
    outgoingPortInfoLabel->setWordWrap(true);
    outgoingPortInfoLabel->setStyleSheet("font-size: 9pt; color: grey;");

    QHBoxLayout *specificOutgoingPortLayout = new QHBoxLayout();
    specificOutgoingPortLayout->addWidget(new QLabel(tr("Port:")), 0);
    specificOutgoingPortLayout->addWidget(outgoingPortSpinBox, 1);
    
    outgoingPortSettingsLayout->addWidget(specifyOutgoingPortCheckBox);
    outgoingPortSettingsLayout->addLayout(specificOutgoingPortLayout);
    outgoingPortSettingsLayout->addWidget(outgoingPortInfoLabel);
    outgoingPortGroup->setLayout(outgoingPortSettingsLayout);
    mainLayout->addWidget(outgoingPortGroup);

    connect(specifyOutgoingPortCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onOutgoingPortSettingsChanged);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    saveButton = new QPushButton(tr("Save"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSaveButtonClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    setLayout(mainLayout);
}

void SettingsDialog::onOutgoingPortSettingsChanged()
{
    outgoingPortSpinBox->setEnabled(specifyOutgoingPortCheckBox->isChecked());
}

void SettingsDialog::onSaveButtonClicked()
{
    QString userName = userNameEdit->text().trimmed();
    if (userName.isEmpty()) {
        QMessageBox::warning(this, tr("Input Error"), tr("Display name cannot be empty."));
        userNameEdit->setFocus();
        return;
    }

    quint16 listenPort = static_cast<quint16>(listenPortSpinBox->value());
    if (listenPort == 0) {
        QMessageBox::warning(this, tr("Input Error"), tr("Listen port cannot be 0."));
        listenPortSpinBox->setFocus();
        return;
    }

    bool useSpecificOutgoing = specifyOutgoingPortCheckBox->isChecked();
    quint16 outgoingPort = 0;
    if (useSpecificOutgoing) {
        outgoingPort = static_cast<quint16>(outgoingPortSpinBox->value());
    }

    emit settingsApplied(userName, listenPort, outgoingPort, useSpecificOutgoing);
    accept();
}

QString SettingsDialog::getUserName() const
{
    return userNameEdit->text().trimmed();
}

quint16 SettingsDialog::getListenPort() const
{
    return static_cast<quint16>(listenPortSpinBox->value());
}

quint16 SettingsDialog::getOutgoingPort() const
{
    if (specifyOutgoingPortCheckBox->isChecked()) {
        return static_cast<quint16>(outgoingPortSpinBox->value());
    }
    return 0;
}

bool SettingsDialog::isSpecificOutgoingPortSelected() const
{
    return specifyOutgoingPortCheckBox->isChecked();
}
