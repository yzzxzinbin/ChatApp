#ifndef CHATMESSAGEDISPLAY_H
#define CHATMESSAGEDISPLAY_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QList>

class ChatMessageDisplay : public QScrollArea
{
    Q_OBJECT

public:
    explicit ChatMessageDisplay(QWidget *parent = nullptr);

    // 添加消息的方法
    void addMessage(const QString &html);
    
    // 清除所有消息
    void clear();
    
    // 设置消息列表
    void setMessages(const QStringList &messages);

private slots: 
    void updateContentMargins();
    void scrollToBottom(); // 将 scrollToBottom 移到槽

private:
    QWidget *contentWidget;      // 消息内容的容器
    QVBoxLayout *contentLayout;  // 垂直布局管理器
    QWidget *spacerWidget;       // 顶部弹性空间
    
    int m_originalRightMargin;   // 新增：存储原始右边距
    int m_scrollBarWidth;        // 新增：存储滚动条宽度
    
    // 重写调整大小事件，确保滚动条位置正确
    void resizeEvent(QResizeEvent *event) override;
};

#endif // CHATMESSAGEDISPLAY_H
