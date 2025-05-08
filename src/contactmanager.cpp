#include "contactmanager.h"
#include <QInputDialog>
#include <QWidget>

ContactManager::ContactManager(QObject *parent) : QObject(parent)
{
}

void ContactManager::showAddContactDialog(QWidget *parentWidget)
{
    bool ok;
    QString name = QInputDialog::getText(parentWidget, tr("Add Contact"),
                                         tr("Contact name:"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !name.isEmpty()) {
        emit contactAdded(name);
    }
}
