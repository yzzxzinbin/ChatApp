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
#include <QPainterPath> // 新增：用于创建圆角路径
#include <QEvent> // 新增
#include <QIcon> // 新增：用于按钮图标
#include <QSizePolicy> // 新增：用于 QSizePolicy
#include <QWidget> // 新增：用于 QWidget

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), m_dragging(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi();
    applyStyles();
    this->update();

    // The shadow has a blurRadius of 25 and a yOffset of 5.
    // We need to make the dialog larger than the containerWidget to accommodate the shadow.
    // Original containerWidget effective size is 400x490.
    // New Dialog width = 400 (container) + 2 * 20 (margins) = 440
    // New Dialog height = 490 (container) + 2 * 20 (margins) + ~40 (for new row) = 570
    setFixedSize(440, 540); // 增加了高度以容纳新行
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

    // Top Image Part
    imageLabel = new QLabel(containerWidget);
    imageLabel->setObjectName("imageLabel");
    // Assuming dialog width is 400, image height for 2:1 ratio would be 200
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
        QPixmap roundedPixmap = createRoundedPixmap(scaledPixmap, 15); // 创建圆角图片
        imageLabel->setPixmap(roundedPixmap);
    }
    imageLabel->setFixedHeight(imageDisplayHeight);
    imageLabel->setAlignment(Qt::AlignCenter);
    containerLayout->addWidget(imageLabel);

    // Create a widget to act as a container for buttons, overlaying imageLabel
    QWidget *buttonOverlayWidget = new QWidget(imageLabel); // Parent is imageLabel
    buttonOverlayWidget->setAttribute(Qt::WA_TranslucentBackground);

    QHBoxLayout *imageButtonsLayout = new QHBoxLayout(buttonOverlayWidget); // Layout for the overlay widget
    imageButtonsLayout->setContentsMargins(0, 5, 5, 0);
    imageButtonsLayout->setSpacing(5);
    imageButtonsLayout->addStretch(); // Push buttons to the right

    minimizeButton = new QPushButton("—", buttonOverlayWidget); // Parent is buttonOverlayWidget
    minimizeButton->setObjectName("minimizeButton");
    minimizeButton->setFixedSize(25, 25); // Ensure buttons have a fixed size for consistent look

    closeButton = new QPushButton("✕", buttonOverlayWidget); // Parent is buttonOverlayWidget
    closeButton->setObjectName("closeButton");
    closeButton->setFixedSize(25, 25); // Ensure buttons have a fixed size

    imageButtonsLayout->addWidget(minimizeButton);
    imageButtonsLayout->addWidget(closeButton);

    QVBoxLayout *imageLabelInternalLayout = new QVBoxLayout(imageLabel); // Layout for imageLabel itself
    imageLabelInternalLayout->setContentsMargins(0, 0, 0, 0);
    imageLabelInternalLayout->addWidget(buttonOverlayWidget, 0, Qt::AlignTop); // Add overlay, align top
    imageLabelInternalLayout->addStretch(1);                                   // Push overlay to top if imageLabel is taller than overlay

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

    // 新增：记住密码和忘记密码行
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    rememberMeCheckBox = new QCheckBox(tr("Remember me"), formContainer);
    rememberMeCheckBox->setObjectName("rememberMeCheckBox");
    
    forgotPasswordLabel = new QLabel(tr("<a href=\"#\">Forgot password?</a>"), formContainer);
    forgotPasswordLabel->setObjectName("forgotPasswordLabel");
    forgotPasswordLabel->setTextFormat(Qt::RichText);
    forgotPasswordLabel->setTextInteractionFlags(Qt::TextBrowserInteraction); // 使其可点击
    forgotPasswordLabel->setOpenExternalLinks(false); // 在应用内处理点击

    optionsLayout->addWidget(rememberMeCheckBox);
    optionsLayout->addStretch();
    optionsLayout->addWidget(forgotPasswordLabel);

    // 新增：登录和注册按钮行
    QWidget *actionButtonsContainer = new QWidget(formContainer); 
    QHBoxLayout *actionButtonsLayout = new QHBoxLayout(actionButtonsContainer); 
    actionButtonsLayout->setContentsMargins(0, 0, 0, 0); 
    actionButtonsLayout->setSpacing(15); // 修改：使用布局的 spacing 属性

    loginButton = new QPushButton(actionButtonsContainer); 
    loginButton->setObjectName("loginButton");
    loginButton->setFixedHeight(45);
    loginButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // 新增：明确尺寸策略

    signUpButton = new QPushButton(actionButtonsContainer); 
    signUpButton->setObjectName("signUpButton");
    signUpButton->setFixedHeight(45);
    signUpButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // 新增：明确尺寸策略

    int totalWidthForButtonsContainer = 340; 
    int spacingBetweenButtons = 15;
    int totalWidthForButtonsOnly = totalWidthForButtonsContainer - spacingBetweenButtons;

    // 初始状态：登录82%，注册18%
    initialLoginWidth = qRound(totalWidthForButtonsOnly * 0.82); // 从 0.75 改为 0.82
    initialSignUpWidth = totalWidthForButtonsOnly - initialLoginWidth; 

    // 悬停在注册按钮上时：登录18%，注册82%
    targetSignUpWidthOnSignUpHover = qRound(totalWidthForButtonsOnly * 0.82); // 从 0.75 改为 0.82
    targetLoginWidthOnSignUpHover = totalWidthForButtonsOnly - targetSignUpWidthOnSignUpHover; 

    // 确保总和严格等于 totalWidthForButtonsOnly
    // 如果因为 qRound 导致偏差，调整其中一个以匹配
    if (initialLoginWidth + initialSignUpWidth != totalWidthForButtonsOnly) {
        initialSignUpWidth = totalWidthForButtonsOnly - initialLoginWidth;
    }
    if (targetLoginWidthOnSignUpHover + targetSignUpWidthOnSignUpHover != totalWidthForButtonsOnly) {
        // 确保 signUpButton 获得目标宽度，loginButton 填充剩余
        targetSignUpWidthOnSignUpHover = qRound(totalWidthForButtonsOnly * 0.82); // 重新计算以确保 signUp 优先
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
    
    actionButtonsLayout->setStretchFactor(loginButton, 0); // 按钮不拉伸
    actionButtonsLayout->setStretchFactor(signUpButton, 0); // 按钮不拉伸

    actionButtonsContainer->setFixedWidth(totalWidthForButtonsContainer); 

    formLayout->addWidget(usernameEdit);
    formLayout->addWidget(passwordEdit);
    formLayout->addLayout(optionsLayout); 
    formLayout->addSpacing(10);           
    formLayout->addWidget(actionButtonsContainer); // 5. 将容器QWidget添加到formLayout
    formLayout->addStretch();

    containerLayout->addWidget(formContainer, 1);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this); // 'this' or 'containerWidget' as parent for effect
    shadow->setBlurRadius(25);
    shadow->setXOffset(0);
    shadow->setYOffset(5);                  // Slight downward offset
    shadow->setColor(QColor(0, 0, 0, 100)); // Semi-transparent black
    containerWidget->setGraphicsEffect(shadow);

    mainLayout->addWidget(containerWidget);
    setLayout(mainLayout);

    // 初始化动画
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

    signUpButton->installEventFilter(this); // 在 signUpButton 上安装事件过滤器

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

        /* 
         * Crucial part for top rounded corners:
         * imageLabel is the first child of containerWidget in the vertical layout.
         * Its background must also have top rounded corners to match containerWidget.
         * NOW TRYING ALL-ROUNDED CORNERS FOR imageLabel.
         */
        #imageLabel {
            background-color: #f0f0f0; /* Fallback/area bg for image */
            border-radius: 15px; /* All corners rounded */
            border: none; 
            padding: 0; 
            margin: 0; 
        }

        QCheckBox#rememberMeCheckBox {
            color: #555555; /* 与标签文本颜色一致 */
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
            background-color: #0078d4; /* 选中时颜色与主按钮一致 */
            image: url(:/icons/check_mark.svg); /* 可选：自定义勾选图标 */
        }
        QCheckBox#rememberMeCheckBox::indicator:hover {
            border: 1px solid #0078d4;
        }

        QLabel#forgotPasswordLabel {
            color: #0078d4; /* 链接颜色 */
            font-size: 13px;
            text-decoration: none; /* 默认无下划线 */
        }
        QLabel#forgotPasswordLabel:hover {
            text-decoration: underline; /* 悬停时有下划线 */
        }

        QPushButton#minimizeButton, QPushButton#closeButton {
            background-color: transparent;
            border: none;
            color:rgba(242, 240, 240, 0.5); /* Adjust color as needed */
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
            padding-left: 10px; /* 为图标腾出空间（如果适用） */
            padding-right: 10px;
        }
        QPushButton#loginButton:hover {
            background-color: #005a9e; /* Darker blue */
        }
        QPushButton#loginButton:pressed {
            background-color: #004578; /* Even darker blue */
        }

        /* 注册按钮样式 - 次要按钮风格 */
        QPushButton#signUpButton {
            background-color: #f0f0f2; /* Light grey background, similar to QLineEdit */
            color: #2f3542; /* Dark grey text */
            border: 1px solid #dcdde1; /* Light border */
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            padding-left: 10px; /* 为图标腾出空间（如果适用） */
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
    // like the imageLabel (but not on its child buttons).
    if (event->button() == Qt::LeftButton)
    {
        QPoint localPos = event->pos(); // Event position in LoginDialog's coordinates

        // Get the geometry of imageLabel relative to LoginDialog
        QRect imageLabelRectInDialogCoords;
        if (imageLabel && imageLabel->parentWidget())
        {
            // Map the top-left point of imageLabel (which is in its parent's coords) to LoginDialog's coords
            QPoint imageLabelTopLeftInDialogCoords = imageLabel->mapTo(this, QPoint(0, 0));
            imageLabelRectInDialogCoords = QRect(imageLabelTopLeftInDialogCoords, imageLabel->size());
        }

        bool onImageLabel = imageLabelRectInDialogCoords.contains(localPos);

        if (onImageLabel)
        {
            // Check if the click was on a button *within* the imageLabel
            // mapFromGlobal returns QPointF, childAt expects QPoint.
            QPoint pointInImageLabelCoords = imageLabel->mapFromGlobal(event->globalPosition()).toPoint();
            QWidget *child = imageLabel->childAt(pointInImageLabelCoords);

            if (child == minimizeButton || child == closeButton)
            {
                m_dragging = false; // Don't drag if a button was clicked
                return;             // Let button handle its event
            }

            m_dragging = true;
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
        else
        {
            m_dragging = false; // Don't drag if clicked on form elements or outside imageLabel
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

// 新增：实现创建圆角 QPixmap 的辅助方法
QPixmap LoginDialog::createRoundedPixmap(const QPixmap &source, int radius)
{
    if (source.isNull()) {
        return QPixmap();
    }

    // 创建一个与源图片大小相同，且背景透明的新 QPixmap
    QPixmap result(source.size());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true); // 抗锯齿
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true); // 平滑缩放（如果源是缩放过的）

    // 创建一个圆角矩形路径
    QPainterPath path;
    path.addRoundedRect(result.rect(), radius, radius);

    // 设置剪切路径
    painter.setClipPath(path);

    // 将源图片绘制到目标 QPixmap 上（在剪切路径内）
    painter.drawPixmap(0, 0, source);

    return result;
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == signUpButton) {
        if (event->type() == QEvent::Enter) {
            buttonWidthAnimationGroup->stop(); 

            // 获取当前的实际宽度作为动画起点
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

            // 获取当前的实际宽度作为动画起点
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
    }
    return QDialog::eventFilter(watched, event);
}
