#ifndef SNTRANS_H
#define SNTRANS_H

#include <QObject>
#include <QString>
#include "snviewer.h"
#include "dictionary_interface.h"

class CSnTrans : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    QTimer *selectionTimer;
    QString storedSelection;
    OrgQjradDictionaryInterface* dbusDict;
public:
    CSnTrans(CSnippetViewer * parent);
public slots:
    void translate();
    void calcFinished(const bool success, const QString &aUrl);
    void postTranslate();
    void progressLoad(int progress);
    void selectionChanged();
    void selectionShow();
    void hideTooltip();
    void showWordTranslation(const QString & html);
    void showDictionaryWindow();
};

#endif // SNTRANS_H
