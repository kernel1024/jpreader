#include "globalprivate.h"

const int settingsSavePeriod = 60000;

CGlobalControlPrivate::CGlobalControlPrivate(QObject *parent) : QObject(parent)
{
    m_appIcon.addFile(QStringLiteral(":/img/globe16"));
    m_appIcon.addFile(QStringLiteral(":/img/globe32"));
    m_appIcon.addFile(QStringLiteral(":/img/globe48"));
    m_appIcon.addFile(QStringLiteral(":/img/globe128"));

    settingsSaveTimer.setInterval(settingsSavePeriod);
    settingsSaveTimer.setSingleShot(false);
}

void CGlobalControlPrivate::settingsDialogDestroyed()
{
    settingsDialog = nullptr;
}
