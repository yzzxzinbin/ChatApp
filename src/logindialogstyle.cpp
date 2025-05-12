#include "logindialog.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QMouseEvent>
#include <QApplication> 
#include <QGraphicsDropShadowEffect>
#include <QPainter> 
#include <QPainterPath> 
#include <QEvent> 
#include <QIcon> 
#include <QSizePolicy> 
#include <QWidget> 
#include <QPalette> 
#include <QVariantAnimation> 
#include <QTextDocument>     
#include <QRegularExpression> 

void LoginDialog::applyStyles()
{
    this->setStyleSheet(R"(
        #containerWidget {
            background-color: #ffffff; 
            border-radius: 15px; 
        }

        #imageLabel {
            background-color: #f0f0f0; 
            border-radius: 15px; 
            border: none; 
            padding: 0; 
            margin: 0; 
        }

        QCheckBox#rememberMeCheckBox {
            color: #555555; 
            font-size: 13px;
        }
        QCheckBox#rememberMeCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #dcdde1;
            border-radius: 4px;
            background-color: #f0f0f2;
        }
        QCheckBox#rememberMeCheckBox::indicator:checked {
            background-color: #0078d4; 
            image: url(:/icons/check_mark.svg); 
        }
        QCheckBox#rememberMeCheckBox::indicator:hover {
            border: 1px solid #0078d4;
        }

        QLabel#forgotPasswordLabel {
            font-size: 13px;
            text-decoration: none !important; /* 尝试增加 !important 来提升优先级 */
        }

        QPushButton#minimizeButton, QPushButton#closeButton {
            background-color: transparent;
            border: none;
            color:rgba(242, 240, 240, 0.5); 
            font-family: "Arial", sans-serif; 
            font-size: 16px; 
            font-weight: bold;
            min-width: 25px;
            max-width: 25px;
            min-height: 25px;
            max-height: 25px;
            border-radius: 4px; 
        }
        QPushButton#minimizeButton:hover, QPushButton#closeButton:hover {
            background-color: #e0e0e0; 
            color: #000000;
        }
        QPushButton#closeButton:hover {
            background-color: #ff6b6b; 
            color: white;
        }
        QPushButton#minimizeButton:pressed, QPushButton#closeButton:pressed {
            background-color: #c0c0c0; 
        }
         QPushButton#closeButton:pressed {
            background-color: #ee3535; 
            color: white;
        }

        #formContainer {
            background-color: #ffffff; 
            border-bottom-left-radius: 15px;  
            border-bottom-right-radius: 15px; 
        }

        QLineEdit {
            background-color: #f0f0f2; 
            border: 1px solid #dcdde1; 
            border-radius: 8px;
            padding: 12px 15px;
            color: #2f3542; 
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #0078d4; 
            background-color: #ffffff;
        }
        QLineEdit::placeholder { 
            color: #a4b0be;
        }

        QPushButton#loginButton {
            background-color:rgb(58, 153, 226); 
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            padding-left: 10px; 
            padding-right: 10px;
        }
        QPushButton#loginButton:hover {
            background-color:rgb(21, 131, 215); 
        }
        QPushButton#loginButton:pressed {
            background-color: #004578; 
        }

        QPushButton#signUpButton {
            background-color: #f0f0f2; 
            color: #2f3542; 
            border: 1px solid #dcdde1; 
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            padding-left: 10px; 
            padding-right: 10px;
        }
        QPushButton#signUpButton:hover {
            background-color: #e0e0e0; 
            border-color: #c0c0c0;
        }
        QPushButton#signUpButton:pressed {
            background-color: #d0d0d0;
        }
    )");

    QPalette palette = usernameEdit->palette();
    palette.setColor(QPalette::PlaceholderText, QColor("#a4b0be"));
    usernameEdit->setPalette(palette);
    passwordEdit->setPalette(palette);

    QPalette fpPalette = forgotPasswordLabel->palette();
    if (fpPalette.color(QPalette::WindowText) != forgotPasswordNormalColor) {
        fpPalette.setColor(QPalette::WindowText, forgotPasswordNormalColor);
        forgotPasswordLabel->setPalette(fpPalette);
    }
}


void LoginDialog::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    int shadowMargin = 20;
    mainLayout->setContentsMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
    mainLayout->setSpacing(0);

    QWidget *containerWidget = new QWidget(this);
    containerWidget->setObjectName("containerWidget");

    QVBoxLayout *containerLayout = new QVBoxLayout(containerWidget);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    imageLabel = new QLabel(containerWidget);
    imageLabel->setObjectName("imageLabel");
    int imageDisplayHeight = 200;
    QPixmap originalStarterPixmap(":/res/starter.png");
    if (originalStarterPixmap.isNull())
    {
        imageLabel->setText("Image not found (400x200)");
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("background-color: #555; color: white;");
    }
    else
    {
        QPixmap scaledPixmap = originalStarterPixmap.scaled(400, imageDisplayHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap roundedPixmap = createRoundedPixmap(scaledPixmap, 15);
        imageLabel->setPixmap(roundedPixmap);
    }
    imageLabel->setFixedHeight(imageDisplayHeight);
    imageLabel->setAlignment(Qt::AlignCenter);
    containerLayout->addWidget(imageLabel);

    QWidget *buttonOverlayWidget = new QWidget(imageLabel);
    buttonOverlayWidget->setAttribute(Qt::WA_TranslucentBackground);

    QHBoxLayout *imageButtonsLayout = new QHBoxLayout(buttonOverlayWidget);
    imageButtonsLayout->setContentsMargins(0, 5, 5, 0);
    imageButtonsLayout->setSpacing(5);
    imageButtonsLayout->addStretch();

    minimizeButton = new QPushButton("—", buttonOverlayWidget);
    minimizeButton->setObjectName("minimizeButton");
    minimizeButton->setFixedSize(25, 25);

    closeButton = new QPushButton("✕", buttonOverlayWidget);
    closeButton->setObjectName("closeButton");
    closeButton->setFixedSize(25, 25);

    imageButtonsLayout->addWidget(minimizeButton);
    imageButtonsLayout->addWidget(closeButton);

    QVBoxLayout *imageLabelInternalLayout = new QVBoxLayout(imageLabel);
    imageLabelInternalLayout->setContentsMargins(0, 0, 0, 0);
    imageLabelInternalLayout->addWidget(buttonOverlayWidget, 0, Qt::AlignTop);
    imageLabelInternalLayout->addStretch(1);

    formContainer = new QWidget(containerWidget);
    formContainer->setObjectName("formContainer");
    QVBoxLayout *formLayout = new QVBoxLayout(formContainer);
    formLayout->setContentsMargins(30, 25, 30, 20);
    formLayout->setSpacing(18);

    usernameEdit = new QLineEdit(formContainer);
    usernameEdit->setObjectName("usernameEdit");
    usernameEdit->setPlaceholderText(tr("Username"));

    passwordEdit = new QLineEdit(formContainer);
    passwordEdit->setObjectName("passwordEdit");
    passwordEdit->setPlaceholderText(tr("Password"));
    passwordEdit->setEchoMode(QLineEdit::Password);

    QHBoxLayout *optionsLayout = new QHBoxLayout();
    rememberMeCheckBox = new QCheckBox(tr("Remember me"), formContainer);
    rememberMeCheckBox->setObjectName("rememberMeCheckBox");

    QWidget *forgotPasswordInteractiveWidget = new QWidget(formContainer);
    QVBoxLayout *fpVerticalLayout = new QVBoxLayout(forgotPasswordInteractiveWidget);
    fpVerticalLayout->setContentsMargins(0, 0, 0, 0);
    fpVerticalLayout->setSpacing(0);

    forgotPasswordLabel = new QLabel(tr("Forgot password?"), forgotPasswordInteractiveWidget); 
    forgotPasswordLabel->setObjectName("forgotPasswordLabel");
    forgotPasswordLabel->setCursor(Qt::PointingHandCursor); // 保持手形光标以指示可交互
    forgotPasswordLabel->installEventFilter(this);

    QPalette fpPalette = forgotPasswordLabel->palette();
    fpPalette.setColor(QPalette::WindowText, forgotPasswordNormalColor); // 设置初始文本颜色
    forgotPasswordLabel->setPalette(fpPalette);

    forgotPasswordUnderlineContainer = new QWidget(forgotPasswordInteractiveWidget);
    forgotPasswordUnderlineContainer->setFixedHeight(2);

    forgotPasswordUnderline = new QWidget(forgotPasswordUnderlineContainer);
    forgotPasswordUnderline->setStyleSheet(QString("background-color: %1;").arg(underlineColor.name()));
    
    fpVerticalLayout->addWidget(forgotPasswordLabel);
    fpVerticalLayout->addWidget(forgotPasswordUnderlineContainer);
    forgotPasswordInteractiveWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    optionsLayout->addWidget(rememberMeCheckBox);
    optionsLayout->addStretch();
    optionsLayout->addWidget(forgotPasswordInteractiveWidget);

    QWidget *actionButtonsContainer = new QWidget(formContainer);
    QHBoxLayout *actionButtonsLayout = new QHBoxLayout(actionButtonsContainer);
    actionButtonsLayout->setContentsMargins(0, 0, 0, 0);
    actionButtonsLayout->setSpacing(15);

    loginButton = new QPushButton(actionButtonsContainer);
    loginButton->setObjectName("loginButton");
    loginButton->setFixedHeight(45);
    loginButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    signUpButton = new QPushButton(actionButtonsContainer);
    signUpButton->setObjectName("signUpButton");
    signUpButton->setFixedHeight(45);
    signUpButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    int totalWidthForButtonsContainer = 340;
    int spacingBetweenButtons = 15;
    int totalWidthForButtonsOnly = totalWidthForButtonsContainer - spacingBetweenButtons;

    initialLoginWidth = qRound(totalWidthForButtonsOnly * 0.82);
    initialSignUpWidth = totalWidthForButtonsOnly - initialLoginWidth;

    targetSignUpWidthOnSignUpHover = qRound(totalWidthForButtonsOnly * 0.82);
    targetLoginWidthOnSignUpHover = totalWidthForButtonsOnly - targetSignUpWidthOnSignUpHover;

    if (initialLoginWidth + initialSignUpWidth != totalWidthForButtonsOnly) {
        initialSignUpWidth = totalWidthForButtonsOnly - initialLoginWidth;
    }
    if (targetLoginWidthOnSignUpHover + targetSignUpWidthOnSignUpHover != totalWidthForButtonsOnly) {
        targetSignUpWidthOnSignUpHover = qRound(totalWidthForButtonsOnly * 0.82);
        targetLoginWidthOnSignUpHover = totalWidthForButtonsOnly - targetSignUpWidthOnSignUpHover;
    }

    loginButton->setFixedWidth(initialLoginWidth);
    loginButton->setText(tr("Login"));
    loginButton->setIcon(QIcon());

    signUpButton->setFixedWidth(initialSignUpWidth);
    signUpButton->setText("");
    signUpButton->setIcon(QIcon(":/icons/register.svg"));
    signUpButton->setIconSize(QSize(24, 24));

    actionButtonsLayout->addWidget(loginButton);
    actionButtonsLayout->addWidget(signUpButton);

    actionButtonsLayout->setStretchFactor(loginButton, 0);
    actionButtonsLayout->setStretchFactor(signUpButton, 0);

    actionButtonsContainer->setFixedWidth(totalWidthForButtonsContainer);

    formLayout->addWidget(usernameEdit);
    formLayout->addWidget(passwordEdit);
    formLayout->addLayout(optionsLayout);
    formLayout->addSpacing(4);
    formLayout->addWidget(actionButtonsContainer);
    formLayout->addStretch();

    containerLayout->addWidget(formContainer, 1);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setXOffset(0);
    shadow->setYOffset(5);
    shadow->setColor(QColor(0, 0, 0, 100));
    containerWidget->setGraphicsEffect(shadow);

    mainLayout->addWidget(containerWidget);
    setLayout(mainLayout);

    buttonWidthAnimationGroup = new QParallelAnimationGroup(this);

    loginMinSizeAnimation = new QPropertyAnimation(loginButton, "minimumWidth", this);
    loginMinSizeAnimation->setDuration(200);
    loginMinSizeAnimation->setEasingCurve(QEasingCurve::InOutSine);
    buttonWidthAnimationGroup->addAnimation(loginMinSizeAnimation);

    loginMaxSizeAnimation = new QPropertyAnimation(loginButton, "maximumWidth", this);
    loginMaxSizeAnimation->setDuration(200);
    loginMaxSizeAnimation->setEasingCurve(QEasingCurve::InOutSine);
    buttonWidthAnimationGroup->addAnimation(loginMaxSizeAnimation);

    signUpMinSizeAnimation = new QPropertyAnimation(signUpButton, "minimumWidth", this);
    signUpMinSizeAnimation->setDuration(200);
    signUpMinSizeAnimation->setEasingCurve(QEasingCurve::InOutSine);
    buttonWidthAnimationGroup->addAnimation(signUpMinSizeAnimation);

    signUpMaxSizeAnimation = new QPropertyAnimation(signUpButton, "maximumWidth", this);
    signUpMaxSizeAnimation->setDuration(200);
    signUpMaxSizeAnimation->setEasingCurve(QEasingCurve::InOutSine);
    buttonWidthAnimationGroup->addAnimation(signUpMaxSizeAnimation);

    signUpButton->installEventFilter(this);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(minimizeButton, &QPushButton::clicked, this, &LoginDialog::onMinimizeClicked);
    connect(closeButton, &QPushButton::clicked, this, &LoginDialog::onCloseClicked);
}
