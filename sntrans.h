#ifndef SNTRANS_H
#define SNTRANS_H

#include <QObject>
#include <QString>
#include <QUrl>
#include "snviewer.h"

class CSnTrans : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    QTimer *selectionTimer;
    QString storedSelection;
    QUrl savedBaseUrl;
    void findWordTranslation(const QString& text);
public:
    CSnTrans(CSnippetViewer * parent);
public slots:
    void translate();
    void translatePriv(const QString& aUri);
    void calcFinished(const bool success, const QString &aUrl);
    void postTranslate();
    void progressLoad(int progress);
    void selectionChanged();
    void selectionShow();
    void hideTooltip();
    void showWordTranslation(const QString & html);
    void showSuggestedTranslation(const QString & link);
    void dictDataReady();
    void convertToXML();
    void convertToXMLPriv(const QString& data);
};

#endif // SNTRANS_H
