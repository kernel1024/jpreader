#ifndef TRANSLATORCACHEDIALOG_H
#define TRANSLATORCACHEDIALOG_H

#include <QDialog>
#include <QTableWidget>

namespace Ui {
    class CTranslatorCacheDialog;
}

class CTranslatorCacheDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CTranslatorCacheDialog(QWidget *parent = nullptr);
    ~CTranslatorCacheDialog() override;

    void updateCacheList();

private:
    Ui::CTranslatorCacheDialog *ui;

    Q_DISABLE_COPY(CTranslatorCacheDialog)

public Q_SLOTS:
    void tableItemActivated(QTableWidgetItem *item);

};

#endif // TRANSLATORCACHEDIALOG_H
