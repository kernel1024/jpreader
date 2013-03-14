#ifndef WAITDLG_H
#define WAITDLG_H

#include <QWidget>
#include <QPushButton>
#include <QString>

namespace Ui {
    class WaitDlg;
}

class CWaitDlg : public QWidget
{
    Q_OBJECT

public:
    QPushButton* abortBtn;
    explicit CWaitDlg(QWidget *parent = 0, QString aMsg = QString("Please wait..."));
    virtual ~CWaitDlg();
    void setText(QString aMsg);
public slots:
    void setProgressEnabled(bool show);
    void setProgressValue(int val);

private:
    Ui::WaitDlg *ui;
};

#endif // WAITDLG_H
