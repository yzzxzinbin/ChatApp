#include "chatmessagedisplay.h"
#include <QResizeEvent>
#include <QScrollBar>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QRegularExpression> // 新增

ChatMessageDisplay::ChatMessageDisplay(QWidget *parent)
    : QScrollArea(parent), m_lastDisplayedTimestampValue("") // 初始化
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
    m_originalRightMargin = 0; // 初始化原始右边距
    contentLayout->setContentsMargins(10, 10, m_originalRightMargin, 10);
    
    // 创建顶部空白区域，使消息保持在底部
    spacerWidget = new QWidget(contentWidget);
    spacerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    contentLayout->addWidget(spacerWidget, 1);
    
    // 设置内容组件
    setWidget(contentWidget);

    m_scrollBarWidth = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    
    // 调整滚动条宽度和样式
    int desiredScrollBarWidth = 8; // 设置期望的滚动条宽度，例如 8px
    QString scrollBarStyle = QString(
        "QScrollBar:vertical {"
        "    border: none;" // 可选：移除边框
        "    background: transparent;" // 可选：设置滚动条背景透明
        "    width: %1px;"
        "    margin: 0px 0px 0px 0px;" // 可选：调整边距
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #c0c0c0;" // 设置滑块的背景颜色，使其可见
        "    min-height: 20px;"    // 可选：设置滑块的最小高度
        "    border-radius: %3px;" // 可选：为滑块添加圆角
        "}"
        "QScrollBar::add-line:vertical {" // 可选：隐藏上下箭头
        "    height: 0px;"
        "    subcontrol-position: bottom;"
        "    subcontrol-origin: margin;"
        "}"
        "QScrollBar::sub-line:vertical {" // 可选：隐藏上下箭头
        "    height: 0px;"
        "    subcontrol-position: top;"
        "    subcontrol-origin: margin;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {" // 滑块前后的区域
        "    background: none;"
        "}"
    ).arg(desiredScrollBarWidth).arg(desiredScrollBarWidth / 2); // 圆角为宽度的一半

    verticalScrollBar()->setStyleSheet(scrollBarStyle);
    m_scrollBarWidth = desiredScrollBarWidth; 

    if (m_scrollBarWidth <= 0) { 
        m_scrollBarWidth = 16; 
    }
    
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

    QTimer::singleShot(0, this, &ChatMessageDisplay::updateContentMargins);
}

// 新增：实现提取时间戳的辅助方法
QString ChatMessageDisplay::extractTimestampValueFromHtml(const QString& timestampHtml) const {
    QRegularExpression re("<div style=\"text-align: center; margin-bottom: 5px;\"><span style=\"background-color: #bbbbbb; color: white; padding: 2px 8px; border-radius: 10px; font-size: 9pt;\">(\\d{2}:\\d{2})</span></div>");
    QRegularExpressionMatch match = re.match(timestampHtml);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

void ChatMessageDisplay::updateContentMargins()
{
    if (!contentLayout || !verticalScrollBar()) return;

    bool scrollBarIsActuallyVisible = verticalScrollBar()->isVisible();

    int currentLeft, currentTop, currentRight, currentBottom;
    contentLayout->getContentsMargins(&currentLeft, &currentTop, &currentRight, &currentBottom);

    int targetRightMargin = m_originalRightMargin;
    if (!scrollBarIsActuallyVisible) {
        targetRightMargin = m_originalRightMargin + m_scrollBarWidth;
    }

    if (currentRight != targetRightMargin) {
        contentLayout->setContentsMargins(currentLeft, currentTop, targetRightMargin, currentBottom);
    }
}

void ChatMessageDisplay::addMessage(const QString &html)
{
    QString extractedTime = extractTimestampValueFromHtml(html);

    if (!extractedTime.isEmpty()) { // 这是一个时间戳条目
        if (extractedTime != m_lastDisplayedTimestampValue) {
            // 时间戳不同，或者之前没有时间戳，显示它
            QLabel *messageLabel = new QLabel(html, contentWidget);
            messageLabel->setTextFormat(Qt::RichText);
            messageLabel->setWordWrap(true);
            messageLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
            messageLabel->setOpenExternalLinks(false);
            messageLabel->setMargin(2);
            messageLabel->setMinimumWidth(100);
            contentLayout->addWidget(messageLabel);
            m_lastDisplayedTimestampValue = extractedTime; // 更新最后显示的时间戳
        } else {
            // 时间戳与上一个相同，不显示
            return;
        }
    } else { // 这是一个普通消息条目
        QLabel *messageLabel = new QLabel(html, contentWidget);
        messageLabel->setTextFormat(Qt::RichText);
        messageLabel->setWordWrap(true);
        messageLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        messageLabel->setOpenExternalLinks(false);
        messageLabel->setMargin(2);
        messageLabel->setMinimumWidth(100);
        contentLayout->addWidget(messageLabel);
    }
    
    // 延迟调用以更新边距并滚动到底部
    QTimer::singleShot(0, this, [this]() {
        this->updateContentMargins();
        QTimer::singleShot(0, this, [this]() {
            this->scrollToBottom();       
        });
    });
}

void ChatMessageDisplay::clear()
{
    m_lastDisplayedTimestampValue.clear(); // 重置最后显示的时间戳
    // 保留spacer，删除所有其他组件
    QLayoutItem *item;
    while ((item = contentLayout->takeAt(1)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    QTimer::singleShot(0, this, &ChatMessageDisplay::updateContentMargins);
}

void ChatMessageDisplay::setMessages(const QStringList &messages)
{
    // 先清除现有消息 (clear() 会重置 m_lastDisplayedTimestampValue)
    clear();
    
    // 添加所有消息 (addMessage 内部会处理时间戳过滤)
    for (const QString &message : messages) {
        addMessage(message); // addMessage 现在包含过滤逻辑
    }
    if (messages.isEmpty()) {
        QTimer::singleShot(0, this, &ChatMessageDisplay::updateContentMargins);
    }
    // scrollToBottom 会在最后一个 addMessage 调用中被触发
}

void ChatMessageDisplay::scrollToBottom()
{
    // 尝试滚动到最后一个实际的消息控件
    if (contentLayout && contentLayout->count() > 0) {
        // contentLayout 的第一个子项是 spacerWidget (假设它总是在那里且不被删除)
        // 因此，如果 count > 1，则表示至少有一个消息标签
        if (contentLayout->count() > 1) { // 确保至少有一个消息标签
            QLayoutItem* lastItem = contentLayout->itemAt(contentLayout->count() - 1);
            if (lastItem) {
                QWidget* lastWidget = lastItem->widget();
                if (lastWidget && lastWidget != spacerWidget) { // 确保它不是 spacer
                    this->ensureWidgetVisible(lastWidget, 0, 0); // yMargin 0 使其底部与视口底部对齐
                    return; // 成功滚动到最后一个消息
                }
            }
        } else if (contentLayout->count() == 1) {
            // 只有一个子项，检查它是否是 spacer。如果是，则内容为空。
            // 如果不是 spacer (不太可能，但作为健壮性检查)，则尝试滚动到它。
            QLayoutItem* singleItem = contentLayout->itemAt(0);
            if (singleItem) {
                QWidget* widget = singleItem->widget();
                if (widget && widget != spacerWidget) {
                     this->ensureWidgetVisible(widget, 0, 0);
                     return;
                }
            }
        }
    }

    // 如果上述方法失败（例如，布局为空或只有spacer），则回退到基于滚动条最大值的方法
    QScrollBar *vScrollBar = verticalScrollBar();
    if (vScrollBar) {
        vScrollBar->setValue(vScrollBar->maximum());
    }
}

void ChatMessageDisplay::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    
    // 调整大小后，延迟更新边距并滚动到底部
    QTimer::singleShot(0, this, [this]() { // 第一个定时器
        this->updateContentMargins(); // 首先更新边距
        
        QTimer::singleShot(0, this, [this]() { // 第二个定时器
            this->scrollToBottom();       // 然后滚动
        });
    });
}
