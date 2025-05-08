#include "chatmessagedisplay.h"
#include <QResizeEvent>
#include <QScrollBar>
#include <QLabel>

ChatMessageDisplay::ChatMessageDisplay(QWidget *parent)
    : QScrollArea(parent)
{
    // 设置滚动区域的属性
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);
    
    // 创建内容容器和布局
    contentWidget = new QWidget(this);
    contentWidget->setObjectName("chatContentWidget");
    
    contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(2);
    contentLayout->setContentsMargins(10, 10, 10, 10);
    
    // 创建顶部空白区域，使消息保持在底部
    spacerWidget = new QWidget(contentWidget);
    spacerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    contentLayout->addWidget(spacerWidget, 1);  // 1表示伸缩系数，会占用所有可用空间
    
    // 设置内容组件
    setWidget(contentWidget);
    
    // 应用样式
    setStyleSheet(R"(
        ChatMessageDisplay {
            background-color: #ffffff;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
        }
        
        #chatContentWidget {
            background-color: transparent;
        }
        
        QLabel {
            padding: 5px;
            margin: 3px 0px;
            background-color: transparent;
            line-height: 140%;
        }
    )");
}

void ChatMessageDisplay::addMessage(const QString &html)
{
    // 创建新的消息标签
    QLabel *messageLabel = new QLabel(html, contentWidget);
    messageLabel->setTextFormat(Qt::RichText);
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    messageLabel->setOpenExternalLinks(false);
    
    // 确保标签能够正确显示富文本
    messageLabel->setMargin(2);
    messageLabel->setMinimumWidth(100);
    
    // 添加到布局中，在spacer之后
    contentLayout->addWidget(messageLabel);
    
    // 滚动到底部
    scrollToBottom();
}

void ChatMessageDisplay::clear()
{
    // 保留spacer，删除所有其他组件
    QLayoutItem *item;
    while ((item = contentLayout->takeAt(1)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void ChatMessageDisplay::setMessages(const QStringList &messages)
{
    // 先清除现有消息
    clear();
    
    // 添加所有消息
    for (const QString &message : messages) {
        addMessage(message);
    }
}

void ChatMessageDisplay::scrollToBottom()
{
    // 确保滚动条滚动到底部
    QScrollBar *vScrollBar = verticalScrollBar();
    if (vScrollBar) {
        vScrollBar->setValue(vScrollBar->maximum());
    }
}

void ChatMessageDisplay::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    
    // 调整大小后滚动到底部，确保消息保持可见
    scrollToBottom();
}
