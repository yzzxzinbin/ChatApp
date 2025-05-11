#include "logindialog.h"
#include <QLineEdit>          // 用于 QLineEdit 类
#include <QPushButton>        // 用于 QPushButton 类
#include <QGraphicsDropShadowEffect> // 用于阴影效果
#include <QPainter>           // 用于自定义绘制
#include <QPainterPath>       // 用于创建圆角路径
#include <QEvent>             // 用于事件处理
#include <QIcon>              // 用于按钮图标
#include <QShowEvent>         // 用于窗口显示事件
#include <QLabel>             // 用于 QLabel 类
#include <QPalette>           // 用于调色板设置
#include <QMessageBox>        // 用于显示消息框（如果用到）

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