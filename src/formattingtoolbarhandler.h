#ifndef FORMATTINGTOOLBARHANDLER_H
#define FORMATTINGTOOLBARHANDLER_H

#include <QObject>
#include <QTextCharFormat>
#include <QColor>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QPushButton;
class QComboBox;
class QFontComboBox;
class QWidget;
QT_END_NAMESPACE

class FormattingToolbarHandler : public QObject
{
    Q_OBJECT

public:
    explicit FormattingToolbarHandler(
        QTextEdit *editor,
        QPushButton *boldBtn,
        QPushButton *italicBtn,
        QPushButton *underlineBtn,
        QPushButton *colorBtn,
        QPushButton *bgColorBtn,
        QComboBox *fontSizeCombo,
        QFontComboBox *fontFamilyCombo,
        QColor initialTextColor,     // Pass initial color values
        QColor initialBgColor,       // Pass initial background color values
        QWidget *parentDialogHost,   // For QColorDialog parent
        QObject *parent = nullptr);

signals:
    void textColorChanged(const QColor &color);
    void backgroundColorChanged(const QColor &color);

public slots:
    // Slots for button toggles and combo box changes
    void onBoldButtonToggled(bool checked);
    void onItalicButtonToggled(bool checked);
    void onUnderlineButtonToggled(bool checked);
    void onColorButtonClicked();
    void onBgColorButtonClicked();
    void onFontSizeChanged(const QString &text);
    void onFontFamilyChanged(const QFont &font);

    // Slot to update button states based on cursor's format
    void updateFormatButtons(const QTextCharFormat &format);

private:
    QTextEdit *messageInputEdit;
    QPushButton *boldButton;
    QPushButton *italicButton;
    QPushButton *underlineButton;
    QPushButton *colorButton;
    QPushButton *bgColorButton;
    QComboBox *fontSizeComboBox;
    QFontComboBox *fontFamilyComboBox;
    QWidget *dialogHost; // Parent for QColorDialog

    QColor currentTextColor_h; // Handler's copy for UI updates
    QColor currentBgColor_h;   // Handler's copy for UI updates
};

#endif // FORMATTINGTOOLBARHANDLER_H
