#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QCheckBox> 
#include <QPropertyAnimation> // 新增
#include <QParallelAnimationGroup> // 新增

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QMouseEvent;
// QPropertyAnimation 和 QParallelAnimationGroup 已包含
QT_END_NAMESPACE

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override; 
    bool eventFilter(QObject *watched, QEvent *event) override; // 新增

private slots:
    void onLoginClicked();
    void onMinimizeClicked(); 
    void onCloseClicked();    
    // void onForgotPasswordClicked(); 
    // void onSignUpClicked();       

private:
    void setupUi();
    void applyStyles();
    QPixmap createRoundedPixmap(const QPixmap &source, int radius); 

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
    QParallelAnimationGroup *buttonWidthAnimationGroup; // 单个动画组

    int initialLoginWidth;
    int initialSignUpWidth;
    int targetLoginWidthOnSignUpHover; 
    int targetSignUpWidthOnSignUpHover; 
};

#endif // LOGINDIALOG_H
