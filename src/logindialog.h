#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QCheckBox> // 新增：包含 QCheckBox 头文件

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QMouseEvent;
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
    void paintEvent(QPaintEvent *event) override; // For custom painting if WA_TranslucentBackground is used

private slots:
    void onLoginClicked();
    void onMinimizeClicked(); // Slot for minimize button
    void onCloseClicked();    // Slot for close button
    // void onForgotPasswordClicked(); // Placeholder for future
    // void onSignUpClicked();       // Placeholder for future

private:
    void setupUi();
    void applyStyles();
    QPixmap createRoundedPixmap(const QPixmap &source, int radius); // 新增辅助方法

    QLabel *imageLabel;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QCheckBox *rememberMeCheckBox; // 新增
    QLabel *forgotPasswordLabel;   // 新增
    QPushButton *loginButton;
    QPushButton *signUpButton;     // 新增
    QPushButton *minimizeButton; // New
    QPushButton *closeButton;    // New
    // QLabel *forgotPasswordLabel; // Placeholder
    // QPushButton *signUpButton;    // Placeholder

    QVBoxLayout *mainLayout;
    QWidget *formContainer;

    bool m_dragging;
    QPoint m_dragPosition;
};

#endif // LOGINDIALOG_H
