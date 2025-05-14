#include "mainwindowstyle.h"

namespace StyleUtils {

QString getApplicationStyleSheet()
{
    return R"(
        QMainWindow {
            background-color: #f0f0f2;
        }

        #leftSidebar {
            background-color: #2c3e50;
            border-right: 1px solid #34495e;
        }

        #leftSidebar QPushButton {
            background-color: transparent;
            border: none;
            padding: 5px;
            border-radius: 4px;
        }
        #leftSidebar QPushButton:hover {
            background-color: #34495e;
        }
        #leftSidebar QPushButton:pressed {
            background-color: #1a252f;
        }

        /* 新的设置按钮将自动继承 #leftSidebar QPushButton 的样式 */

        #contactListWidget {
            background-color: #ffffff;
            border: 1px solid #dcdcdc;
            border-radius-top-right: 8px;
            border-radius-bottom-right: 8px;
            padding: 8px;
            font-size: 14px;
            margin-left: 0px;
            margin-top: 0px;
            margin-bottom: 0px;
            margin-right: 5px;
        }
        #contactListWidget::item {
            padding: 10px;
            border-radius: 4px;
        }
        #contactListWidget::item:selected {
            background-color: #0078d4;
            color: white;
        }
        #contactListWidget::item:selected:hover {
            background-color: #0078d4;
            color: white;
        }
        #contactListWidget::item:hover {
            background-color: #e6f2ff;
        }

        #peerInfoWidget { /* This will target the PeerInfoWidget instance */
            background-color: #e8e8e8; /* 淡灰色背景 */
            border-bottom: 1px solid #d0d0d0;
            /* Padding is handled by PeerInfoWidget's layout margins */
        }

        #peerInfoLabel { /* This will target QLabels with this objectName */
            font-size: 9pt;
            color: #333333;
        }

        #chatAreaWidget {
            background-color: #fdfdfd;
            border-radius: 8px;
        }

        #emptyChatPlaceholderLabel {
            font-size: 16px;
            color: #7f8c8d;
        }

        /* ChatMessageDisplay internal styling is handled by the component itself */
        /* Styles for #messageDisplay might be redundant if ChatMessageDisplay handles its own border/bg */
        /* However, if ChatMessageDisplay's objectName is "messageDisplay", this could still apply. */
        /* For now, assume ChatMessageDisplay handles its own core appearance. */
        /*
        #messageDisplay {
            background-color: #ffffff;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 10px;
            font-size: 14px;
            color: #333333;
        }
        */

        #formattingToolbarWidget {
            margin: 0px;
            padding: 0px;
        }

        #formattingToolbarWidget QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: 1px solid #cccccc;
            padding: 0px;
            font-size: 13px;
            border-radius: 4px;
            min-height: 30px;
            max-height: 30px;
            min-width: 30px;
            max-width: 30px;
            font-weight: bold;
        }
        
        #colorButton, #bgColorButton {
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            border-radius: 4px;
        }
        
        #bgColorButton {
            background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAGElEQVQYlWNgYGD4z0AEMBGkwIGJiQEKABUQAQCQ1ARCQV5unQAAAABJRU5ErkJggg==);
            background-repeat: repeat;
        }

        #formattingToolbarWidget QPushButton:checked {
            background-color: #cce5ff;
            border: 1px solid #0078d4;
        }
        #formattingToolbarWidget QPushButton:hover {
            background-color: #e0e0e0;
        }

        #formattingToolbarWidget QComboBox, 
        #formattingToolbarWidget QFontComboBox {
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 1px 5px 1px 5px;
            min-height: 28px;
            max-height: 28px;
            font-size: 13px;
            background-color: #f0f0f0;
            color: #333;
        }

        #formattingToolbarWidget QComboBox::drop-down,
        #formattingToolbarWidget QFontComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left-width: 1px;
            border-left-color: #cccccc;
            border-left-style: solid;
            border-top-right-radius: 3px;
            border-bottom-right-radius: 3px;
            background-color: #e0e0e0;
        }
        #formattingToolbarWidget QComboBox::down-arrow,
        #formattingToolbarWidget QFontComboBox::down-arrow {
            image: url(:/icons/dropdown_arrow.svg);
            width: 12px;
            height: 12px;
        }

        #formattingToolbarWidget QComboBox QAbstractItemView,
        #formattingToolbarWidget QFontComboBox QAbstractItemView {
            border: 1px solid #cccccc;
            background-color: white;
            selection-background-color: #0078d4;
            selection-color: white;
            padding: 2px;
        }

        /* Styles for the wrapper around message input and its clear button */
        #messageInputWrapper {
            background-color: #ffffff;
            border: 1px solid #d0d0d0; /* Border for the entire input area */
            border-radius: 6px;
            /* QHBoxLayout inside has margins 0, spacing 0 */
        }

        /* QTextEdit when inside the messageInputWrapper */
        #messageInputWrapper > QTextEdit#messageInputEdit {
            border: none; /* Remove individual border from QTextEdit */
            background-color: transparent; /* Make QTextEdit background transparent */
            padding: 8px; /* Internal padding for the text */
            font-size: 14px;
            color: #333333;
        }

        /* Clear button when inside the messageInputWrapper */
        #messageInputWrapper > QPushButton#clearMessageButton {
            border: none;
            background-color: transparent;
            color: #7f8c8d; /* Icon/Text color */
            font-weight: bold; /* If using text 'X' */
            /* Fixed size is set in code (22x22) */
            /* Icon size is set in code (12x12), it will be centered */
            padding: 0px; /* No internal padding for the button itself */
            margin: 0px 4px 0px 2px; /* top, right, bottom, left margins for the button */
                                     /* Adds a little space around the button */
            border-radius: 11px; /* Optional: if you want the button hover to be round */
        }
        #messageInputWrapper > QPushButton#clearMessageButton:hover {
            background-color: #f0f0f0; 
            color: #333333;
        }
        #messageInputWrapper > QPushButton#clearMessageButton:pressed {
            background-color: #e0e0e0;
        }

        #sendButton, #clearButton, #closeChatButton, #sendFileButton {
            background-color: #0078d4;
            color: white;
            border: none;
            padding: 4px 12px;
            font-size: 14px;
            border-radius: 5px;
        }
        #sendButton:hover, #clearButton:hover, #closeChatButton:hover, #sendFileButton:hover {
            background-color: #005a9e;
        }
        #sendButton:pressed, #clearButton:pressed, #closeChatButton:pressed, #sendFileButton:pressed {
            background-color: #004578;
        }
        #clearButton {
            background-color: #e74c3c;
        }
        #clearButton:hover {
            background-color: #c0392b;
        }
        #clearButton:pressed {
            background-color: #a93226;
        }
        #closeChatButton {
            background-color: #6c757d;
        }
        #closeChatButton:hover {
            background-color: #5a6268;
        }
        #closeChatButton:pressed {
            background-color: #545b62;
        }
    )";
}

} // namespace StyleUtils
