#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QCheckBox> 
#include <QPropertyAnimation> 
#include <QParallelAnimationGroup> 
#include <QColor> // 新增
#include <QShowEvent> // 新增
#include "databasemanager.h" // New: Include DatabaseManager

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QMouseEvent;
class QVariantAnimation; // Forward declare
QT_END_NAMESPACE

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
    QString getLoggedInUserId() const; // 正确的 const 位置和注释

protected: // 将事件处理器移至 protected
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override; 
    bool eventFilter(QObject *watched, QEvent *event) override; 
    void showEvent(QShowEvent *event) override; // 新增

private slots:
    void onLoginClicked();
    void onMinimizeClicked(); 
    void onCloseClicked();    
    void onSignUpClicked(); // New: Slot for sign up button
    void onForgotPasswordClicked(); // 新增：忘记密码槽

private:
    void setupUi();
    void applyStyles();
    QPixmap createRoundedPixmap(const QPixmap &source, int radius); 
    void showDatabaseError(const QString& errorMsg); // New: Helper to show DB errors

    QLabel *imageLabel;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QCheckBox *rememberMeCheckBox; 
    QLabel *forgotPasswordLabel;   
    QPushButton *loginButton;
    QPushButton *signUpButton;     
    QPushButton *minimizeButton; 
    QPushButton *closeButton;    

    QVBoxLayout *mainLayout;
    QWidget *formContainer;

    bool m_dragging;
    QPoint m_dragPosition;

    // 动画相关成员
    QPropertyAnimation *loginMinSizeAnimation;
    QPropertyAnimation *loginMaxSizeAnimation;
    QPropertyAnimation *signUpMinSizeAnimation;
    QPropertyAnimation *signUpMaxSizeAnimation;
    QParallelAnimationGroup *buttonWidthAnimationGroup; 

    // Forgot Password 动画及相关成员
    QColor forgotPasswordNormalColor;      
    QColor forgotPasswordHoverColor;       
    QColor underlineColor;                 
    QWidget* forgotPasswordUnderlineContainer; 
    QWidget* forgotPasswordUnderline;          
    QPropertyAnimation* underlineAnimation;    

    int initialLoginWidth;
    int initialSignUpWidth;
    int targetLoginWidthOnSignUpHover; 
    int targetSignUpWidthOnSignUpHover; 

    DatabaseManager *m_dbManager; // New: DatabaseManager instance
    QString m_loggedInUserIdStr; // 确保此成员变量存在
};

#endif // LOGINDIALOG_H
