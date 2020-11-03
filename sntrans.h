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
    QString m_storedSelection;
    QUrl m_savedBaseUrl;
    void findWordTranslation(const QString& text);
    void openSeparateTranslationTab(const QString& html, const QUrl &baseUrl);
    void applyScripts();

Q_SIGNALS:
    void scriptFinished();

public:
    explicit CSnTrans(CSnippetViewer * parent);

public Q_SLOTS:
    void translatePriv(const QString& sourceHtml, const QString &title, const QUrl &origin);
    void translationFinished(bool success, bool aborted, const QString &resultHtml, const QString &error);
    void postTranslate();
    void selectionChanged();
    void selectionShow();
    void showWordTranslation(const QString & html);
    void showSuggestedTranslation(const QString & link);
    void reparseDocument();
    void translateDocument();
    void reparseDocumentPriv(const QString& data);
    void getImgUrlsAndParse();
};

#endif // SNTRANS_H
