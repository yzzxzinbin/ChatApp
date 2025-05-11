#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QDebug>

SettingsDialog::SettingsDialog(const QString &currentUserName,
                               const QString &currentUserUuid,
                               quint16 currentListenPort,
                               bool currentAutoListenEnabled,
                               quint16 currentOutgoingPort,
                               bool currentUseSpecificOutgoing,
                               bool currentUdpDiscoveryEnabled,
                               quint16 currentUdpDiscoveryPort,
                               bool currentContinuousUdpBroadcastEnabled, // Added
                               int currentBroadcastIntervalSeconds,      // Added
                               QWidget *parent)
    : QDialog(parent),
      initialUserName(currentUserName),
      initialUserUuid(currentUserUuid),
      initialListenPort(currentListenPort),
      initialAutoListenEnabled(currentAutoListenEnabled),
      initialOutgoingPort(currentOutgoingPort),
      initialUseSpecificOutgoing(currentUseSpecificOutgoing),
      initialUdpDiscoveryEnabled(currentUdpDiscoveryEnabled),
      initialUdpDiscoveryPort(currentUdpDiscoveryPort),
      initialContinuousUdpBroadcastEnabled(currentContinuousUdpBroadcastEnabled), // Added
      initialBroadcastIntervalSeconds(currentBroadcastIntervalSeconds)          // Added
{
    setupUI(); // Setup UI first

    // Set initial values for the fields
    updateFields(initialUserName, initialUserUuid, initialListenPort, initialAutoListenEnabled,
                 initialOutgoingPort, initialUseSpecificOutgoing,
                 initialUdpDiscoveryEnabled, initialUdpDiscoveryPort,
                 initialContinuousUdpBroadcastEnabled, initialBroadcastIntervalSeconds); // Pass new params

    // Connect signals after UI setup and initial value setting
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSaveButtonClicked);
    connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::reject);

    connect(enableListeningCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onEnableListeningChanged);
    connect(retryListenButton, &QPushButton::clicked, this, &SettingsDialog::onRetryListenNowClicked);
    connect(specifyOutgoingPortCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onOutgoingPortSettingsChanged);

    connect(udpDiscoveryCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onUdpDiscoveryEnableChanged);
    connect(continuousUdpBroadcastCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onContinuousUdpBroadcastEnableChanged); // Added
    connect(manualBroadcastButton, &QPushButton::clicked, this, &SettingsDialog::onManualBroadcastClicked);
}

SettingsDialog::~SettingsDialog()
{
    // Destructor implementation
    // Qt's parent-child mechanism will handle deletion of UI elements
}

void SettingsDialog::setupUI() {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    tabWidget = new QTabWidget(this);

    // General Tab
    QWidget *generalTab = new QWidget();
    QFormLayout *generalLayout = new QFormLayout(generalTab);
    userNameEdit = new QLineEdit();
    userUuidEdit = new QLineEdit();
    userUuidEdit->setReadOnly(true);
    generalLayout->addRow(tr("User Name:"), userNameEdit);
    generalLayout->addRow(tr("User UUID:"), userUuidEdit);
    tabWidget->addTab(generalTab, tr("General"));

    // Network Tab
    QWidget *networkTab = new QWidget();
    QVBoxLayout *networkTabLayout = new QVBoxLayout(networkTab);

    // TCP Settings Group
    QGroupBox *tcpGroup = new QGroupBox(tr("TCP Connection Settings"));
    QFormLayout *tcpLayout = new QFormLayout();
    enableListeningCheckBox = new QCheckBox(tr("Enable Network Listening"));
    listenPortSpinBox = new QSpinBox();
    listenPortSpinBox->setRange(1024, 65535);
    retryListenButton = new QPushButton(tr("Retry Listen Now"));
    QHBoxLayout* listenPortLayout = new QHBoxLayout();
    listenPortLayout->addWidget(listenPortSpinBox);
    listenPortLayout->addWidget(retryListenButton);

    tcpLayout->addRow(enableListeningCheckBox);
    tcpLayout->addRow(tr("Listen on Port:"), listenPortLayout);

    specifyOutgoingPortCheckBox = new QCheckBox(tr("Specify Outgoing Source Port (0 for dynamic)"));
    outgoingPortSpinBox = new QSpinBox();
    outgoingPortSpinBox->setRange(0, 65535); // 0 means dynamic
    tcpLayout->addRow(specifyOutgoingPortCheckBox);
    tcpLayout->addRow(tr("Outgoing Source Port:"), outgoingPortSpinBox);
    tcpGroup->setLayout(tcpLayout);
    networkTabLayout->addWidget(tcpGroup);

    // UDP Discovery Settings Group
    QGroupBox *udpGroup = new QGroupBox(tr("UDP Discovery Settings"));
    QFormLayout *udpLayout = new QFormLayout();
    udpDiscoveryCheckBox = new QCheckBox(tr("Enable UDP Discovery"));
    udpDiscoveryPortSpinBox = new QSpinBox();
    udpDiscoveryPortSpinBox->setRange(1024, 65535);

    continuousUdpBroadcastCheckBox = new QCheckBox(tr("Enable Continuous Broadcast")); // Added
    broadcastIntervalLabel = new QLabel(tr("Broadcast Interval (seconds):")); // Added
    broadcastIntervalSpinBox = new QSpinBox();      // Added
    broadcastIntervalSpinBox->setRange(1, 300);     // Example range: 1s to 5min
    broadcastIntervalSpinBox->setValue(5);          // Default interval

    manualBroadcastButton = new QPushButton(tr("Send Manual Broadcast Now"));

    udpLayout->addRow(udpDiscoveryCheckBox);
    udpLayout->addRow(tr("UDP Discovery Port:"), udpDiscoveryPortSpinBox);
    udpLayout->addRow(continuousUdpBroadcastCheckBox); // Added
    QHBoxLayout* intervalLayout = new QHBoxLayout(); // Added for label and spinbox
    intervalLayout->addWidget(broadcastIntervalLabel);
    intervalLayout->addWidget(broadcastIntervalSpinBox);
    udpLayout->addRow(intervalLayout); // Added
    udpLayout->addRow(manualBroadcastButton);
    udpGroup->setLayout(udpLayout);
    networkTabLayout->addWidget(udpGroup);

    tabWidget->addTab(networkTab, tr("Network"));
    mainLayout->addWidget(tabWidget);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox();
    saveButton = buttonBox->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    cancelButton = buttonBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    mainLayout->addWidget(buttonBox);

    // Initial UI states based on checkboxes
    onEnableListeningChanged(enableListeningCheckBox->isChecked());
    onOutgoingPortSettingsChanged();
    onUdpDiscoveryEnableChanged(udpDiscoveryCheckBox->isChecked());
    onContinuousUdpBroadcastEnableChanged(continuousUdpBroadcastCheckBox->isChecked()); // Added
}

void SettingsDialog::updateFields(const QString &userName, const QString &uuid,
                                  quint16 listenPort, bool enableListening,
                                  quint16 outgoingPort, bool useSpecificOutgoing,
                                  bool enableUdpDiscovery, quint16 udpDiscoveryPort,
                                  bool continuousBroadcast, int intervalSeconds) // Added
{
    userNameEdit->setText(userName);
    userUuidEdit->setText(uuid);

    enableListeningCheckBox->setChecked(enableListening);
    listenPortSpinBox->setValue(listenPort > 0 ? listenPort : 60248);
    onEnableListeningChanged(enableListening);

    specifyOutgoingPortCheckBox->setChecked(useSpecificOutgoing);
    outgoingPortSpinBox->setValue(useSpecificOutgoing && outgoingPort > 0 ? outgoingPort : 0);
    onOutgoingPortSettingsChanged();

    udpDiscoveryCheckBox->setChecked(enableUdpDiscovery);
    udpDiscoveryPortSpinBox->setValue(udpDiscoveryPort > 0 ? udpDiscoveryPort : 60249);
    continuousUdpBroadcastCheckBox->setChecked(continuousBroadcast); // Added
    broadcastIntervalSpinBox->setValue(qBound(1, intervalSeconds, 300)); // Added, ensure value is within spinbox range
    onUdpDiscoveryEnableChanged(enableUdpDiscovery); // This will call onContinuousUdpBroadcastEnableChanged if needed
    onContinuousUdpBroadcastEnableChanged(enableUdpDiscovery && continuousBroadcast); // Explicitly set state based on both
}

void SettingsDialog::onSaveButtonClicked()
{
    // Update initial values to current field values before emitting
    initialUserName = userNameEdit->text();
    // UUID is not changed by user
    initialListenPort = listenPortSpinBox->value();
    initialAutoListenEnabled = enableListeningCheckBox->isChecked();
    initialUseSpecificOutgoing = specifyOutgoingPortCheckBox->isChecked();
    initialOutgoingPort = initialUseSpecificOutgoing ? outgoingPortSpinBox->value() : 0;
    initialUdpDiscoveryEnabled = udpDiscoveryCheckBox->isChecked();
    initialUdpDiscoveryPort = udpDiscoveryPortSpinBox->value();
    initialContinuousUdpBroadcastEnabled = continuousUdpBroadcastCheckBox->isChecked(); // Added
    initialBroadcastIntervalSeconds = broadcastIntervalSpinBox->value();              // Added

    emit settingsApplied(initialUserName,
                         initialListenPort,
                         initialAutoListenEnabled,
                         initialOutgoingPort, initialUseSpecificOutgoing,
                         initialUdpDiscoveryEnabled, initialUdpDiscoveryPort,
                         initialContinuousUdpBroadcastEnabled, initialBroadcastIntervalSeconds); // Added
    accept();
}

void SettingsDialog::onEnableListeningChanged(bool checked)
{
    listenPortSpinBox->setEnabled(checked);
    retryListenButton->setEnabled(checked);
}

void SettingsDialog::onRetryListenNowClicked()
{
    emit retryListenNowRequested();
}

void SettingsDialog::onUdpDiscoveryEnableChanged(bool checked)
{
    udpDiscoveryPortSpinBox->setEnabled(checked);
    manualBroadcastButton->setEnabled(checked);
    continuousUdpBroadcastCheckBox->setEnabled(checked); // Added: continuous broadcast depends on general UDP discovery
    if (!checked) { // If UDP discovery is disabled, continuous broadcast and its interval also get disabled
        continuousUdpBroadcastCheckBox->setChecked(false); // Uncheck it
    }
    // Update interval spinbox based on continuous checkbox state (which might have changed)
    onContinuousUdpBroadcastEnableChanged(continuousUdpBroadcastCheckBox->isChecked() && checked);
}

void SettingsDialog::onContinuousUdpBroadcastEnableChanged(bool checked) // Added
{
    // Interval spinbox is only enabled if BOTH general UDP discovery AND continuous broadcast are checked
    bool generalUdpEnabled = udpDiscoveryCheckBox->isChecked();
    broadcastIntervalSpinBox->setEnabled(generalUdpEnabled && checked);
    broadcastIntervalLabel->setEnabled(generalUdpEnabled && checked);
}

void SettingsDialog::onManualBroadcastClicked()
{
    emit manualUdpBroadcastRequested();
}

void SettingsDialog::onOutgoingPortSettingsChanged()
{
    outgoingPortSpinBox->setEnabled(specifyOutgoingPortCheckBox->isChecked());
}
