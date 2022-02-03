#ifndef ZMANGAVIEW_H
#define ZMANGAVIEW_H

#include <QWidget>
#include <QRubberBand>
#include <QPixmap>
#include <QImage>
#include <QScrollArea>
#include <QThreadPool>
#include <QPointer>
#include <QHash>
#include <QMutex>
#include <QUrl>
#include <QList>
#include <QAtomicInteger>
#include "scalefilter.h"
#include "global/structures.h"
#include "browser-utils/downloadwriter.h"

class ZMangaView : public QWidget
{
    Q_OBJECT
public:
    enum ZoomMode {
        zmFit = 1,
        zmWidth = 2,
        zmHeight = 3,
        zmOriginal = 4
    };
    Q_ENUM(ZoomMode)

private:
    Q_DISABLE_COPY(ZMangaView)

    ZoomMode m_zoomMode { zmFit };
    bool m_zoomDynamic { false };
    bool m_aborted { false };
    bool m_finished { false };
    int m_rotation { 0 };
    int m_currentPage { 0 };
    int m_scrollAccumulator { 0 };
    int m_pageCount { 0 };
    int m_zoomAny { -1 };
    QAtomicInteger<int> m_networkLoadersActive;
    QAtomicInteger<qint64> m_networkLoadedTotal;
    QAtomicInteger<int> m_zipWorkersActive;
    QImage m_curPixmap;
    QImage m_curUnscaledPixmap;
    QPoint m_zoomPos;
    QPointer<QScrollArea> m_scroller;

    QString m_openedManga;

    QThreadPool m_mangaThreadPool;

    QHash<int,QImage> m_iCacheImages;
    QList<QPair<QUrl,QByteArray> > m_pageData;
    QMutex m_pageDataLock;
    QList<int> m_processingPages;

    void cacheDropUnusable();
    void cacheFillNearest();
    QList<int> cacheGetActivePages() const;
    void displayCurrentPage();
    void cacheGetPage(int num);   
    static QImage resizeImage(const QImage &src, const QSize &targetSize, bool forceFilter,
                              Blitz::ScaleFilterType filter, int page = -1, const int *currentPage = nullptr);
    CDownloadWriter *makeWriterJob(const QString &zipName, const QString &fileName) const;

public:
    explicit ZMangaView(QWidget *parent = nullptr);
    ~ZMangaView() override;
    int getPageCount() const;
    void getPage(int num);
    int getCurrentPage() const;
    void setScroller(QScrollArea* scroller);
    bool getZoomDynamic() const;
    bool isMangaOpened() const;
    void loadMangaPages(const QVector<CUrlWithName> &pages, const QString &title,
                        const QUrl &referer, bool isFanbox);

Q_SIGNALS:
    void loadedPage(int num);
    void keyPressed(int key);
    void rotationUpdated(double angle);
    void auxMessage(const QString& msg);
    void requestRedrawPageEx(const QImage &scaled, int page);

    void abortNetworkRequest();
    void loadingStarted();
    void loadingFinished();
    void loadingProgress(int value, qint64 totalSize);

    void exportStarted();
    void exportFinished();
    void exportProgress(int value);
    void exportError(const QString& msg);

private Q_SLOTS:
    void writerCompleted(bool success);
    void writerError(const QString &message);
    void cacheGotPage(const QImage &pageImage, int num);
    void mangaPageDownloaded();
    void redrawPage();
    void redrawPageEx(const QImage &scaled, int page);

public Q_SLOTS:
    void setZoomMode(int mode);
    void closeManga();
    void setPage(int page);
    void ownerResized(const QSize& size);

    void navFirst();
    void navPrev();
    void navNext();
    void navLast();
    void abortLoading();

    void setZoomDynamic(bool state);
    void setZoomAny(int comboIdx);

    void viewRotateCCW();
    void viewRotateCW();

    bool exportPages();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

class ZImageLoaderRunnable : public QObject, public QRunnable
{
    Q_OBJECT
private:
    int m_pageNum { -1 };
    QByteArray m_pageData;
public:
    void run() override;
    void setPageData(const QByteArray& data, int pageNum);
Q_SIGNALS:
    void pageReady(const QImage &image, int pageNum);
};

#endif // ZMANGAVIEW_H
