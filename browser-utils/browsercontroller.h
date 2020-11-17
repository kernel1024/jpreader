#ifndef BROWSERCONTROLLER_H
#define BROWSERCONTROLLER_H

#include <QObject>
#include <QString>
#include <QUrl>

class CBrowserController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kernel1024.jpreader.browsercontroller")
public:
    explicit CBrowserController(QObject *parent = nullptr);

public Q_SLOTS:
    void openUrl(const QString& url);
    void openDefaultSearch(const QString& text);

};

#endif // BROWSERCONTROLLER_H
