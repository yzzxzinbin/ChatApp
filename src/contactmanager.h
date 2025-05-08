#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <QObject>
#include <QString>

class QWidget;

class ContactManager : public QObject
{
    Q_OBJECT
public:
    explicit ContactManager(QObject *parent = nullptr);

    void showAddContactDialog(QWidget *parentWidget);

signals:
    void contactAdded(const QString &name);
};

#endif // CONTACTMANAGER_H
