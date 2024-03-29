#ifndef MANGAVIEWTAB_H
#define MANGAVIEWTAB_H

#include <QButtonGroup>
#include "global/structures.h"
#include "utils/specwidgets.h"

namespace Ui {
class CMangaViewTab;
}

class CMangaViewTab : public CSpecTabContainer
{
    Q_OBJECT
public:
    explicit CMangaViewTab(CMainWindow *parent, bool setFocused = true);
    ~CMangaViewTab() override;
    void loadMangaPages(const QVector<CUrlWithName> &pages, const QString &title, const QString &description,
                        const QUrl &referer, bool isFanbox, bool originalScale);
    void tabAcquiresFocus() override;

    QString mangaTitle() const;
    QString mangaDescription() const;

private:
    void updateTabTitle();

    void updateStatusLabel(const QString &msg);

public Q_SLOTS:
    void pageNumEdited();
    void pageLoaded(int num);
    void rotationUpdated(double angle);
    void auxMessage(const QString& msg);
    void loadingStarted();
    void loadingFinished();
    void loadingProgress(int value);
    void loadingProgressSize(qint64 size);
    void loadingAborted();
    void exportStarted();
    void exportFinished();
    void exportProgress(int value);
    void exportError(const QString& msg);
    void save();
    void openOrigin();

private:
    Q_DISABLE_COPY(CMangaViewTab)
    Ui::CMangaViewTab *ui;
    QButtonGroup *m_zoomGroup { nullptr };
    QString m_mangaTitle;
    QString m_mangaDescription;
    QStringList m_exportErrors;
    QUrl m_origin;
    bool m_aborted { false };
    bool m_originalScale { false };

    void updateTabColor(bool loadFinished);
};

#endif // MANGAVIEWTAB_H
