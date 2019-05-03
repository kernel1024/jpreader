#include <QApplication>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QVector>
#include <QStringList>
#include <QSslCertificate>

#include "specwidgets.h"
#include "globalcontrol.h"
#include "indexersearch.h"
#include "genericfuncs.h"
#include "adblockrule.h"

#include <sys/resource.h>

CGlobalControl* gSet = nullptr;
QStringList debugMessages;

int main(int argc, char *argv[])
{
    int dbgport = getRandomTCPPort();
    if (dbgport>0) {
        QString url = QStringLiteral("127.0.0.1:%1").arg(dbgport);
        setenv("QTWEBENGINE_REMOTE_DEBUGGING",url.toUtf8().constData(),1);
    }
    qInstallMessageHandler(stdConsoleOutput);
    qRegisterMetaType<CUrlHolder>("CUrlHolder");
    qRegisterMetaType<QDir>("QDir");
    qRegisterMetaType<QSslCertificate>("QSslCertificate");
    qRegisterMetaType<CIntList>("CIntList");
    qRegisterMetaType<CLangPair>("CLangPair");
    qRegisterMetaTypeStreamOperators<CUrlHolder>("CUrlHolder");
    qRegisterMetaTypeStreamOperators<CAdBlockRule>("CAdBlockRule");
    qRegisterMetaTypeStreamOperators<CAdBlockVector>("CAdBlockVector");
    qRegisterMetaTypeStreamOperators<CUrlHolderVector>("CUrlHolderVector");
    qRegisterMetaTypeStreamOperators<CStringHash>("CStringHash");
    qRegisterMetaTypeStreamOperators<CSslCertificateHash>("CSslCertificateHash");
    qRegisterMetaTypeStreamOperators<CLangPairVector>("CLangPairVector");
    qRegisterMetaTypeStreamOperators<CStringSet>("CStringSet");

    QApplication app(argc, argv);

    gSet = new CGlobalControl(&app, dbgport);
    if (gSet->ipcServer==nullptr)
        return 0;

    if (gSet->settings.createCoredumps) {
        // create core dumps on segfaults
        rlimit rlp{};
        getrlimit(RLIMIT_CORE, &rlp);
        rlp.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlp);
    }

    QApplication::setStyle(new CSpecMenuStyle);
    QApplication::setQuitOnLastWindowClosed(false);

    QVector<QUrl> urls;
    for (int i=1;i<qApp->arguments().count();i++) {
        QUrl u(qApp->arguments().at(i));
        if (!u.isEmpty() && u.isValid())
            urls << u;
    }
    gSet->ui.addMainWindowEx(false,true,urls);

    return app.exec();
}
