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
#include <QGroupBox> // For grouping within tabs

SettingsDialog::SettingsDialog(const QString &currentUserName,
                               const QString &currentUserUuid,
                               quint16 currentListenPort,
                               bool currentAutoListenEnabled,
                               quint16 currentOutgoingPort,
                               bool currentUseSpecificOutgoing,
                               bool currentUdpDiscoveryEnabled,
                               quint16 currentUdpDiscoveryPort, // Added
                               bool currentEnableContinuousUdpBroadcast, // Added
                               int currentUdpBroadcastInterval, // Added
                               QWidget *parent)
    : QDialog(parent),
      initialUserName(currentUserName),
      initialUserUuid(currentUserUuid),
      initialListenPort(currentListenPort),
      initialAutoListenEnabled(currentAutoListenEnabled),
      initialOutgoingPort(currentOutgoingPort),
      initialUseSpecificOutgoing(currentUseSpecificOutgoing),
      initialUdpDiscoveryEnabled(currentUdpDiscoveryEnabled),
      initialUdpDiscoveryPort(currentUdpDiscoveryPort), // Added
      initialContinuousUdpBroadcastEnabled(currentEnableContinuousUdpBroadcast), // 修正变量名
      initialUdpBroadcastInterval(currentUdpBroadcastInterval) // Added
{
    setupUI();
    setWindowTitle(tr("Application Settings"));
    setMinimumWidth(450);
    setMinimumHeight(350);

    userNameEdit->setText(currentUserName);
    userUuidEdit->setText(currentUserUuid);

    enableListeningCheckBox->setChecked(currentAutoListenEnabled);
    listenPortSpinBox->setEnabled(currentAutoListenEnabled);
    retryListenButton->setEnabled(currentAutoListenEnabled);
    if (currentListenPort > 0) {
        listenPortSpinBox->setValue(currentListenPort);
    } else {
        listenPortSpinBox->setValue(60248);
    }

    specifyOutgoingPortCheckBox->setChecked(currentUseSpecificOutgoing);
    if (currentUseSpecificOutgoing && currentOutgoingPort > 0) {
        outgoingPortSpinBox->setValue(currentOutgoingPort);
    } else {
        outgoingPortSpinBox->setValue(0);
    }
    onOutgoingPortSettingsChanged();

    udpDiscoveryCheckBox->setChecked(currentUdpDiscoveryEnabled);
    udpDiscoveryPortSpinBox->setValue(currentUdpDiscoveryPort); // Added
    manualBroadcastButton->setEnabled(currentUdpDiscoveryEnabled);

    enableContinuousUdpBroadcastCheckBox->setChecked(currentEnableContinuousUdpBroadcast); // Added
    udpBroadcastIntervalSpinBox->setValue(currentUdpBroadcastInterval > 0 ? currentUdpBroadcastInterval : 5); // Added
    onUdpContinuousBroadcastChanged(currentEnableContinuousUdpBroadcast); // Added

    // Connect signals after UI setup and initial value setting
    connect(enableListeningCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onEnableListeningChanged);
    connect(retryListenButton, &QPushButton::clicked, this, &SettingsDialog::onRetryListenNowClicked);
    connect(specifyOutgoingPortCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onOutgoingPortSettingsChanged);
    connect(udpDiscoveryCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onUdpDiscoveryEnableChanged);
    connect(enableContinuousUdpBroadcastCheckBox, &QCheckBox::toggled, this, &SettingsDialog::onUdpContinuousBroadcastChanged); // Added
    connect(manualBroadcastButton, &QPushButton::clicked, this, &SettingsDialog::onManualBroadcastClicked);
}

SettingsDialog::~SettingsDialog()
{
    // Destructor implementation
    // Qt's parent-child mechanism will handle deletion of UI elements
    // if they were parented to 'this' or one of its child widgets.
    // No explicit deletion of tabWidget, userNameEdit, etc., is needed here
    // as long as they are children of SettingsDialog.
}

void SettingsDialog::updateFields(const QString &userName, const QString &uuid,
                                  quint16 listenPort, bool enableListening,
                                  quint16 outgoingPort, bool useSpecificOutgoing,
                                  bool enableUdpDiscovery, quint16 udpDiscoveryPort, // Added
                                  bool enableContinuousUdpBroadcast, int udpBroadcastInterval) // Added
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
    udpDiscoveryPortSpinBox->setValue(udpDiscoveryPort); // Added
    manualBroadcastButton->setEnabled(enableUdpDiscovery);

    enableContinuousUdpBroadcastCheckBox->setChecked(enableContinuousUdpBroadcast); // Added
    udpBroadcastIntervalSpinBox->setValue(udpBroadcastInterval > 0 ? udpBroadcastInterval : 5); // Added
    onUdpContinuousBroadcastChanged(enableContinuousUdpBroadcast); // Added

    // Update initial values as well, so "Cancel" reverts to these if dialog is re-shown without saving
    initialUserName = userName;
    initialUserUuid = uuid;
    initialListenPort = listenPort;
    initialAutoListenEnabled = enableListening;
    initialOutgoingPort = outgoingPort;
    initialUseSpecificOutgoing = useSpecificOutgoing;
    initialUdpDiscoveryEnabled = enableUdpDiscovery;
    initialUdpDiscoveryPort = udpDiscoveryPort; // Added
    initialContinuousUdpBroadcastEnabled = enableContinuousUdpBroadcast; // 修正变量名
    initialUdpBroadcastInterval = udpBroadcastInterval; // Added
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

    // --- Tab 2: TCP Settings ---
    QWidget *tcpTab = new QWidget(this);
    QVBoxLayout *tcpTabLayout = new QVBoxLayout(tcpTab); // Main layout for this tab

    // GroupBox for Listening Settings
    QGroupBox *listenGroup = new QGroupBox(tr("TCP Listening Settings"), this);
    QFormLayout *listenPortFormLayout = new QFormLayout(); // Form layout for listening settings

    enableListeningCheckBox = new QCheckBox(tr("Enable Network Listening"), this);

    listenPortSpinBox = new QSpinBox(this);
    listenPortSpinBox->setRange(1, 65535);

    retryListenButton = new QPushButton(tr("Attempt Listen Now"), this);

    listenPortFormLayout->addRow(enableListeningCheckBox);
    listenPortFormLayout->addRow(tr("Listen on Port:"), listenPortSpinBox);
    listenPortFormLayout->addRow(retryListenButton);
    listenGroup->setLayout(listenPortFormLayout);
    tcpTabLayout->addWidget(listenGroup);

    // GroupBox for Outgoing Connections
    QGroupBox *outgoingGroup = new QGroupBox(tr("TCP Outgoing Connections"), this);
    QVBoxLayout *outgoingPortSettingsLayout = new QVBoxLayout(); // VBox for outgoing settings
    specifyOutgoingPortCheckBox = new QCheckBox(tr("Specify source port for outgoing connections"), this);
    outgoingPortSpinBox = new QSpinBox(this);
    outgoingPortSpinBox->setRange(0, 65535);
    QLabel* outgoingPortInfoLabel = new QLabel(tr("Set to 0 for dynamic port allocation by OS."), this);
    outgoingPortInfoLabel->setWordWrap(true);
    outgoingPortInfoLabel->setStyleSheet("font-size: 9pt; color: grey;");

    QHBoxLayout *specificOutgoingPortLayout = new QHBoxLayout();
    specificOutgoingPortLayout->addWidget(new QLabel(tr("Port:")), 0);
    specificOutgoingPortLayout->addWidget(outgoingPortSpinBox, 1);
    
    outgoingPortSettingsLayout->addWidget(specifyOutgoingPortCheckBox);
    outgoingPortSettingsLayout->addLayout(specificOutgoingPortLayout);
    outgoingPortSettingsLayout->addWidget(outgoingPortInfoLabel);
    outgoingGroup->setLayout(outgoingPortSettingsLayout);
    tcpTabLayout->addWidget(outgoingGroup);
    tcpTabLayout->addStretch(); // Add stretch to push groups to top

    tabWidget->addTab(tcpTab, tr("TCP"));

    // --- Tab 3: UDP Discovery ---
    QWidget *udpDiscoveryTab = new QWidget(this);
    QFormLayout *udpDiscoveryFormLayout = new QFormLayout(udpDiscoveryTab); // Use QFormLayout for consistency
    udpDiscoveryCheckBox = new QCheckBox(tr("Enable UDP Auto Discovery & Connection"), this);
    
    udpDiscoveryPortSpinBox = new QSpinBox(this); // Added
    udpDiscoveryPortSpinBox->setRange(1, 65535);  // Added

    enableContinuousUdpBroadcastCheckBox = new QCheckBox(tr("Enable Continuous Broadcast"), this); // Added
    udpBroadcastIntervalSpinBox = new QSpinBox(this); // Added
    udpBroadcastIntervalSpinBox->setRange(1, 300); // Example range: 1 to 300 seconds
    udpBroadcastIntervalSpinBox->setSuffix(tr(" seconds")); // Added

    manualBroadcastButton = new QPushButton(tr("Broadcast Discovery Signal Now"), this);
    
    udpDiscoveryFormLayout->addRow(udpDiscoveryCheckBox);
    udpDiscoveryFormLayout->addRow(tr("Discovery Port:"), udpDiscoveryPortSpinBox); // Added
    udpDiscoveryFormLayout->addRow(enableContinuousUdpBroadcastCheckBox); // Added
    udpDiscoveryFormLayout->addRow(tr("Broadcast Interval:"), udpBroadcastIntervalSpinBox); // Added
    udpDiscoveryFormLayout->addRow(manualBroadcastButton);
    tabWidget->addTab(udpDiscoveryTab, tr("UDP"));

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
    udpDiscoveryPortSpinBox->setEnabled(checked);
    enableContinuousUdpBroadcastCheckBox->setEnabled(checked); // Added
    if (!checked) {
        enableContinuousUdpBroadcastCheckBox->setChecked(false);
    }
    onUdpContinuousBroadcastChanged(enableContinuousUdpBroadcastCheckBox->isChecked() && checked); // Added
}

void SettingsDialog::onUdpContinuousBroadcastChanged(bool checked) // Added
{
    udpBroadcastIntervalSpinBox->setEnabled(checked && udpDiscoveryCheckBox->isChecked());
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
    QString newUserName = userNameEdit->text().trimmed();
    quint16 newListenPort = static_cast<quint16>(listenPortSpinBox->value());
    bool newEnableListening = enableListeningCheckBox->isChecked();
    quint16 newOutgoingPort = static_cast<quint16>(outgoingPortSpinBox->value());
    bool newUseSpecificOutgoing = specifyOutgoingPortCheckBox->isChecked();
    bool newEnableUdpDiscovery = udpDiscoveryCheckBox->isChecked();
    quint16 newUdpDiscoveryPort = static_cast<quint16>(udpDiscoveryPortSpinBox->value()); // Added
    bool newEnableContinuousUdpBroadcast = enableContinuousUdpBroadcastCheckBox->isChecked(); // Added
    int newUdpBroadcastInterval = udpBroadcastIntervalSpinBox->value(); // Added

    if (newUserName.isEmpty()) {
        QMessageBox::warning(this, tr("Input Error"), tr("User name cannot be empty."));
        tabWidget->setCurrentIndex(0); // Switch to User Profile tab
        userNameEdit->setFocus();
        return;
    }

    if (newEnableListening && newListenPort == 0) {
        QMessageBox::warning(this, tr("Input Error"), tr("Listen port cannot be 0 when listening is enabled."));
        tabWidget->setCurrentIndex(1); // Switch to TCP tab
        listenPortSpinBox->setFocus();
        return;
    }

    if (newEnableUdpDiscovery && newUdpDiscoveryPort == 0) {
        QMessageBox::warning(this, tr("Input Error"), tr("Discovery port cannot be 0 when UDP discovery is enabled."));
        tabWidget->setCurrentIndex(2); // Switch to UDP tab
        udpDiscoveryPortSpinBox->setFocus();
        return;
    }

    // Update initial values to reflect the saved state
    initialUserName = newUserName;
    initialListenPort = newListenPort;
    initialAutoListenEnabled = newEnableListening;
    initialOutgoingPort = newOutgoingPort;
    initialUseSpecificOutgoing = newUseSpecificOutgoing;
    initialUdpDiscoveryEnabled = newEnableUdpDiscovery;
    initialUdpDiscoveryPort = newUdpDiscoveryPort; // Added
    initialContinuousUdpBroadcastEnabled = newEnableContinuousUdpBroadcast; // 修正变量名
    initialUdpBroadcastInterval = newUdpBroadcastInterval; // Added

    emit settingsApplied(newUserName, newListenPort, newEnableListening,
                         newOutgoingPort, newUseSpecificOutgoing,
                         newEnableUdpDiscovery, newUdpDiscoveryPort, // Added
                         newEnableContinuousUdpBroadcast, newUdpBroadcastInterval); // Added
    accept(); // Close the dialog
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

quint16 SettingsDialog::getUdpDiscoveryPort() const // Added
{
    return static_cast<quint16>(udpDiscoveryPortSpinBox->value());
}

bool SettingsDialog::isContinuousUdpBroadcastEnabled() const // Added
{
    return enableContinuousUdpBroadcastCheckBox->isChecked();
}

int SettingsDialog::getUdpBroadcastInterval() const // Added
{
    return udpBroadcastIntervalSpinBox->value();
}
