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
    explicit CLogDisplay(QWidget *parent = nullptr);
    ~CLogDisplay() override;

public Q_SLOTS:
    void updateMessages();
    void logCtxMenu(const QPoint& pos);
    void addToAdblock();

private:
    Ui::CLogDisplay *ui;
    QSyntaxHighlighter* syntax;
    bool firstShow;

    Q_DISABLE_COPY(CLogDisplay)

protected:
    void showEvent(QShowEvent * event) override;

};

#endif // LOGDISPLAY_H
