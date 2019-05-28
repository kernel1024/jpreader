#ifndef AUTHDLG_H
#define AUTHDLG_H

#include <QDialog>
#include <QUrl>
#include <QString>

namespace Ui {
    class AuthDlg;
}

class CAuthDlg : public QDialog
{
    Q_OBJECT
    
public:
    explicit CAuthDlg(QWidget *parent = nullptr,
                      const QUrl & origin = QUrl(),
                      const QString & realm = QString(),
                      bool autofillLogin = false);
    ~CAuthDlg() override;
    QString getUser();
    QString getPassword();
    
private:
    Ui::AuthDlg *ui;
    QUrl u_origin;
    bool m_autofillLogin { false };

    Q_DISABLE_COPY(CAuthDlg)

public slots:
    void acceptPass();
};

#endif // AUTHDLG_H
