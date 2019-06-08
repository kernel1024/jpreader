#include <QFile>
#include <QFileInfo>

extern "C" {
#include <unistd.h>
#include <syslog.h>
}

#include "logdisplay.h"
#include "downloadmanager.h"
#include "lighttranslator.h"
#include "globalprivate.h"

namespace CDefaults {
const int settingsSavePeriod = 60000;
}

CGlobalControlPrivate::CGlobalControlPrivate(QObject *parent) : QObject(parent)
{
    m_appIcon.addFile(QStringLiteral(":/img/globe16"));
    m_appIcon.addFile(QStringLiteral(":/img/globe32"));
    m_appIcon.addFile(QStringLiteral(":/img/globe48"));
    m_appIcon.addFile(QStringLiteral(":/img/globe128"));

    settingsSaveTimer.setInterval(CDefaults::settingsSavePeriod);
    settingsSaveTimer.setSingleShot(false);
}

CGlobalControlPrivate::~CGlobalControlPrivate() = default;

void CGlobalControlPrivate::settingsDialogDestroyed()
{
    settingsDialog = nullptr;
}

bool CGlobalControlPrivate::runnedFromQtCreator()
{
    static int ppid = -1;
    static bool res = true;
    static QMutex mtx;
    QMutexLocker locker(&mtx);

    int tpid = getppid();
    if (tpid==ppid)
        return res;

    ppid = tpid;
    if (ppid>0) {
        QFileInfo fi(QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(ppid)));
        res = (fi.fileName().contains(QStringLiteral("creator"),Qt::CaseInsensitive) ||
               (fi.fileName().compare(QStringLiteral("gdb"))==0));
    }

    return res;
}


void CGlobalControlPrivate::stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static bool syslogOpened = false;
    static QMutex loggerMutex;
    const int maxMessagesCount = 5000;

    loggerMutex.lock();

    QString lmsg = QString();

    int line = context.line;
    QString file(QString::fromUtf8(context.file));
    QString category(QString::fromUtf8(context.category));
    if (category==QStringLiteral("default")) {
        category.clear();
    } else {
        category.append(' ');
    }
    int logpri = LOG_NOTICE;

    switch (type) {
        case QtDebugMsg:
            lmsg = QStringLiteral("%1Debug: %2 (%3:%4)").arg(category, msg, file, QString::number(line));
            logpri = LOG_DEBUG;
            break;
        case QtWarningMsg:
            lmsg = QStringLiteral("%1Warning: %2 (%3:%4)").arg(category, msg, file, QString::number(line));
            logpri = LOG_WARNING;
            break;
        case QtCriticalMsg:
            lmsg = QStringLiteral("%1Critical: %2 (%3:%4)").arg(category, msg, file, QString::number(line));
            logpri = LOG_CRIT;
            break;
        case QtFatalMsg:
            lmsg = QStringLiteral("%1Fatal: %2 (%3:%4)").arg(category, msg, file, QString::number(line));
            logpri = LOG_ALERT;
            break;
        case QtInfoMsg:
            lmsg = QStringLiteral("%1Info: %2 (%3:%4)").arg(category, msg, file, QString::number(line));
            logpri = LOG_INFO;
            break;
    }

    if (!lmsg.isEmpty()) {
        QString fmsg = QStringLiteral("%1 %2").arg(QTime::currentTime()
                                                   .toString(QStringLiteral("h:mm:ss")),lmsg);
        gSet->d_func()->debugMessages << fmsg;
        while (gSet->d_func()->debugMessages.count()>maxMessagesCount)
            gSet->d_func()->debugMessages.removeFirst();
        fmsg.append('\n');

        if (runnedFromQtCreator()) {
            fprintf(stderr, "%s", fmsg.toLocal8Bit().constData());
        } else {
            if (!syslogOpened) {
                syslogOpened = true;
                openlog("jpreader", LOG_PID, LOG_USER); // NOLINT
            }
            syslog(logpri, "%s", lmsg.toLocal8Bit().constData());
        }

        if (gSet->logWindow()!=nullptr)
            QMetaObject::invokeMethod(gSet->logWindow(),&CLogDisplay::updateMessages);
    }

    loggerMutex.unlock();
}
