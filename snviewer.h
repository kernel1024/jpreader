#ifndef SNVIEWER_H
#define SNVIEWER_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QList>
#include <QUrl>
#include <QKeyEvent>
#include <QTimer>
#include <QWebEngineView>

#include "specwidgets.h"
#include "ui_snviewer.h"

class CSnCtxHandler;
class CSnMsgHandler;
class CSnNet;
class CSnTrans;
class CSnWaitCtl;

namespace CDefaults {
const int maxDataUrlFileSize = 1024*1024;
}

class CSnippetViewer : public CSpecTabContainer, public Ui::SnippetViewer
{
	Q_OBJECT

    friend class CSnCtxHandler;
    friend class CSnMsgHandler;
    friend class CSnNet;
    friend class CSnTrans;
    friend class CSnWaitCtl;

private:
    CSnCtxHandler *ctxHandler;
    CSnMsgHandler *msgHandler;
    CSnNet *netHandler;
    CSnTrans *transHandler;
    CSnWaitCtl *waitHandler;

    QPixmap m_pageImage;
    QStringList m_searchList;
    QString m_translatedHtml;
    bool m_startPage;
    bool m_fileChanged { false };
    bool m_translationBkgdFinished { false };
    bool m_loadingBkgdFinished { false };
    bool m_onceTranslated { false };
    bool m_loading { false };
    bool m_requestAutotranslate { false };
    bool m_pageLoaded { false };
    bool m_auxContentLoaded { false };

    void updateTabColor(bool loadFinished = false, bool tranFinished = false);

public:
    explicit CSnippetViewer(QWidget* parent, const QUrl& aUri = QUrl(),
                            const QStringList& aSearchText = QStringList(),
                            bool setFocused = true, const QString& AuxContent = QString(),
                            const QString& zoom = QStringLiteral("100%"),
                            bool startPage = false);
    virtual QString getDocTitle();
    void keyPressEvent(QKeyEvent* event) override;
    void updateButtonsState();
    void updateWebViewAttributes();
    bool canClose() override;
    void recycleTab() override;
    QUrl getUrl();
    void setToolbarVisibility(bool visible) override;
    void outsideDragStart() override;
    void printToPDF();
    bool isStartPage() const { return m_startPage; }
    QPixmap pageImage() const { return m_pageImage; }
    bool isTranslationBkgdFinished() const { return m_translationBkgdFinished; }
    bool isLoadingBkgdFinished() const { return m_loadingBkgdFinished; }

public Q_SLOTS:
    void navByUrlDefault();
    void navByUrl(const QUrl &url);
    void navByUrl(const QString &url);
    void titleChanged(const QString & title);
    void urlChanged(const QUrl & url);
    void statusBarMsg(const QString & msg);
    void takeScreenshot();
    void save();
    void tabAcquiresFocus();
};

#endif
