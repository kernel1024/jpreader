#include <algorithm>
#include <execution>
#include <functional>
#include <climits>
#include <cmath>

#include <QPainter>
#include <QScrollBar>
#include <QApplication>
#include <QMessageBox>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QProgressDialog>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QComboBox>
#include <QUuid>

#include "zmangaview.h"
#include "utils/genericfuncs.h"
#include "global/control.h"
#include "global/structures.h"
#include "global/network.h"
#include "global/ui.h"
#include "global/settings.h"
#include "global/startup.h"
#include "browser-utils/downloadwriter.h"

namespace CDefaults {
const int errorPageLoadMsgVerticalMargin = 5;
const double dynamicZoomUpScale = 3.0;
const auto propertyMangaPageNum = "PAGENUM";
}

ZMangaView::ZMangaView(QWidget *parent) :
    QWidget(parent)
{
    setMouseTracking(true);
    setBackgroundRole(QPalette::Dark);
    QPalette p = palette();
    p.setBrush(QPalette::Dark,QBrush(QColor(Qt::black)));
    setPalette(p);

    connect(this, &ZMangaView::requestRedrawPageEx, this, &ZMangaView::redrawPageEx, Qt::QueuedConnection);
    connect(gSet->settings(), &CSettings::mangaViewerSettingsUpdated, this, &ZMangaView::redrawPage, Qt::QueuedConnection);
}

ZMangaView::~ZMangaView()
{
    if (m_networkLoadersActive > 0) {
        Q_EMIT abortNetworkRequest();
    }
}

void ZMangaView::setZoomMode(int mode)
{
    m_zoomMode = static_cast<ZMangaView::ZoomMode>(mode);
    redrawPage();
}

int ZMangaView::getPageCount() const
{
    return m_pageCount;
}

void ZMangaView::getPage(int num)
{
    if (!m_scroller) return;
    m_currentPage = num;

    if (m_scroller->verticalScrollBar()->isVisible())
        m_scroller->verticalScrollBar()->setValue(0);
    if (m_scroller->horizontalScrollBar()->isVisible())
        m_scroller->horizontalScrollBar()->setValue(0);

    cacheDropUnusable();
    if (m_iCacheImages.contains(m_currentPage))
        displayCurrentPage();
    cacheFillNearest();
}

int ZMangaView::getCurrentPage() const
{
    return m_currentPage;
}

void ZMangaView::setScroller(QScrollArea *scroller)
{
    m_scroller = scroller;
}

void ZMangaView::displayCurrentPage()
{
    QImage p;

    if (m_iCacheImages.contains(m_currentPage)) {
        p = m_iCacheImages.value(m_currentPage);

        if (m_rotation!=0) {
            QTransform mr;
            mr.rotateRadians(m_rotation*M_PI_2);
            p = p.transformed(mr,Qt::SmoothTransformation);
        }
    }

    m_curUnscaledPixmap = p;
    Q_EMIT loadedPage(m_currentPage);
    setFocus();
    redrawPage();
}

void ZMangaView::closeManga()
{
    m_curPixmap = QImage();
    m_openedManga.clear();
    m_pageData.clear();
    m_processingPages.clear();
    m_pageCount = 0;
    m_networkLoadedTotal = 0L;
    update();
    Q_EMIT loadedPage(-1);
    Q_EMIT abortNetworkRequest();
    Q_EMIT loadingFinished();
    m_aborted = false;
    m_finished = false;
}

void ZMangaView::setPage(int page)
{
    m_scrollAccumulator = 0;
    if (page<0 || page>=m_pageCount) return;
    getPage(page);
}

void ZMangaView::wheelEvent(QWheelEvent *event)
{
    if (!m_scroller) return;
    int sf = 1;
    int dy = event->angleDelta().y();
    QScrollBar *vb = m_scroller->verticalScrollBar();
    if (vb!=nullptr && vb->isVisible()) { // page is zoomed and scrollable
        if (((vb->value()>vb->minimum()) && (vb->value()<vb->maximum()))    // somewhere middle of page
                || ((vb->value()==vb->minimum()) && (dy<0))                 // at top, scrolling down
                || ((vb->value()==vb->maximum()) && (dy>0))) {              // at bottom, scrolling up
            m_scrollAccumulator = 0;
            event->ignore();
            return;
        }

        // at the page border, attempting to flip the page
        sf = gSet->settings()->mangaScrollFactor;
    }

    if (abs(dy) < gSet->ui()->getMangaDetectedScrollDelta())
        gSet->ui()->setMangaDetectedScrollDelta(abs(dy));

    m_scrollAccumulator+= dy;
    int numSteps = m_scrollAccumulator / (gSet->settings()->mangaScrollDelta * sf);

    if (numSteps!=0)
        setPage(m_currentPage-numSteps);

    event->accept();
}

QImage ZMangaView::resizeImage(const QImage& src, const QSize& targetSize, bool forceFilter,
                   Blitz::ScaleFilterType filter, int page, const int *currentPage)
{
    QSize dsize = src.size().scaled(targetSize,Qt::KeepAspectRatio);

    Blitz::ScaleFilterType rf = gSet->settings()->mangaDownscaleFilter;
    if (dsize.width()>src.size().width())
        rf = gSet->settings()->mangaUpscaleFilter;

    if (forceFilter)
        rf = filter;

    if (rf==Blitz::UndefinedFilter)
        return src.scaled(targetSize,Qt::IgnoreAspectRatio,Qt::FastTransformation);
    if (rf==Blitz::Bilinear)
        return src.scaled(targetSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

    return Blitz::smoothScaleFilter(src,targetSize,gSet->settings()->mangaResizeBlur,
                                    rf,Qt::KeepAspectRatio,page,currentPage);
}

void ZMangaView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (!m_scroller) return;

    QPainter w(this);
    if (!m_curPixmap.isNull()) {
        int x=0;
        int y=0;
        if (m_curPixmap.width() < m_scroller->viewport()->width())
            x = (m_scroller->viewport()->width() - m_curPixmap.width()) / 2;
        if (m_curPixmap.height() < m_scroller->viewport()->height())
            y = (m_scroller->viewport()->height() - m_curPixmap.height()) / 2;
        w.drawImage(x,y,m_curPixmap);

        if (m_zoomDynamic) {
            QPoint mp(m_zoomPos.x()-x,m_zoomPos.y()-y);
            QRect baseRect = m_curPixmap.rect();
            if (baseRect.contains(mp,true)) {
                mp.setX(mp.x()*m_curUnscaledPixmap.width()/m_curPixmap.width());
                mp.setY(mp.y()*m_curUnscaledPixmap.height()/m_curPixmap.height());
                int msz = gSet->settings()->mangaMagnifySize;

                if (m_curPixmap.width()<m_curUnscaledPixmap.width() || m_curPixmap.height()<m_curUnscaledPixmap.height()) {
                    QRect cutBox(mp.x()-msz/2,mp.y()-msz/2,msz,msz);
                    baseRect = m_curUnscaledPixmap.rect();
                    if (cutBox.left()<baseRect.left()) cutBox.moveLeft(baseRect.left());
                    if (cutBox.right()>baseRect.right()) cutBox.moveRight(baseRect.right());
                    if (cutBox.top()<baseRect.top()) cutBox.moveTop(baseRect.top());
                    if (cutBox.bottom()>baseRect.bottom()) cutBox.moveBottom(baseRect.bottom());
                    QImage zoomed = m_curUnscaledPixmap.copy(cutBox);
                    baseRect = QRect(QPoint(m_zoomPos.x()-zoomed.width()/2,m_zoomPos.y()-zoomed.height()/2),
                                     zoomed.size());
                    if (baseRect.left()<0) baseRect.moveLeft(0);
                    if (baseRect.right()>width()) baseRect.moveRight(width());
                    if (baseRect.top()<0) baseRect.moveTop(0);
                    if (baseRect.bottom()>height()) baseRect.moveBottom(height());
                    w.drawImage(baseRect,zoomed);
                } else {
                    QRect cutBox(mp.x()-msz/4,mp.y()-msz/4,msz/2,msz/2);
                    baseRect = m_curUnscaledPixmap.rect();
                    if (cutBox.left()<baseRect.left()) cutBox.moveLeft(baseRect.left());
                    if (cutBox.right()>baseRect.right()) cutBox.moveRight(baseRect.right());
                    if (cutBox.top()<baseRect.top()) cutBox.moveTop(baseRect.top());
                    if (cutBox.bottom()>baseRect.bottom()) cutBox.moveBottom(baseRect.bottom());
                    QImage zoomed = m_curUnscaledPixmap.copy(cutBox);
                    zoomed = ZMangaView::resizeImage(zoomed,zoomed.size()*CDefaults::dynamicZoomUpScale,
                                                     true,gSet->settings()->mangaUpscaleFilter);
                    baseRect = QRect(QPoint(m_zoomPos.x()-zoomed.width()/2,m_zoomPos.y()-zoomed.height()/2),
                                     zoomed.size());
                    if (baseRect.left()<0) baseRect.moveLeft(0);
                    if (baseRect.right()>width()) baseRect.moveRight(width());
                    if (baseRect.top()<0) baseRect.moveTop(0);
                    if (baseRect.bottom()>height()) baseRect.moveBottom(height());
                    w.drawImage(baseRect,zoomed);
                }
            }

        }
    } else {
        if (m_curUnscaledPixmap.isNull()) {
            const int preferredIconSize = 32;
            QPixmap p = QIcon::fromTheme(QSL("edit-delete")).pixmap(preferredIconSize);
            w.drawPixmap((width()-p.width())/2,(height()-p.height())/2,p);
            w.setPen(QPen(gSet->ui()->getMangaForegroundColor()));
            w.drawText(0,(height()-p.height())/2+p.height()+CDefaults::errorPageLoadMsgVerticalMargin,
                       width(),w.fontMetrics().height(),Qt::AlignCenter,
                       tr("Page %1 now loading...").arg(m_currentPage+1));
        }
    }
}

void ZMangaView::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::NoButton) {
        if ((QApplication::keyboardModifiers() & Qt::ControlModifier) == 0) {
            if (m_zoomDynamic) {
                m_zoomPos = event->pos();
                update();
            } else
                m_zoomPos = QPoint();
        }
    }
}

void ZMangaView::keyPressEvent(QKeyEvent *event)
{
    if (!m_scroller) return;

    QScrollBar* vb = m_scroller->verticalScrollBar();
    QScrollBar* hb = m_scroller->horizontalScrollBar();
    switch (event->key()) {
        case Qt::Key_Up:
            if (vb) vb->setValue(vb->value()-vb->singleStep());
            break;
        case Qt::Key_Down:
            if (vb) vb->setValue(vb->value()+vb->singleStep());
            break;
        case Qt::Key_Left:
            if (hb) hb->setValue(hb->value()-hb->singleStep());
            break;
        case Qt::Key_Right:
            if (hb) hb->setValue(hb->value()+hb->singleStep());
            break;
        case Qt::Key_Space:
        case Qt::Key_PageDown:
            navNext();
            break;
        case Qt::Key_Backspace:
        case Qt::Key_PageUp:
            navPrev();
            break;
        case Qt::Key_Home:
            navFirst();
            break;
        case Qt::Key_End:
            navLast();
            break;
        default:
            Q_EMIT keyPressed(event->key());
            break;
    }
    event->accept();
}

void ZMangaView::redrawPage()
{
    redrawPageEx(QImage(),m_currentPage);
}

void ZMangaView::redrawPageEx(const QImage& scaled, int page)
{
    if (!m_scroller) return;
    if (!scaled.isNull() && page!=m_currentPage) return;

    QPalette p = palette();
    p.setBrush(QPalette::Dark,QBrush(gSet->settings()->mangaBackgroundColor));
    setPalette(p);

    if (m_openedManga.isEmpty()) return;
    if (page<0 || page>=m_pageCount) return;

    // Draw current page
    m_curPixmap = QImage();

    if (!m_curUnscaledPixmap.isNull()) {
        QSize scrollerSize = m_scroller->viewport()->size() - QSize(4,4);
        QSize targetSize = m_curUnscaledPixmap.size();

        m_curPixmap = m_curUnscaledPixmap;
        if (!m_curUnscaledPixmap.isNull() && m_curUnscaledPixmap.height()>0) {
            double pixAspect = static_cast<double>(m_curUnscaledPixmap.width()) /
                               static_cast<double>(m_curUnscaledPixmap.height());
            double myAspect = static_cast<double>(width()) /
                              static_cast<double>(height());

            switch (m_zoomMode) {
                case zmFit:
                    if ( pixAspect > myAspect ) { // the image is wider than widget -> resize by width
                        targetSize = QSize(scrollerSize.width(),
                                           qRound((static_cast<double>(scrollerSize.width()))/pixAspect));
                    } else {
                        targetSize = QSize(qRound((static_cast<double>(scrollerSize.height()))*pixAspect),
                                           scrollerSize.height());
                    }
                    break;
                case zmWidth:
                    targetSize = QSize(scrollerSize.width(),
                                       qRound((static_cast<double>(scrollerSize.width()))/pixAspect));
                    break;
                case zmHeight:
                    targetSize = QSize(qRound((static_cast<double>(scrollerSize.height()))*pixAspect),
                                       scrollerSize.height());
                    break;
                case zmOriginal:
                    if (m_zoomAny>0)
                        targetSize = m_curUnscaledPixmap.size()*(static_cast<double>(m_zoomAny)/100.0);
                    break;
            }

            if (targetSize!=m_curUnscaledPixmap.size()) {
                if (!scaled.isNull()) {
                    m_curPixmap = scaled;
                } else {
                    // fast inplace resampling
                    m_curPixmap = m_curUnscaledPixmap.scaled(targetSize,Qt::IgnoreAspectRatio,
                                                   Qt::FastTransformation);

                    if (gSet->settings()->mangaUseFineRendering) {
                        Blitz::ScaleFilterType filter = Blitz::UndefinedFilter;
                        if (targetSize.width()>m_curUnscaledPixmap.width()) {
                            filter = gSet->settings()->mangaUpscaleFilter;
                        } else {
                            filter = gSet->settings()->mangaDownscaleFilter;
                        }

                        if (filter!=Blitz::UndefinedFilter) {
                            QImage image = m_curUnscaledPixmap;
                            m_mangaThreadPool.start([this,targetSize,filter,image,page](){
                                QElapsedTimer timer;
                                timer.start();

                                QImage res = ZMangaView::resizeImage(image,targetSize,true,filter,
                                                                     page,&m_currentPage);

                                if (!res.isNull()) {
                                    qint64 elapsed = timer.elapsed();
                                    Q_EMIT requestRedrawPageEx(res, page);
                                    gSet->ui()->addMangaFineRenderTime(elapsed);
                                }
                            });
                        }
                    }
                }
            }
        }
        setMinimumSize(m_curPixmap.width(),m_curPixmap.height());

        if (targetSize.height()<scrollerSize.height()) targetSize.setHeight(scrollerSize.height());
        if (targetSize.width()<scrollerSize.width()) targetSize.setWidth(scrollerSize.width());
        resize(targetSize);
    }
    update();
}

void ZMangaView::ownerResized(const QSize &size)
{
    Q_UNUSED(size)

    redrawPage();
}

bool ZMangaView::exportPages()
{
    if (m_openedManga.isEmpty() || (m_pageCount<1) || (m_zipWorkersActive>0))
        return false;

    if (!m_finished) {
        QMessageBox::information(gSet->activeWindow(),QGuiApplication::applicationDisplayName(),
                                 tr("Manga download not completed. Please wait."));
        return false;
    }

    if (m_aborted &&
            (QMessageBox::question(gSet->activeWindow(),QGuiApplication::applicationDisplayName(),
                                   tr("Manga download was abored. Save incomplete data?"),
                                   QMessageBox::Yes,QMessageBox::No) != QMessageBox::Yes)) {
        return false;
    }

    const QString containerPath = CGenericFuncs::getSaveFileNameD(gSet->activeWindow(),tr("Save manga to file"),
                                                                  gSet->settings()->savedAuxSaveDir,
                                                                  QStringList( { tr("ZIP archive (*.zip)") } ),
                                                                  nullptr,m_openedManga);
    if (containerPath.isNull() || containerPath.isEmpty())
        return false;
    gSet->ui()->setSavedAuxSaveDir(QFileInfo(containerPath).absolutePath());

    m_zipWorkersActive = 0;
    Q_EMIT exportProgress(0);
    Q_EMIT exportStarted();
    {
        QMutexLocker locker(&m_pageDataLock);
        int idx = 1;
        const int pageCount = m_pageData.count();
        for (const auto &file : qAsConst(m_pageData)) {
            const QString fileName = QSL("%1_%2")
                                     .arg(CGenericFuncs::paddedNumber(idx++,pageCount),
                                          CGenericFuncs::decodeHtmlEntities(file.first.fileName()));
            QPointer<CDownloadWriter> writer(makeWriterJob(containerPath,fileName));
            if (writer) {
                const QByteArray data = file.second;
                QMetaObject::invokeMethod(writer, [writer,data](){
                    writer->appendBytesToFile(data);
                    writer->finalizeFile(true,false);
                }, Qt::QueuedConnection);
                m_zipWorkersActive++;
            }
        }
    }
    return true;
}

CDownloadWriter* ZMangaView::makeWriterJob(const QString& zipName, const QString& fileName) const
{
    auto *writer = new CDownloadWriter(nullptr,
                                       zipName,
                                       fileName,
                                       0L,
                                       QUuid::createUuid());
    if (!gSet->startup()->setupThreadedWorker(writer)) {
        delete writer;
        return nullptr;
    }

    connect(writer,&CDownloadWriter::writeComplete,
            this,&ZMangaView::writerCompleted,Qt::QueuedConnection);
    connect(writer,&CDownloadWriter::error,
            this,&ZMangaView::writerError,Qt::QueuedConnection);

    QMetaObject::invokeMethod(writer,&CAbstractThreadWorker::start,Qt::QueuedConnection);
    return writer;
}

void ZMangaView::writerCompleted(bool success)
{
    Q_UNUSED(success)

    m_zipWorkersActive--;
    Q_EMIT exportProgress(100 * (m_pageCount - m_zipWorkersActive) / m_pageCount);
    if (m_zipWorkersActive < 1) {
        Q_EMIT exportFinished();
    }
}

void ZMangaView::writerError(const QString &message)
{
    Q_EMIT exportError(message);
}

void ZMangaView::navFirst()
{
    setPage(0);
}

void ZMangaView::navPrev()
{
    if (m_currentPage>0)
        setPage(m_currentPage-1);
}

void ZMangaView::navNext()
{
    if (m_currentPage<m_pageCount-1)
        setPage(m_currentPage+1);
}

void ZMangaView::navLast()
{
    setPage(m_pageCount-1);
}

void ZMangaView::abortLoading()
{
    m_aborted = true;
    Q_EMIT abortNetworkRequest();
}

void ZMangaView::setZoomDynamic(bool state)
{
    m_zoomDynamic = state;
    redrawPage();
}

bool ZMangaView::getZoomDynamic() const
{
    return m_zoomDynamic;
}

bool ZMangaView::isMangaOpened() const
{
    return (m_pageCount > 0);
}

void ZMangaView::setZoomAny(int comboIdx)
{
    auto *cb = qobject_cast<QComboBox *>(sender());
    if (cb==nullptr) return;
    QString s = cb->itemText(comboIdx);
    m_zoomAny = -1; // original zoom
    s.remove(QRegularExpression(QSL("[^0123456789]")));
    bool okconv = false;
    if (!s.isEmpty()) {
        m_zoomAny = s.toInt(&okconv);
        if (!okconv)
            m_zoomAny = -1;
    }
    redrawPage();
}

void ZMangaView::viewRotateCCW()
{
    m_rotation--;
    if (m_rotation<0) m_rotation = 3;
    displayCurrentPage();
    Q_EMIT rotationUpdated(m_rotation*M_PI_2);
}

void ZMangaView::viewRotateCW()
{
    m_rotation++;
    if (m_rotation>3) m_rotation = 0;
    displayCurrentPage();
    Q_EMIT rotationUpdated(m_rotation*M_PI_2);
}

void ZMangaView::cacheGotPage(const QImage &pageImage, int num)
{
    if (m_processingPages.contains(num))
        m_processingPages.removeOne(num);

    if (!pageImage.isNull() && m_iCacheImages.contains(num))
        return;

    if (!pageImage.isNull())
        m_iCacheImages[num]=pageImage;

    if (num==m_currentPage)
        displayCurrentPage();
}

void ZMangaView::cacheDropUnusable()
{
    QList<int> toCache = cacheGetActivePages();
    for (auto it = m_iCacheImages.keyBegin(), end = m_iCacheImages.keyEnd(); it != end; ++it) {
        const int num = *it;
        if (!toCache.contains(num))
            m_iCacheImages.remove(num);
    }
}

void ZMangaView::cacheFillNearest()
{
    QList<int> toCache = cacheGetActivePages();
    int idx = 0;
    while (idx<toCache.count()) {
        if (m_iCacheImages.contains(toCache.at(idx))) {
            toCache.removeAt(idx);
        } else {
            idx++;
        }
    }

    for (int i=0;i<toCache.count();i++) {
        if (!m_processingPages.contains(toCache.at(i))) {
            m_processingPages << toCache.at(i);
            cacheGetPage(toCache.at(i));
        }
    }
}

QList<int> ZMangaView::cacheGetActivePages() const
{
    QList<int> l;
    l.reserve(gSet->settings()->mangaCacheWidth*2+1);

    if (m_currentPage==-1) return l;
    if (m_pageCount<=0) {
        l.append(m_currentPage);
        return l;
    }

    int cacheRadius = 1;
    if (gSet->settings()->mangaCacheWidth>=2) {
        if ((gSet->settings()->mangaCacheWidth % 2)==0) {
            cacheRadius = gSet->settings()->mangaCacheWidth / 2;
        } else {
            cacheRadius = (gSet->settings()->mangaCacheWidth+1) / 2;
        }
    }

    l.append(m_currentPage); // load current page at first

    for (int i=0;i<cacheRadius;i++) {
        // first pages, then last pages
        const QList<int> pageNums ({ i, m_pageCount-i-1 });
        for (const auto& num : pageNums) {
            if (!l.contains(num) && (num>=0) && (num<m_pageCount))
                l.append(num);
        }
    }
    // pages around current page
    for (int i=(m_currentPage-cacheRadius);i<(m_currentPage+cacheRadius);i++) {
        if (i>=0 && i<m_pageCount && !l.contains(i))
            l.append(i);
    }
    return l;
}

void ZMangaView::loadMangaPages(const QVector<CUrlWithName> &pages, const QString &title,
                                const QUrl &referer, bool isFanbox)
{
    m_iCacheImages.clear();
    m_pageData.clear();
    m_currentPage = 0;
    m_curUnscaledPixmap = QImage();
    m_pageCount = 0;
    m_processingPages.clear();
    m_aborted = false;
    m_finished = false;
    redrawPage();
    Q_EMIT loadedPage(-1);

    m_openedManga = title;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_pageData.resize(pages.count());
#else
    for (int i=0; i<pages.count(); i++)
        m_pageData.append(qMakePair(QUrl(),QByteArray()));
#endif
    m_pageCount = pages.count();
    m_networkLoadersActive = 0;
    m_networkLoadedTotal = 0L;
    for (int i=0; i<pages.count(); i++) {
        const QUrl url(pages.at(i).first);
        m_pageData[i].first = url;
        QNetworkRequest req(url);
        req.setRawHeader("referer",referer.toString().toUtf8());
        if (isFanbox)
            req.setRawHeader("origin","https://www.fanbox.cc");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
        req.setMaximumRedirectsAllowed(CDefaults::httpMaxRedirects);
        req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute,true);
        QNetworkReply* rpl = gSet->net()->auxNetworkAccessManagerGet(req,true);
        rpl->setProperty(CDefaults::propertyMangaPageNum,i);
        connect(rpl, &QNetworkReply::finished, this, &ZMangaView::mangaPageDownloaded);
        connect(this, &ZMangaView::abortNetworkRequest, rpl, &QNetworkReply::abort);
        m_networkLoadersActive++;
    }
    Q_EMIT loadingStarted();
    Q_EMIT loadingProgress(0,0L);
    setPage(0); // force display update with incomplete page
}

void ZMangaView::mangaPageDownloaded()
{
    QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> rpl(qobject_cast<QNetworkReply *>(sender()));
    int status = CGenericFuncs::getHttpStatusFromReply(rpl.data());
    bool numOk = false;
    int pageNum = rpl->property(CDefaults::propertyMangaPageNum).toInt(&numOk);
    if ((rpl->error() != QNetworkReply::NoError) || (status >= CDefaults::httpCodeRedirect) || (!numOk)) {
        qWarning() << "Manga viewer network request failed: " << rpl->url();
    } else {
        const QByteArray data = rpl->readAll();
        {
            QMutexLocker locker(&m_pageDataLock);
            m_pageData[pageNum].second = data;
        }
        m_networkLoadedTotal += data.size();
        if (m_currentPage == pageNum)
            setPage(pageNum);
    }
    m_networkLoadersActive--;
    Q_EMIT loadingProgress(100 * (m_pageCount - m_networkLoadersActive) / m_pageCount,
                           m_networkLoadedTotal);
    if (m_networkLoadersActive < 1) {
        Q_EMIT loadingFinished();
        m_finished = true;
    }
}

void ZMangaView::cacheGetPage(int num)
{
    QMutexLocker locker(&m_pageDataLock);

    auto *work = new ZImageLoaderRunnable;
    work->setAutoDelete(true);
    work->setPageData(m_pageData.at(num).second,num);
    connect(work,&ZImageLoaderRunnable::pageReady,this,&ZMangaView::cacheGotPage,Qt::QueuedConnection);
    m_mangaThreadPool.start(work);
}

void ZImageLoaderRunnable::run()
{
    const QImage img = QImage::fromData(m_pageData);
    Q_EMIT pageReady(img,m_pageNum);
}

void ZImageLoaderRunnable::setPageData(const QByteArray &data, int pageNum)
{
    m_pageNum = pageNum;
    m_pageData = data;
}
