#ifndef AUTHDLG_H
#define AUTHDLG_H

#include <QtCore>
#include <QtGui>

namespace Ui {
    class AuthDlg;
}

class CAuthDlg : public QDialog
{
    Q_OBJECT
    
public:
    explicit CAuthDlg(QWidget *parent = 0, const QUrl & origin = QUrl(), const QString & realm = QString());
    ~CAuthDlg();
    QString getUser();
    QString getPassword();
    
private:
    Ui::AuthDlg *ui;
    QUrl u_origin;

public slots:
    void acceptPass();
};

#endif // AUTHDLG_H
