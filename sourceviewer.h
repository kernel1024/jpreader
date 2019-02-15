#ifndef SOURCEVIEWER_H
#define SOURCEVIEWER_H

#include <QWidget>
#include "snviewer.h"

namespace Ui {
class CSourceViewer;
}

class CSourceViewer : public QWidget
{
    Q_OBJECT

public:
    explicit CSourceViewer(CSnippetViewer *origin, QWidget *parent = nullptr);
    ~CSourceViewer();

private:
    Ui::CSourceViewer *ui;

    void updateSource(const QString& src);
    QString reformatSource(const QString &html);
};

#endif // SOURCEVIEWER_H
