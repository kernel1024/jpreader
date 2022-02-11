#ifndef BROWSER_H
#define BROWSER_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QList>
#include <QUrl>
#include <QKeyEvent>
#include <QTimer>
#include <QWebEngineView>

#include "global/structures.h"
#include "utils/specwidgets.h"
#include "ui_browser.h"

class CBrowserCtxHandler;
class CBrowserMsgHandler;
class CBrowserNet;
class CBrowserTrans;
class CBrowserWaitCtl;

namespace CDefaults {
const int maxDataUrlFileSize = 256*1024;
}

class CBrowserTab : public CSpecTabContainer, public Ui::BrowserTab
{
    Q_OBJECT

    friend class CBrowserCtxHandler;
    friend class CBrowserMsgHandler;
    friend class CBrowserNet;
    friend class CBrowserTrans;
    friend class CBrowserWaitCtl;

private:
    CBrowserCtxHandler *m_ctxHandler;
    CBrowserMsgHandler *m_msgHandler;
    CBrowserNet *m_netHandler;
    CBrowserTrans *m_transHandler;
    CBrowserWaitCtl *m_waitHandler;

    QPixmap m_pageImage;
    QStringList m_searchList;
    QString m_translatedHtml;
    bool m_startPage;
    bool m_translationBkgdFinished { false };
    bool m_loadingBkgdFinished { false };
    bool m_onceTranslated { false };
    bool m_loading { false };
    bool m_requestAutotranslate { false };
    bool m_requestAlternateAutotranslate { false };
    bool m_pageLoaded { false };
    bool m_auxContentLoaded { false };

    void updateTabColor(bool loadFinished = false, bool tranFinished = false);

public:
    explicit CBrowserTab(QWidget* parent, const QUrl& aUri = QUrl(),
                         const QStringList& aSearchText = QStringList(),
                         const QString& AuxContent = QString(),
                         bool setFocused = true, bool startPage = false, bool autoTranslate = false,
                         bool alternateAutoTranslate = false, bool onceTranslated = false);
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
    void sendInputToBrowser(const QString &text);
    void tabAcquiresFocus() override;
    CBrowserNet *netHandler() const;

public Q_SLOTS:
    void navByUrlDefault();
    void navByUrl(const QUrl &url);
    void navByUrl(const QString &url);
    void titleChanged(const QString & title);
    void urlChanged(const QUrl & url);
    void statusBarMsg(const QString & msg);
    void takeScreenshot();
    void save();
};

#endif
