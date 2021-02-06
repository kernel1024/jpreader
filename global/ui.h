#ifndef GLOBALUI_H
#define GLOBALUI_H

#include <QObject>
#include <QAction>

#include "structures.h"

class CMainWindow;

class CGlobalUI : public QObject
{
    Q_OBJECT
public:
    explicit CGlobalUI(QObject *parent = nullptr);

    // UI
    QIcon appIcon() const;
    void setSavedAuxSaveDir(const QString& dir);
    void setSavedAuxDir(const QString& dir);
    CMainWindow* addMainWindowEx(bool withSearch, bool withBrowser,
                                 const QVector<QUrl> &viewerUrls = { }) const;
    void settingsTab();
    void translationStatisticsTab();
    QRect getLastMainWindowGeometry() const;
    bool isBlockTabCloseActive() const;
    void setFileDialogNewFolderName(const QString& name);
    void setPixivFetchCovers(CStructures::PixivFetchCoversMode mode);

    void showLightTranslator(const QString& text = QString());

Q_SIGNALS:
    void startAuxTranslation();
    void translationEngineChanged();

public Q_SLOTS:
    void addMainWindow();
    void blockTabClose();
    void showDictionaryWindow();
    void showDictionaryWindowEx(const QString& text);
    void windowDestroyed(CMainWindow* obj);
    void focusChanged(QWidget* old, QWidget* now);
    void forceCharset();

};

#endif // GLOBALUI_H
