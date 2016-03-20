#ifndef SNTRANS_H
#define SNTRANS_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTimer>
#include "snviewer.h"

class CSnTrans : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    QTimer *selectionTimer;
    QTimer *longClickTimer;
    QString storedSelection;
    QUrl savedBaseUrl;
    void findWordTranslation(const QString& text);
public:
    CSnTrans(CSnippetViewer * parent);
public slots:
    void translate();
    void translatePriv(const QString& aUri, bool forceTranSubSentences);
    void calcFinished(const bool success, const QString &aUrl);
    void postTranslate();
    void progressLoad(int progress);
    void selectionChanged();
    void selectionShow();
    void showWordTranslation(const QString & html);
    void showSuggestedTranslation(const QString & link);
    void dictDataReady();
    void reparseDocument();
    void reparseDocumentPriv(const QString& data);
    void transButtonHighlight();
};

#endif // SNTRANS_H
