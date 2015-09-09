#ifndef LOGDISPLAY_H
#define LOGDISPLAY_H

#include <QDialog>
#include <QStringList>
#include <QSyntaxHighlighter>

namespace Ui {
    class CLogDisplay;
}

class CLogDisplay : public QDialog
{
    Q_OBJECT

public:
    explicit CLogDisplay();
    ~CLogDisplay();

public slots:
    void updateMessages();
    void logCtxMenu(const QPoint& pos);
    void addToAdblock();

private:
    Ui::CLogDisplay *ui;
    QStringList savedMessages;
    bool firstShow;
    QSyntaxHighlighter* syntax;

    void updateText(const QString& text);

protected:
    void showEvent(QShowEvent *);
};

#endif // LOGDISPLAY_H
