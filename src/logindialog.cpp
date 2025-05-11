#include "logindialog.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QMouseEvent>
#include <QApplication> // Required for qApp
#include <QGraphicsDropShadowEffect>
#include <QPainter> // Required for custom painting if needed

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), m_dragging(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi();
    applyStyles();

    // The shadow has a blurRadius of 25 and a yOffset of 5.
    // We need to make the dialog larger than the containerWidget to accommodate the shadow.
    // Let's add a margin of approx. 20px around the containerWidget for the shadow.
    // Original containerWidget effective size is 400x490.
    // New Dialog width = 400 (container) + 2 * 20 (margins) = 440
    // New Dialog height = 490 (container) + 2 * 20 (margins) = 530
    setFixedSize(440, 530); // Adjusted size to include shadow margin
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    // Set margins for the mainLayout. This creates a transparent border around containerWidget
    // where the shadow can be rendered.
    // A margin of 20px should be mostly sufficient for a blurRadius of 25px.
    int shadowMargin = 20;
    mainLayout->setContentsMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
    mainLayout->setSpacing(0);

    QWidget *containerWidget = new QWidget(this);
    containerWidget->setObjectName("containerWidget");
    // containerWidget will now effectively be 400x490 due to the margins in mainLayout.

    QVBoxLayout *containerLayout = new QVBoxLayout(containerWidget);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // Title bar area for minimize/close buttons
    titleBarWidget = new QWidget(containerWidget); // Initialize member variable
    titleBarWidget->setObjectName("titleBarWidget");
    titleBarWidget->setFixedHeight(30); // Height for the button bar
    QHBoxLayout *titleBarLayout = new QHBoxLayout(titleBarWidget);
    titleBarLayout->setContentsMargins(10, 0, 10, 0); // Margins for buttons
    titleBarLayout->setSpacing(5);

    titleBarLayout->addStretch(); // Push buttons to the right

    minimizeButton = new QPushButton("—", titleBarWidget); // Using text for now
    minimizeButton->setObjectName("minimizeButton");
    closeButton = new QPushButton("✕", titleBarWidget); // Using text for now
    closeButton->setObjectName("closeButton");

    titleBarLayout->addWidget(minimizeButton);
    titleBarLayout->addWidget(closeButton);

    containerLayout->addWidget(titleBarWidget);

    // Top Image Part
    imageLabel = new QLabel(containerWidget);
    imageLabel->setObjectName("imageLabel");
    // Assuming dialog width is 400, image height for 2:1 ratio would be 200
    int imageDisplayHeight = 200;
    QPixmap starterPixmap(":/res/starter.png");
    if (starterPixmap.isNull())
    {
        imageLabel->setText("Image not found (400x200)");
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("background-color: #555; color: white;");
    }
    else
    {
        // Scale pixmap to fit width 400, height 200, keeping aspect ratio.
        // containerWidget's width will be 400 (Dialog width 440 - 2*20 margin).
        imageLabel->setPixmap(starterPixmap.scaled(400, imageDisplayHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    imageLabel->setFixedHeight(imageDisplayHeight);
    imageLabel->setAlignment(Qt::AlignCenter); // Center the (possibly letterboxed) pixmap
    containerLayout->addWidget(imageLabel);

    // Bottom Form Part
    formContainer = new QWidget(containerWidget);
    formContainer->setObjectName("formContainer");
    QVBoxLayout *formLayout = new QVBoxLayout(formContainer);
    formLayout->setContentsMargins(30, 25, 30, 30); // Adjusted padding
    formLayout->setSpacing(18);                     // Adjusted spacing

    usernameEdit = new QLineEdit(formContainer);
    usernameEdit->setObjectName("usernameEdit");
    usernameEdit->setPlaceholderText(tr("Username"));

    passwordEdit = new QLineEdit(formContainer);
    passwordEdit->setObjectName("passwordEdit");
    passwordEdit->setPlaceholderText(tr("Password"));
    passwordEdit->setEchoMode(QLineEdit::Password);

    loginButton = new QPushButton(tr("Login"), formContainer);
    loginButton->setObjectName("loginButton");
    loginButton->setFixedHeight(45);

    formLayout->addWidget(usernameEdit);
    formLayout->addWidget(passwordEdit);
    formLayout->addWidget(loginButton);
    formLayout->addStretch();

    containerLayout->addWidget(formContainer, 1);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this); // 'this' or 'containerWidget' as parent for effect
    shadow->setBlurRadius(25);
    shadow->setXOffset(0);
    shadow->setYOffset(5); // Slight downward offset
    shadow->setColor(QColor(0, 0, 0, 100)); // Semi-transparent black
    containerWidget->setGraphicsEffect(shadow);

    mainLayout->addWidget(containerWidget);
    setLayout(mainLayout);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(minimizeButton, &QPushButton::clicked, this, &LoginDialog::onMinimizeClicked);
    connect(closeButton, &QPushButton::clicked, this, &LoginDialog::onCloseClicked);
}

void LoginDialog::applyStyles()
{
    this->setStyleSheet(R"(
        #containerWidget {
            background-color: #ffffff; 
            border-radius: 15px; /* This provides the overall rounded shape */
        }

        #titleBarWidget {
            background-color: transparent; /* Title bar itself is transparent */
        }

        QPushButton#minimizeButton, QPushButton#closeButton {
            background-color: transparent;
            border: none;
            color: #555555; /* Adjust color as needed */
            font-family: "Arial", sans-serif; /* Specify a font that has the symbols */
            font-size: 16px; /* Adjust size for symbols */
            font-weight: bold;
            min-width: 25px;
            max-width: 25px;
            min-height: 25px;
            max-height: 25px;
            border-radius: 4px; /* Optional: slight rounding for buttons themselves */
        }
        QPushButton#minimizeButton:hover, QPushButton#closeButton:hover {
            background-color: #e0e0e0; /* Light grey hover */
            color: #000000;
        }
        QPushButton#closeButton:hover {
            background-color: #ff6b6b; /* Reddish hover for close */
            color: white;
        }
        QPushButton#minimizeButton:pressed, QPushButton#closeButton:pressed {
            background-color: #c0c0c0; /* Darker grey pressed */
        }
         QPushButton#closeButton:pressed {
            background-color: #ee3535; /* Darker Reddish pressed for close */
            color: white;
        }


        #imageLabel {
            background-color: #f0f0f0; /* Fallback/area bg for image */
            border-top-left-radius: 15px; /* Match containerWidget's top radius */
            border-top-right-radius: 15px;
            /* No bottom radius needed here as formContainer is below it */
        }

        #formContainer {
            background-color: #ffffff; 
            border-bottom-left-radius: 15px;  /* Match containerWidget's bottom radius */
            border-bottom-right-radius: 15px; 
            /* No top radius needed here as imageLabel is above it */
        }

        QLineEdit {
            background-color: #f0f0f2; /* Light grey background */
            border: 1px solid #dcdde1; /* Light border */
            border-radius: 8px;
            padding: 12px 15px;
            color: #2f3542; /* Dark grey text */
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #0078d4; /* Blue border on focus - similar to other parts of app */
            background-color: #ffffff;
        }
        QLineEdit::placeholder { /* Note: This might not work on all styles/platforms directly in QSS */
            color: #a4b0be;
        }


        QPushButton#loginButton {
            background-color: #0078d4; /* Primary blue */
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton#loginButton:hover {
            background-color: #005a9e; /* Darker blue */
        }
        QPushButton#loginButton:pressed {
            background-color: #004578; /* Even darker blue */
        }
    )");

    // For placeholder text, QPalette is more reliable if QSS doesn't work
    QPalette palette = usernameEdit->palette();
    palette.setColor(QPalette::PlaceholderText, QColor("#a4b0be"));
    usernameEdit->setPalette(palette);
    passwordEdit->setPalette(palette);
}

void LoginDialog::onLoginClicked()
{
    // Login logic will go here
    // For now, just accept the dialog to simulate a successful login
    // In a real app, you'd validate credentials
    // QString username = usernameEdit->text();
    // QString password = passwordEdit->text();
    // if (username == "admin" && password == "password") { // Dummy check
    //     accept();
    // } else {
    //     // Show error message
    //     QMessageBox::warning(this, "Login Failed", "Invalid username or password.");
    // }
    accept(); // Temporarily accept to proceed
}

void LoginDialog::onMinimizeClicked()
{
    this->showMinimized();
}

void LoginDialog::onCloseClicked()
{
    this->reject(); // Or close(), reject() is typical for dialogs to indicate cancellation
}

void LoginDialog::mousePressEvent(QMouseEvent *event)
{
    // Allow dragging only if the click is not on the input fields or buttons
    // A common way is to check if the click is on a specific "draggable" area,
    // like the titleBarWidget or imageLabel.
    if (event->button() == Qt::LeftButton)
    {
        // Check if the press is within the titleBarWidget or imageLabel bounds
        // relative to the LoginDialog.
        QPoint localPos = event->pos();
        // Ensure titleBarWidget is not null before accessing, though it should be initialized in setupUi
        bool onTitleBar = titleBarWidget ? titleBarWidget->geometry().contains(localPos) : false;
        bool onImageLabel = imageLabel ? imageLabel->geometry().contains(localPos) : false;

        if (onTitleBar || onImageLabel)
        { // Only allow dragging from these areas
            // Check if the click was on a button within the title bar
            if (onTitleBar)
            {
                QWidget *child = childAt(localPos);
                if (child == minimizeButton || child == closeButton)
                {
                    m_dragging = false; // Don't drag if a button was clicked
                    return;             // Let button handle its event
                }
            }
            m_dragging = true;
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
        else
        {
            m_dragging = false; // Don't drag if clicked on form elements
        }
    }
}

void LoginDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && m_dragging)
    {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void LoginDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = false;
        event->accept();
    }
}

// If WA_TranslucentBackground is true, and you need perfect rounded corners
// without relying solely on stylesheet border-radius for the top-level widget,
// you might need a paintEvent. However, for a simple dialog shape like this,
// having a containerWidget with border-radius and shadow, inside a transparent dialog,
// is often sufficient.
void LoginDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    // painter.setRenderHint(QPainter::Antialiasing); // Antialiasing for the fillRect is not usually necessary
    
    // Fill the entire dialog's rectangle with a transparent color.
    // This is crucial for WA_TranslucentBackground to work correctly with custom shapes,
    // ensuring that areas not covered by child widgets (like containerWidget) are truly transparent.
    painter.fillRect(rect(), Qt::transparent);

    // The actual visible content (containerWidget with its own background, border-radius, and shadow)
    // will be drawn by Qt's rendering system on top of this transparent background.
    // No need to call QDialog::paintEvent(event) if we are fully overriding.
}
