#ifndef BOOKMARKDLG_H
#define BOOKMARKDLG_H

#include <QDialog>

namespace Ui {
    class BookmarkDlg;
}

class CBookmarkDlg : public QDialog
{
    Q_OBJECT
    
public:
    explicit CBookmarkDlg(QWidget *parent = 0, const QString &name = QString(), const QString &url = QString());
    ~CBookmarkDlg();
    QString getBkTitle();
    QString getBkUrl();
    
private:
    Ui::BookmarkDlg *ui;
};

#endif // BOOKMARKDLG_H
