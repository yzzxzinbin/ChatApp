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
#include <QMessageBox> // New: For showing messages
#include <QSettings> // For saving UUID and password backup
#include <QUuid>     // For generating UUID

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), m_dragging(false), 
      forgotPasswordUnderlineContainer(nullptr), 
      forgotPasswordUnderline(nullptr),
      underlineAnimation(nullptr),
      m_dbManager(new DatabaseManager(this)), 
      m_loggedInUserIdStr("") // 初始化
{
    // 首先初始化颜色成员变量
    forgotPasswordNormalColor = QColor(Qt::darkGray).lighter(130); 
    forgotPasswordHoverColor = Qt::darkGray;                     
    underlineColor = Qt::darkGray; 

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);

    setupUi(); // 然后调用 setupUi
    applyStyles();
    this->update();

    setFixedSize(440, 500);

    // Attempt to connect to the database with specified credentials new pwd is 12345678
    // Host: "127.0.0.1", DB: "QTWork", User: "root", Pass: "12345678", Port: 3306
    if (!m_dbManager->connectToDatabase("127.0.0.1", "QTWork", "root", "12345678", 3306)) {
        // Error is handled by signal or logged by DatabaseManager
        QMessageBox::critical(this, tr("Database Error"), tr("Could not connect to the database 'QTWork'. Please ensure the database exists and credentials are correct. Login/Signup will not work."));
        // loginButton->setEnabled(false); // Example: disable buttons
        // signUpButton->setEnabled(false);
    }
    connect(m_dbManager, &DatabaseManager::errorOccurred, this, &LoginDialog::showDatabaseError);
}

LoginDialog::~LoginDialog()
{
    qInfo() << "LoginDialog destructor: Stopping animations.";
    if (underlineAnimation) {
        underlineAnimation->stop();
        // underlineAnimation is parented to 'this', Qt will delete it.
    }
    if (buttonWidthAnimationGroup) {
        buttonWidthAnimationGroup->stop();
        // buttonWidthAnimationGroup and its children are parented, Qt will delete them.
    }
    // QVariantAnimations in eventFilter are set to DeleteWhenStopped.
    // If the dialog is deleted while they are running, their lambdas are risky.
    // No direct way to stop them here as they are created on-the-fly in eventFilter.
    // The QPointer fix below is more robust for those.

    qInfo() << "LoginDialog destructor: Animations stopped. m_dbManager will be deleted by Qt's parent-child mechanism.";
    // m_dbManager is a child of LoginDialog, Qt will handle its deletion.
}

QString LoginDialog::getLoggedInUserId() const
{
    return m_loggedInUserIdStr;
}

void LoginDialog::onLoginClicked()
{
    if (!m_dbManager || !m_dbManager->isConnected()) {
        QMessageBox::warning(this, tr("Login Failed"), tr("Database is not connected. Cannot process login."));
        return;
    }

    QString username = usernameEdit->text().trimmed();
    QString password = passwordEdit->text(); // Get password as is

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("Login Failed"), tr("Username and password cannot be empty."));
        return;
    }

    if (m_dbManager->validateUser(username, password)) {
        QSettings settings;
        QString activeSessionKey = QString("ActiveSessions/%1").arg(username);
        if (settings.value(activeSessionKey, false).toBool()) {
            QMessageBox::warning(this, tr("Login Failed"), tr("User '%1' is already logged in on another instance.").arg(username));
            return;
        }

        settings.setValue(activeSessionKey, true);
        settings.sync();

        QMessageBox::information(this, tr("Login Successful"), tr("Welcome, %1!").arg(username));
        m_loggedInUserIdStr = username; // 存储成功登录的ID
        accept();
    } else {
        // Check if user exists to give a more specific error
        if (m_dbManager->userExists(username)) {
            QMessageBox::warning(this, tr("Login Failed"), tr("Invalid password for user '%1'.").arg(username));
        } else {
            QMessageBox::warning(this, tr("Login Failed"), tr("User '%1' not found.").arg(username));
        }
        passwordEdit->clear(); // Clear password field on failure
        passwordEdit->setFocus();
    }
}

void LoginDialog::onSignUpClicked()
{
    if (!m_dbManager || !m_dbManager->isConnected()) {
        QMessageBox::warning(this, tr("Sign Up Failed"), tr("Database is not connected. Cannot process sign up."));
        return;
    }

    QString userIdStr = usernameEdit->text().trimmed(); 
    QString password = passwordEdit->text();

    if (userIdStr.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("Sign Up Failed"), tr("User ID and password cannot be empty."));
        return;
    }
    
    // 前端用户ID重复检查 (基于QSettings)
    QSettings checkSettings;
    QString userProfilePath = QString("UserAccounts/%1/Profile/uuid").arg(userIdStr);
    if (checkSettings.contains(userProfilePath)) {
        QMessageBox::warning(this, tr("Sign Up Failed"), tr("User ID '%1' is already registered locally. Please choose a different User ID or log in.").arg(userIdStr));
        usernameEdit->setFocus();
        return;
    }

    bool ok;
    userIdStr.toInt(&ok); 
    if (!ok) {
        QMessageBox::warning(this, tr("Sign Up Failed"), tr("User ID must be an integer."));
        usernameEdit->setFocus();
        return;
    }

    if (password.length() < 6) { 
        QMessageBox::warning(this, tr("Sign Up Failed"), tr("Password must be at least 6 characters long."));
        return;
    }

    if (m_dbManager->addUser(userIdStr, password)) {
        // SQL添加成功后，在注册表中创建用户配置
        QSettings settings;
        QString userGroup = "UserAccounts/" + userIdStr;
        settings.beginGroup(userGroup + "/Profile");
        QString newUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("uuid", newUuid);
        settings.setValue("localUserName", userIdStr); // 默认用户名
        // !!重要: 存储密码备份 (理想情况下应哈希)
        // 为了简单起见，这里存储明文，但实际应用中应使用强哈希算法
        // 例如: settings.setValue("passwordHash", YourPasswordHasher::hash(password));
        settings.setValue("passwordBackup", password); 
        settings.endGroup();
        settings.sync(); // 确保写入

        qInfo() << "User ID" << userIdStr << "registered. Profile with UUID" << newUuid << "created in QSettings.";

        QMessageBox::information(this, tr("Sign Up Successful"), tr("User ID '%1' created successfully. You can now log in.").arg(userIdStr));
        usernameEdit->clear();
        passwordEdit->clear();
    } else {
        // Error message is handled by showDatabaseError slot via errorOccurred signal
    }
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
        underlineAnimation->setEasingCurve(QEasingCurve::InOutExpo); 
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

        // 使用 QPointer 包装 'this' 以在 lambda 中安全访问
        QPointer<LoginDialog> weakThis = this;

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
            connect(textColorAnim, &QVariantAnimation::valueChanged, this, [weakThis](const QVariant &value) {
                if (!weakThis) return; // 如果 LoginDialog 已销毁，则返回
                if (!weakThis->forgotPasswordLabel) return; // 如果标签已销毁，则返回
                QPalette palette = weakThis->forgotPasswordLabel->palette();
                palette.setColor(QPalette::WindowText, value.value<QColor>());
                weakThis->forgotPasswordLabel->setPalette(palette);
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
            connect(textColorAnim, &QVariantAnimation::valueChanged, this, [weakThis](const QVariant &value) {
                if (!weakThis) return; // 如果 LoginDialog 已销毁，则返回
                if (!weakThis->forgotPasswordLabel) return; // 如果标签已销毁，则返回
                QPalette palette = weakThis->forgotPasswordLabel->palette();
                palette.setColor(QPalette::WindowText, value.value<QColor>());
                weakThis->forgotPasswordLabel->setPalette(palette);
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

void LoginDialog::showDatabaseError(const QString& errorMsg)
{
    QMessageBox::critical(this, tr("Database Operation Error"), errorMsg);
}
