#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QTabWidget>

SettingsDialog::SettingsDialog(const QString &currentUserName,
                               const QString &currentUserUuid,
                               quint16 currentListenPort,
                               bool currentEnableListening,
                               quint16 currentOutgoingPort, 
                               bool useSpecificOutgoingPort,
                               bool currentEnableUdpDiscovery,
                               QWidget *parent)
    : QDialog(parent),
      initialUserName(currentUserName),
      initialUserUuid(currentUserUuid),
      initialListenPort(currentListenPort),
      initialEnableListening(currentEnableListening),
      initialOutgoingPort(currentOutgoingPort), 
      initialUseSpecificOutgoingPort(useSpecificOutgoingPort),
      initialEnableUdpDiscovery(currentEnableUdpDiscovery)
{
    setupUI();
    setWindowTitle(tr("Application Settings"));
    setMinimumWidth(450);
    setMinimumHeight(350);

    userNameEdit->setText(currentUserName);
    userUuidEdit->setText(currentUserUuid);

    enableListeningCheckBox->setChecked(currentEnableListening);
    listenPortSpinBox->setEnabled(currentEnableListening);
    retryListenButton->setEnabled(currentEnableListening);
    if (currentListenPort > 0) {
        listenPortSpinBox->setValue(currentListenPort);
    } else {
        listenPortSpinBox->setValue(60248);
    }

    specifyOutgoingPortCheckBox->setChecked(useSpecificOutgoingPort);
    if (useSpecificOutgoingPort && currentOutgoingPort > 0) {
        outgoingPortSpinBox->setValue(currentOutgoingPort);
    } else {
        outgoingPortSpinBox->setValue(0);
    }
    onOutgoingPortSettingsChanged();

    udpDiscoveryCheckBox->setChecked(currentEnableUdpDiscovery);
    manualBroadcastButton->setEnabled(currentEnableUdpDiscovery);
}

void SettingsDialog::updateFields(const QString &userName, const QString &uuid, quint16 listenPort, bool enableListening, quint16 outgoingPort, bool useSpecificOutgoing, bool enableUdpDiscovery)
{
    userNameEdit->setText(userName);
    userUuidEdit->setText(uuid);

    enableListeningCheckBox->setChecked(enableListening);
    listenPortSpinBox->setEnabled(enableListening);
    retryListenButton->setEnabled(enableListening);
    if (listenPort > 0) {
        listenPortSpinBox->setValue(listenPort);
    } else {
        listenPortSpinBox->setValue(60248);
    }

    specifyOutgoingPortCheckBox->setChecked(useSpecificOutgoing);
    if (useSpecificOutgoing && outgoingPort > 0) {
        outgoingPortSpinBox->setValue(outgoingPort);
    } else {
        outgoingPortSpinBox->setValue(0);
    }
    onOutgoingPortSettingsChanged();

    udpDiscoveryCheckBox->setChecked(enableUdpDiscovery);
    manualBroadcastButton->setEnabled(enableUdpDiscovery);
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    tabWidget = new QTabWidget(this);

    // --- Tab 1: User Profile ---
    QWidget *userProfileTab = new QWidget(this);
    QFormLayout *userProfileLayout = new QFormLayout(userProfileTab);
    userNameEdit = new QLineEdit(this);
    userNameEdit->setPlaceholderText(tr("Enter your display name"));
    userProfileLayout->addRow(tr("Display Name:"), userNameEdit);

    userUuidEdit = new QLineEdit(this);
    userUuidEdit->setReadOnly(true);
    userUuidEdit->setToolTip(tr("Your unique identifier. This cannot be changed."));
    userProfileLayout->addRow(tr("User UUID:"), userUuidEdit);
    tabWidget->addTab(userProfileTab, tr("User Profile"));

    // --- Tab 2: Listening Settings ---
    QWidget *listeningTab = new QWidget(this);
    QFormLayout *listenPortFormLayout = new QFormLayout(listeningTab);

    enableListeningCheckBox = new QCheckBox(tr("Enable Network Listening"), this);
    connect(enableListeningCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onEnableListeningChanged);

    listenPortSpinBox = new QSpinBox(this);
    listenPortSpinBox->setRange(1, 65535);
    listenPortSpinBox->setValue(60248);

    retryListenButton = new QPushButton(tr("Attempt Listen Now"), this);
    connect(retryListenButton, &QPushButton::clicked, this, &SettingsDialog::onRetryListenNowClicked);

    listenPortFormLayout->addRow(enableListeningCheckBox);
    listenPortFormLayout->addRow(tr("Listen on Port:"), listenPortSpinBox);
    listenPortFormLayout->addRow(retryListenButton);
    tabWidget->addTab(listeningTab, tr("Listening"));

    // --- Tab 3: Outgoing Connections ---
    QWidget *outgoingTab = new QWidget(this);
    QVBoxLayout *outgoingPortSettingsLayout = new QVBoxLayout(outgoingTab);
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
    outgoingPortSettingsLayout->addStretch();
    tabWidget->addTab(outgoingTab, tr("Outgoing"));

    connect(specifyOutgoingPortCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onOutgoingPortSettingsChanged);

    // --- Tab 4: Discovery (UDP) ---
    QWidget *discoveryTab = new QWidget(this);
    QVBoxLayout *udpDiscoveryLayout = new QVBoxLayout(discoveryTab);
    udpDiscoveryCheckBox = new QCheckBox(tr("Enable UDP Auto Discovery & Connection"), this);
    manualBroadcastButton = new QPushButton(tr("Broadcast Discovery Signal Now"), this);
    
    udpDiscoveryLayout->addWidget(udpDiscoveryCheckBox);
    udpDiscoveryLayout->addWidget(manualBroadcastButton);
    udpDiscoveryLayout->addStretch();
    tabWidget->addTab(discoveryTab, tr("Discovery"));
    
    connect(udpDiscoveryCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onUdpDiscoveryEnableChanged);
    connect(manualBroadcastButton, &QPushButton::clicked, this, &SettingsDialog::onManualBroadcastClicked);

    mainLayout->addWidget(tabWidget);

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

void SettingsDialog::onEnableListeningChanged(bool checked)
{
    listenPortSpinBox->setEnabled(checked);
    retryListenButton->setEnabled(checked);
}

void SettingsDialog::onUdpDiscoveryEnableChanged(bool checked)
{
    manualBroadcastButton->setEnabled(checked);
}

void SettingsDialog::onManualBroadcastClicked()
{
    emit manualUdpBroadcastRequested();
}

void SettingsDialog::onRetryListenNowClicked()
{
    emit retryListenNowRequested();
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

    bool enableListening = enableListeningCheckBox->isChecked();
    quint16 listenPort = 0;
    if (enableListening) {
        listenPort = static_cast<quint16>(listenPortSpinBox->value());
        if (listenPort == 0) {
            QMessageBox::warning(this, tr("Input Error"), tr("Listen port cannot be 0 when listening is enabled."));
            listenPortSpinBox->setFocus();
            return;
        }
    }

    bool useSpecificOutgoing = specifyOutgoingPortCheckBox->isChecked();
    quint16 outgoingPort = 0;
    if (useSpecificOutgoing) {
        outgoingPort = static_cast<quint16>(outgoingPortSpinBox->value());
    }

    bool enableUdpDiscovery = udpDiscoveryCheckBox->isChecked();

    emit settingsApplied(userName, listenPort, enableListening, outgoingPort, useSpecificOutgoing, enableUdpDiscovery);
    accept();
}

QString SettingsDialog::getUserName() const
{
    return userNameEdit->text().trimmed();
}

quint16 SettingsDialog::getListenPort() const
{
    if (enableListeningCheckBox->isChecked()) {
        return static_cast<quint16>(listenPortSpinBox->value());
    }
    return 0;
}

bool SettingsDialog::isListeningEnabled() const
{
    return enableListeningCheckBox->isChecked();
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

bool SettingsDialog::isUdpDiscoveryEnabled() const
{
    return udpDiscoveryCheckBox->isChecked();
}
