#include <QtMath>
#include <QMessageBox>
#include "browser/browser.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/startup.h"
#include "extractors/abstractextractor.h"
#include "mainwindow.h"
#include "mangaviewtab.h"
#include "ui_mangaviewtab.h"

CMangaViewTab::CMangaViewTab(CMainWindow *parent, bool setFocused) :
    CSpecTabContainer(parent),
    ui(new Ui::CMangaViewTab)
{
    ui->setupUi(this);

    ui->spinPosition->hide();
    rotationUpdated(0.0);
    ui->frameLoading->hide();

    m_zoomGroup = new QButtonGroup(this);
    m_zoomGroup->addButton(ui->btnZoomFit, ZMangaView::zmFit);
    m_zoomGroup->addButton(ui->btnZoomWidth, ZMangaView::zmWidth);
    m_zoomGroup->addButton(ui->btnZoomHeight, ZMangaView::zmHeight);
    m_zoomGroup->addButton(ui->btnZoomOriginal, ZMangaView::zmOriginal);
    m_zoomGroup->setExclusive(true);

    connect(ui->btnRotateCCW, &QPushButton::clicked, ui->mangaView, &ZMangaView::viewRotateCCW);
    connect(ui->btnRotateCW, &QPushButton::clicked, ui->mangaView, &ZMangaView::viewRotateCW);
    connect(ui->btnNavFirst, &QPushButton::clicked, ui->mangaView, &ZMangaView::navFirst);
    connect(ui->btnNavPrev, &QPushButton::clicked, ui->mangaView, &ZMangaView::navPrev);
    connect(ui->btnNavNext, &QPushButton::clicked, ui->mangaView, &ZMangaView::navNext);
    connect(ui->btnNavLast, &QPushButton::clicked, ui->mangaView, &ZMangaView::navLast);
    connect(ui->btnAbortLoading, &QPushButton::clicked, ui->mangaView, &ZMangaView::abortLoading);
    connect(ui->btnAbortLoading, &QPushButton::clicked, this, &CMangaViewTab::loadingAborted);
    connect(ui->btnZoomDynamic, &QPushButton::toggled, ui->mangaView, &ZMangaView::setZoomDynamic);
    connect(ui->btnSave, &QPushButton::clicked, this, &CMangaViewTab::save);
    connect(ui->btnOpenOrigin, &QPushButton::clicked, this, &CMangaViewTab::openOrigin);

    connect(m_zoomGroup,&QButtonGroup::idClicked,this,[this](int mode){
        ui->comboZoom->setEnabled(mode==ZMangaView::zmOriginal);
        ui->mangaView->setZoomMode(mode);
    });

    connect(ui->scrollArea,&ZScrollArea::sizeChanged,ui->mangaView,&ZMangaView::ownerResized);
    connect(ui->comboZoom,&QComboBox::currentIndexChanged,ui->mangaView,&ZMangaView::setZoomAny);
    connect(ui->spinPosition,&QSpinBox::editingFinished,this,&CMangaViewTab::pageNumEdited);

    connect(ui->mangaView,&ZMangaView::loadedPage,this,&CMangaViewTab::pageLoaded);
    connect(ui->mangaView,&ZMangaView::auxMessage,this,&CMangaViewTab::auxMessage);
    connect(ui->mangaView,&ZMangaView::rotationUpdated,this,&CMangaViewTab::rotationUpdated);
    connect(ui->mangaView,&ZMangaView::loadingStarted,this,&CMangaViewTab::loadingStarted);
    connect(ui->mangaView,&ZMangaView::loadingFinished,this,&CMangaViewTab::loadingFinished);
    connect(ui->mangaView,&ZMangaView::loadingProgress,this,&CMangaViewTab::loadingProgress);
    connect(ui->mangaView,&ZMangaView::loadingProgressSize,this,&CMangaViewTab::loadingProgressSize);
    connect(ui->mangaView,&ZMangaView::exportStarted,this,&CMangaViewTab::exportStarted);
    connect(ui->mangaView,&ZMangaView::exportFinished,this,&CMangaViewTab::exportFinished);
    connect(ui->mangaView,&ZMangaView::exportProgress,this,&CMangaViewTab::exportProgress);
    connect(ui->mangaView,&ZMangaView::exportError,this,&CMangaViewTab::exportError);

    ui->mangaView->setScroller(ui->scrollArea);
    pageLoaded(-1);
    ui->btnZoomFit->click();
    ui->labelStatus->setText(tr("Ready"));

    bindToTab(parentWnd()->tabMain,setFocused);
}

CMangaViewTab::~CMangaViewTab()
{
    delete ui;
}

void CMangaViewTab::loadMangaPages(const QVector<CUrlWithName> &pages, const QString &title, const QString &description,
                                   const QUrl &referer, bool isFanbox, bool originalScale)
{
    ui->mangaView->loadMangaPages(pages,title,referer,isFanbox);
    ui->toolbar->setToolTip(description);
    m_mangaTitle = title;
    m_mangaDescription = description;
    m_origin = referer;
    m_originalScale = originalScale;
    updateTabTitle();
}

void CMangaViewTab::updateTabTitle()
{
    QString title = tr("%1 - artwork viewer").arg(mangaTitle());
    if (m_aborted)
        title.append(tr(" (Aborted)"));
    setTabTitle(title);
}

void CMangaViewTab::updateStatusLabel(const QString &msg)
{
    QString text = msg;
    const QString aborted = tr("<b>Aborted</b>");
    if (m_aborted) {
        if (text.isEmpty()) {
            text.append(aborted);
        } else {
            text.append(QSL(", %1").arg(aborted));
        }
    }
    ui->labelStatus->setText(text);
}

void CMangaViewTab::pageNumEdited()
{
    if (ui->spinPosition->value()!=ui->mangaView->getCurrentPage())
        ui->mangaView->setPage(ui->spinPosition->value()-1);
}

void CMangaViewTab::pageLoaded(int num)
{
    if (num<0 || !(ui->mangaView->isMangaOpened())) {
        if (ui->spinPosition->isVisible()) {
            ui->spinPosition->hide();
        }
        ui->lblPosition->clear();
    } else {
        if (!ui->spinPosition->isVisible()) {
            ui->spinPosition->show();
            ui->spinPosition->setMinimum(1);
            ui->spinPosition->setMaximum(ui->mangaView->getPageCount());
        }
        ui->spinPosition->setValue(num+1);
        ui->lblPosition->setText(tr("  /  %1").arg(ui->mangaView->getPageCount()));
    }
}

void CMangaViewTab::rotationUpdated(double angle)
{
    auxMessage(tr("Rotation: %1").arg(qRadiansToDegrees(angle),1,'f',1));
}

void CMangaViewTab::auxMessage(const QString &msg)
{
    CMainWindow* wnd = gSet->activeWindow();
    if (wnd)
        wnd->displayStatusBarMessage(msg);
}

void CMangaViewTab::loadingStarted()
{
    ui->frameLoading->show();
    ui->labelLoadInfo->clear();
    ui->loadingProgress->setValue(0);
    ui->btnAbortLoading->setEnabled(true);
    updateStatusLabel(tr("Loading"));
}

void CMangaViewTab::loadingFinished()
{
    ui->frameLoading->hide();
    if (!m_aborted)
        updateStatusLabel(tr("Ok"));
    updateTabColor(true);
}

void CMangaViewTab::loadingProgress(int value)
{
    ui->loadingProgress->setValue(value);
}

void CMangaViewTab::loadingProgressSize(qint64 size)
{
    ui->labelLoadInfo->setText(tr("Loaded: %1").arg(CGenericFuncs::formatFileSize(size)));
}

void CMangaViewTab::loadingAborted()
{
    m_aborted = true;
    updateTabTitle();
    updateStatusLabel(QString());
}

void CMangaViewTab::exportStarted()
{
    m_exportErrors.clear();
    ui->frameLoading->show();
    ui->labelLoadInfo->setText(tr("Exporting"));
    ui->loadingProgress->setValue(0);
    ui->btnAbortLoading->setEnabled(false);
}

void CMangaViewTab::exportFinished()
{
    ui->frameLoading->hide();

    if (!m_exportErrors.isEmpty()) {
        QMessageBox msgBox(parentWnd());
        msgBox.setWindowTitle(QGuiApplication::applicationDisplayName());
        msgBox.setText(tr("Export was finished with %1 errors.").arg(m_exportErrors.count()));
        msgBox.setDetailedText(m_exportErrors.join(QChar(u'\n')));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
}

void CMangaViewTab::exportProgress(int value)
{
    ui->loadingProgress->setValue(value);
}

void CMangaViewTab::exportError(const QString &msg)
{
    m_exportErrors.append(msg);
}

void CMangaViewTab::save()
{
    if (!m_originalScale) {
        QVariantHash data;
        data.clear();
        data[QSL("type")] = QSL("pixivManga");
        data[QSL("url")] = m_origin;

        auto *ex = CAbstractExtractor::extractorFactory(data,this);
        if (ex == nullptr) return;
        if (!gSet->startup()->setupThreadedWorker(ex)) {
            delete ex;
            return;
        }

        QMetaObject::invokeMethod(ex,&CAbstractExtractor::start,Qt::QueuedConnection);
        return;
    }

    if (ui->mangaView->exportPages())
        updateStatusLabel(tr("Saved"));
}

void CMangaViewTab::openOrigin()
{
    if (!m_origin.isEmpty() && m_origin.isValid()) {
        new CBrowserTab(parentWnd(),m_origin);
    }
}

QString CMangaViewTab::mangaDescription() const
{
    return m_mangaDescription;
}

QString CMangaViewTab::mangaTitle() const
{
    return m_mangaTitle;
}

void CMangaViewTab::updateTabColor(bool loadFinished)
{
    int selfIdx = parentWnd()->tabMain->indexOf(this);
    if (selfIdx<0) return;

    // reset color on focus acquiring
    if (!loadFinished) {
        parentWnd()->tabMain->tabBar()->setTabTextColor(
                    selfIdx,gSet->app()->palette(parentWnd()->tabMain->tabBar()).windowText().color());
        return;
    }

    QColor color;
    // change color for background tab only
    if (parentWnd()->tabMain->currentIndex()!=selfIdx) {
        if (loadFinished) {
            color = QColor(Qt::blue);
        }

        if (color.isValid()) {
            parentWnd()->tabMain->tabBar()->setTabTextColor(selfIdx,color);
            parentWnd()->updateTabs();
        }
    }
}

void CMangaViewTab::tabAcquiresFocus()
{
    updateTabColor(false);
}
