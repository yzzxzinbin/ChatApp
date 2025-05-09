#include "formattingtoolbarhandler.h"
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFontComboBox>
#include <QColorDialog>
#include <QApplication> // For QApplication::font()

FormattingToolbarHandler::FormattingToolbarHandler(
    QTextEdit *editor, QPushButton *boldBtn, QPushButton *italicBtn,
    QPushButton *underlineBtn, QPushButton *colorBtn, QPushButton *bgColorBtn,
    QComboBox *fontSizeCombo, QFontComboBox *fontFamilyCombo,
    QColor initialTextColor, QColor initialBgColor,
    QWidget *parentDialogHost, QObject *parent)
    : QObject(parent),
      messageInputEdit(editor),
      boldButton(boldBtn),
      italicButton(italicBtn),
      underlineButton(underlineBtn),
      colorButton(colorBtn),
      bgColorButton(bgColorBtn),
      fontSizeComboBox(fontSizeCombo),
      fontFamilyComboBox(fontFamilyCombo),
      dialogHost(parentDialogHost),
      currentTextColor_h(initialTextColor),
      currentBgColor_h(initialBgColor)
{
}

void FormattingToolbarHandler::onBoldButtonToggled(bool checked)
{
    if (!messageInputEdit) return;
    QTextCharFormat fmt;
    fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void FormattingToolbarHandler::onItalicButtonToggled(bool checked)
{
    if (!messageInputEdit) return;
    QTextCharFormat fmt;
    fmt.setFontItalic(checked);
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void FormattingToolbarHandler::onUnderlineButtonToggled(bool checked)
{
    if (!messageInputEdit) return;
    QTextCharFormat fmt;
    fmt.setFontUnderline(checked);
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void FormattingToolbarHandler::onColorButtonClicked()
{
    if (!messageInputEdit || !colorButton) return;
    QColor color = QColorDialog::getColor(currentTextColor_h, dialogHost, tr("Select Text Color"));
    if (color.isValid())
    {
        currentTextColor_h = color;
        QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(color.name());
        colorButton->setStyleSheet(colorStyle);

        QTextCharFormat fmt;
        fmt.setForeground(color);
        messageInputEdit->mergeCurrentCharFormat(fmt);
        messageInputEdit->setFocus();
        emit textColorChanged(color);
    }
}

void FormattingToolbarHandler::onBgColorButtonClicked()
{
    if (!messageInputEdit || !bgColorButton) return;
    QColor color = QColorDialog::getColor(currentBgColor_h.isValid() ? currentBgColor_h : Qt::white,
                                          dialogHost, tr("Select Background Color"),
                                          QColorDialog::ShowAlphaChannel);
    if (color.isValid())
    {
        currentBgColor_h = color;
        // Update bgColorButton's appearance (assuming it's styled like colorButton)
        // If it has a special icon for transparency, that needs to be handled.
        // For simplicity, using a solid background color for the button itself.
        QString bgColorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(color.name());
        if (color.alpha() == 0) { // If transparent, use the default checkerboard pattern from stylesheet
             bgColorButton->setStyleSheet(QString("border: 1px solid #cccccc;") +
                                         "background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAGElEQVQYlWNgYGD4z0AEMBGkwIGJiQEKABUQAQCQ1ARCQV5unQAAAABJRU5ErkJggg==);"
                                         "background-repeat: repeat;");
        } else {
            bgColorButton->setStyleSheet(bgColorStyle);
        }


        QTextCharFormat fmt;
        fmt.setBackground(color);
        messageInputEdit->mergeCurrentCharFormat(fmt);
        messageInputEdit->setFocus();
        emit backgroundColorChanged(color);
    }
}

void FormattingToolbarHandler::onFontSizeChanged(const QString &text)
{
    if (!messageInputEdit) return;
    bool ok;
    double pointSize = text.toDouble(&ok);
    if (ok && pointSize > 0)
    {
        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        messageInputEdit->mergeCurrentCharFormat(fmt);
        messageInputEdit->setFocus();
    }
}

void FormattingToolbarHandler::onFontFamilyChanged(const QFont &font)
{
    if (!messageInputEdit) return;
    QTextCharFormat fmt;
    fmt.setFontFamilies(font.families());
    messageInputEdit->mergeCurrentCharFormat(fmt);
    messageInputEdit->setFocus();
}

void FormattingToolbarHandler::updateFormatButtons(const QTextCharFormat &format)
{
    if (!boldButton || !italicButton || !underlineButton || !fontSizeComboBox || !fontFamilyComboBox || !colorButton || !bgColorButton) return;

    boldButton->blockSignals(true);
    boldButton->setChecked(format.fontWeight() >= QFont::Bold);
    boldButton->blockSignals(false);

    italicButton->blockSignals(true);
    italicButton->setChecked(format.fontItalic());
    italicButton->blockSignals(false);

    underlineButton->blockSignals(true);
    underlineButton->setChecked(format.fontUnderline());
    underlineButton->blockSignals(false);

    fontSizeComboBox->blockSignals(true);
    int pointSize = static_cast<int>(format.fontPointSize() + 0.5);
    if (pointSize <= 0 && format.font().pointSize() > 0)
        pointSize = format.font().pointSize();
    if (pointSize <= 0)
        pointSize = QApplication::font().pointSize(); // Default application font size
    if (pointSize <= 0)
        pointSize = 12; // Fallback

    QString sizeStr = QString::number(pointSize);
    int index = fontSizeComboBox->findText(sizeStr);
    if (index != -1)
    {
        fontSizeComboBox->setCurrentIndex(index);
    }
    else
    {
        // If size not in list, add it or set text directly if combo is editable
        // For now, just set current text, assuming it might not be in the list
        fontSizeComboBox->setCurrentText(sizeStr);
    }
    fontSizeComboBox->blockSignals(false);

    fontFamilyComboBox->blockSignals(true);
    QFont fontFromFormat = format.font();
    // Ensure families are not empty before setting, to avoid clearing the combo
    if (!fontFromFormat.families().isEmpty()) {
        fontFamilyComboBox->setCurrentFont(fontFromFormat);
    } else {
        // Fallback or default font if format.font() is not specific enough
        fontFamilyComboBox->setCurrentFont(QApplication::font());
    }
    fontFamilyComboBox->blockSignals(false);

    if (format.hasProperty(QTextFormat::ForegroundBrush))
    {
        currentTextColor_h = format.foreground().color();
        QString colorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentTextColor_h.name());
        colorButton->setStyleSheet(colorStyle);
        // emit textColorChanged(currentTextColor_h); // Avoid emitting in update, only on user action
    }

    if (format.hasProperty(QTextFormat::BackgroundBrush))
    {
        currentBgColor_h = format.background().color();
         QString bgColorStyle = QString("background-color: %1; border: 1px solid #cccccc;").arg(currentBgColor_h.name());
        if (currentBgColor_h.alpha() == 0) {
             bgColorButton->setStyleSheet(QString("border: 1px solid #cccccc;") +
                                         "background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAGElEQVQYlWNgYGD4z0AEMBGkwIGJiQEKABUQAQCQ1ARCQV5unQAAAABJRU5ErkJggg==);"
                                         "background-repeat: repeat;");
        } else {
            bgColorButton->setStyleSheet(bgColorStyle);
        }
        // emit backgroundColorChanged(currentBgColor_h); // Avoid emitting in update
    }
}
