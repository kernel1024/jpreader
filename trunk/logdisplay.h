#ifndef LOGDISPLAY_H
#define LOGDISPLAY_H

#include <QDialog>
#include <QStringList>

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

private:
    Ui::CLogDisplay *ui;
    QStringList savedMessages;
    bool firstShow;

    // QWidget interface
protected:
    void showEvent(QShowEvent *);
};

#endif // LOGDISPLAY_H
