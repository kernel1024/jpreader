#ifndef SNTRANS_H
#define SNTRANS_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTimer>

class CSnippetViewer;

class CSnTrans : public QObject
{
    Q_OBJECT
private:
    CSnippetViewer *snv;
    QTimer m_selectionTimer;
    QTimer m_longClickTimer;
    QString m_storedSelection;
    QUrl m_savedBaseUrl;
    void findWordTranslation(const QString& text);
    void openSeparateTranslationTab(const QString& html, const QUrl &baseUrl);

public:
    explicit CSnTrans(CSnippetViewer * parent);
    void translate(bool forceTranslateSubSentences);

public Q_SLOTS:
    void translatePriv(const QString& sourceHtml, bool forceTranslateSubSentences);
    void translationFinished(bool success, const QString &resultHtml, const QString &error);
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
    void getImgUrlsAndParse();
};

#endif // SNTRANS_H
