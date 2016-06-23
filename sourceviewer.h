#ifndef SOURCEVIEWER_H
#define SOURCEVIEWER_H

#include <QDialog>
#include "snviewer.h"

namespace Ui {
class CSourceViewer;
}

class CSourceViewer : public QDialog
{
    Q_OBJECT

public:
    explicit CSourceViewer(CSnippetViewer *origin);
    ~CSourceViewer();

private:
    Ui::CSourceViewer *ui;

    void updateSource(const QString& src);
    QString reformatSource(const QString &html);
};

#endif // SOURCEVIEWER_H
