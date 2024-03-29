#ifndef SNTRANS_H
#define SNTRANS_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTimer>

class CBrowserTab;

class CBrowserTrans : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CBrowserTrans)
private:
    CBrowserTab *snv;
    QTimer m_selectionTimer;
    QString m_storedSelection;
    QUrl m_savedBaseUrl;
    void findWordTranslation(const QString& text);
    void applyScripts();
    QString computeFileName(const QUrl &url) const;

Q_SIGNALS:
    void scriptFinished();

public:
    enum UrlsExtractorMode {
        uemImages = 1,
        uemAllFiles = 2
    };
    explicit CBrowserTrans(CBrowserTab * parent);
    ~CBrowserTrans() override = default;

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
    void getUrlsFromPageAndParse();
};

#endif // SNTRANS_H
