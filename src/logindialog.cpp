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

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), m_dragging(false), 
      forgotPasswordUnderlineContainer(nullptr), 
      forgotPasswordUnderline(nullptr),
      underlineAnimation(nullptr)
{
    // 首先初始化颜色成员变量
    forgotPasswordNormalColor = QColor(Qt::darkGray).lighter(130); 
    forgotPasswordHoverColor = Qt::darkGray;                     
    underlineColor = Qt::darkGray; 

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi(); // 然后调用 setupUi
    applyStyles();
    this->update();

    setFixedSize(440, 500);
}

LoginDialog::~LoginDialog()
{
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
    loginMinSizeAnimation->setDuration(300);
    loginMinSizeAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    buttonWidthAnimationGroup->addAnimation(loginMinSizeAnimation);

    loginMaxSizeAnimation = new QPropertyAnimation(loginButton, "maximumWidth", this);
    loginMaxSizeAnimation->setDuration(300);
    loginMaxSizeAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    buttonWidthAnimationGroup->addAnimation(loginMaxSizeAnimation);

    signUpMinSizeAnimation = new QPropertyAnimation(signUpButton, "minimumWidth", this);
    signUpMinSizeAnimation->setDuration(300);
    signUpMinSizeAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    buttonWidthAnimationGroup->addAnimation(signUpMinSizeAnimation);

    signUpMaxSizeAnimation = new QPropertyAnimation(signUpButton, "maximumWidth", this);
    signUpMaxSizeAnimation->setDuration(300);
    signUpMaxSizeAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    buttonWidthAnimationGroup->addAnimation(signUpMaxSizeAnimation);

    signUpButton->installEventFilter(this);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(minimizeButton, &QPushButton::clicked, this, &LoginDialog::onMinimizeClicked);
    connect(closeButton, &QPushButton::clicked, this, &LoginDialog::onCloseClicked);
}


void LoginDialog::onLoginClicked()
{
    accept();
}

void LoginDialog::onMinimizeClicked()
{
    this->showMinimized();
}

void LoginDialog::onCloseClicked()
{
    this->reject();
}

void LoginDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        QPoint localPos = event->pos();

        QRect imageLabelRectInDialogCoords;
        if (imageLabel && imageLabel->parentWidget())
        {
            QPoint imageLabelTopLeftInDialogCoords = imageLabel->mapTo(this, QPoint(0, 0));
            imageLabelRectInDialogCoords = QRect(imageLabelTopLeftInDialogCoords, imageLabel->size());
        }

        bool onImageLabel = imageLabelRectInDialogCoords.contains(localPos);

        if (onImageLabel)
        {
            QPoint pointInImageLabelCoords = imageLabel->mapFromGlobal(event->globalPosition()).toPoint();
            QWidget *child = imageLabel->childAt(pointInImageLabelCoords);

            if (child == minimizeButton || child == closeButton)
            {
                m_dragging = false;
                return;
            }

            m_dragging = true;
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
        else
        {
            m_dragging = false;
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

void LoginDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::transparent);
}

QPixmap LoginDialog::createRoundedPixmap(const QPixmap &source, int radius)
{
    if (source.isNull()) {
        return QPixmap();
    }

    QPixmap result(source.size());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(result.rect(), radius, radius);

    painter.setClipPath(path);
    painter.drawPixmap(0, 0, source);

    return result;
}

void LoginDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event); 

    if (forgotPasswordLabel && forgotPasswordUnderlineContainer && forgotPasswordUnderline && !underlineAnimation) {
        QString plainText = forgotPasswordLabel->text(); // 直接获取纯文本
        
        QFontMetrics fm = forgotPasswordLabel->fontMetrics();
        int textWidth = fm.horizontalAdvance(plainText);

        if (textWidth <= 0) {
            textWidth = 100;
        }

        forgotPasswordUnderlineContainer->setFixedWidth(textWidth);
        forgotPasswordUnderline->setFixedSize(textWidth, forgotPasswordUnderlineContainer->height());
        // 确保初始位置在左侧外部
        forgotPasswordUnderline->move(-textWidth, 0); 

        underlineAnimation = new QPropertyAnimation(forgotPasswordUnderline, "pos", this);
        underlineAnimation->setDuration(250); 
        underlineAnimation->setEasingCurve(QEasingCurve::InOutQuad); 
    }
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == signUpButton) {
        if (event->type() == QEvent::Enter) {
            buttonWidthAnimationGroup->stop();

            int currentLoginWidth = loginButton->width();
            int currentSignUpWidth = signUpButton->width();

            loginMinSizeAnimation->setStartValue(currentLoginWidth);
            loginMaxSizeAnimation->setStartValue(currentLoginWidth);
            signUpMinSizeAnimation->setStartValue(currentSignUpWidth);
            signUpMaxSizeAnimation->setStartValue(currentSignUpWidth);

            loginMinSizeAnimation->setEndValue(targetLoginWidthOnSignUpHover);
            loginMaxSizeAnimation->setEndValue(targetLoginWidthOnSignUpHover);
            signUpMinSizeAnimation->setEndValue(targetSignUpWidthOnSignUpHover);
            signUpMaxSizeAnimation->setEndValue(targetSignUpWidthOnSignUpHover);
            
            signUpButton->setText(tr("Sign Up"));
            signUpButton->setIcon(QIcon());

            loginButton->setText("");
            loginButton->setIcon(QIcon(":/icons/login.svg"));
            loginButton->setIconSize(QSize(24, 24));

            buttonWidthAnimationGroup->start();
            return true; 
        } else if (event->type() == QEvent::Leave) {
            buttonWidthAnimationGroup->stop();

            int currentLoginWidth = loginButton->width();
            int currentSignUpWidth = signUpButton->width();

            loginMinSizeAnimation->setStartValue(currentLoginWidth);
            loginMaxSizeAnimation->setStartValue(currentLoginWidth);
            signUpMinSizeAnimation->setStartValue(currentSignUpWidth);
            signUpMaxSizeAnimation->setStartValue(currentSignUpWidth);

            loginMinSizeAnimation->setEndValue(initialLoginWidth);
            loginMaxSizeAnimation->setEndValue(initialLoginWidth);
            signUpMinSizeAnimation->setEndValue(initialSignUpWidth);
            signUpMaxSizeAnimation->setEndValue(initialSignUpWidth);

            signUpButton->setText("");
            signUpButton->setIcon(QIcon(":/icons/register.svg"));
            signUpButton->setIconSize(QSize(24, 24));

            loginButton->setText(tr("Login"));
            loginButton->setIcon(QIcon());
            
            buttonWidthAnimationGroup->start();
            return true; 
        }
    } else if (watched == forgotPasswordLabel) {
        if (!underlineAnimation || !forgotPasswordUnderline || !forgotPasswordUnderlineContainer) { // 添加了对 container 的检查
            return QDialog::eventFilter(watched, event);
        }

        // 每次都从 forgotPasswordUnderlineContainer 获取宽度，以防万一标签文本动态改变（虽然本例中不常见）
        int textWidth = forgotPasswordUnderlineContainer->width(); 
        if (textWidth <= 0) textWidth = 100; // Absolute fallback

        if (event->type() == QEvent::Enter) {
            // 文本颜色动画
            QVariantAnimation *textColorAnim = new QVariantAnimation(this);
            textColorAnim->setDuration(250); // 动画时长与下划线一致
            textColorAnim->setEasingCurve(QEasingCurve::InOutQuad); // 缓动曲线与下划线一致
            // 使用 palette 获取当前颜色作为起始值，而不是直接用 forgotPasswordNormalColor
            // 因为可能存在动画中途再次触发 enter 的情况
            QPalette currentPalette = forgotPasswordLabel->palette();
            textColorAnim->setStartValue(currentPalette.color(QPalette::WindowText));
            textColorAnim->setEndValue(forgotPasswordHoverColor);
            connect(textColorAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
                QPalette palette = forgotPasswordLabel->palette();
                palette.setColor(QPalette::WindowText, value.value<QColor>());
                forgotPasswordLabel->setPalette(palette);
            });
            textColorAnim->start(QAbstractAnimation::DeleteWhenStopped);

            // 下划线动画：滑入
            underlineAnimation->stop(); // 停止当前动画
            // 确保滑入动画总是从左侧外部开始
            forgotPasswordUnderline->move(-textWidth, 0); // 立即重置到左侧外部
            underlineAnimation->setStartValue(QPoint(-textWidth, 0)); // 起始位置
            underlineAnimation->setEndValue(QPoint(0, 0));           // 目标位置
            underlineAnimation->setDirection(QAbstractAnimation::Forward); 
            underlineAnimation->start();
            return true;

        } else if (event->type() == QEvent::Leave) {
            // 文本颜色动画
            QVariantAnimation *textColorAnim = new QVariantAnimation(this);
            textColorAnim->setDuration(250);
            textColorAnim->setEasingCurve(QEasingCurve::InOutQuad);
            // 使用 palette 获取当前颜色作为起始值
            QPalette currentPalette = forgotPasswordLabel->palette();
            textColorAnim->setStartValue(currentPalette.color(QPalette::WindowText));
            textColorAnim->setEndValue(forgotPasswordNormalColor);
            connect(textColorAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
                QPalette palette = forgotPasswordLabel->palette();
                palette.setColor(QPalette::WindowText, value.value<QColor>());
                forgotPasswordLabel->setPalette(palette);
            });
            textColorAnim->start(QAbstractAnimation::DeleteWhenStopped);

            // 下划线动画：滑出 (向右)
            underlineAnimation->stop(); // 停止当前动画
            // 滑出动画从当前位置（应该是0,0）开始
            underlineAnimation->setStartValue(forgotPasswordUnderline->pos()); 
            underlineAnimation->setEndValue(QPoint(textWidth, 0)); // 目标位置：滑出到右侧
            underlineAnimation->setDirection(QAbstractAnimation::Forward); 
            underlineAnimation->start();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}
