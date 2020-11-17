#ifndef SOURCEVIEWER_H
#define SOURCEVIEWER_H

#include <QWidget>
#include "browser/browser.h"

namespace Ui {
class CSourceViewer;
}

class CSourceViewer : public QWidget
{
    Q_OBJECT

public:
    explicit CSourceViewer(CBrowserTab *origin, QWidget *parent = nullptr);
    ~CSourceViewer() override;

private:
    Ui::CSourceViewer *ui;

    Q_DISABLE_COPY(CSourceViewer)

    void updateSource(const QString& src);
    QString reformatSource(const QString &html);
};

#endif // SOURCEVIEWER_H
