#include <QTextCodec>
#include <QStringList>
#include <QMimeData>
#include <QString>
#include <QTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutex>
#include <QTcpServer>
#include <QRegExp>
#include <QtTest>

#include <iostream>
#include <unistd.h>
#include <magic.h>
#include <unicode/utypes.h>
#include <unicode/localpointer.h>
#include <unicode/uenum.h>
#include <unicode/ucsdet.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "genericfuncs.h"
#include "globalcontrol.h"

static QSize openFileDialogSize = QSize();
static QSize saveFileDialogSize = QSize();

bool runnedFromQtCreator()
{
    return qEnvironmentVariableIsSet("QT_LOGGING_TO_CONSOLE");
}

int getRandomTCPPort()
{
    QTcpServer srv;
    int res = -1;
    for (quint16 i=20000;i<40000;i++) {
        if (srv.listen(QHostAddress(QHostAddress(QHostAddress::LocalHost)),i)) {
            res = i;
            break;
        }
    }
    if (srv.isListening())
        srv.close();
    return res;
}

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static bool syslogOpened = false;
    static QMutex loggerMutex;

    loggerMutex.lock();

    QString lmsg = QString();

    int line = context.line;
    QString file = QString(context.file);
    QString category = QString(context.category);
    if (category==QString("default"))
        category.clear();
    else
        category.append(' ');
    int logpri = LOG_NOTICE;

    switch (type) {
        case QtDebugMsg:
            lmsg = QString("%1Debug: %2 (%3:%4)").arg(category, msg, file, QString("%1").arg(line));
            logpri = LOG_DEBUG;
            break;
        case QtWarningMsg:
            lmsg = QString("%1Warning: %2 (%3:%4)").arg(category, msg, file, QString("%1").arg(line));
            logpri = LOG_WARNING;
            break;
        case QtCriticalMsg:
            lmsg = QString("%1Critical: %2 (%3:%4)").arg(category, msg, file, QString("%1").arg(line));
            logpri = LOG_CRIT;
            break;
        case QtFatalMsg:
            lmsg = QString("%1Fatal: %2 (%3:%4)").arg(category, msg, file, QString("%1").arg(line));
            logpri = LOG_ALERT;
            break;
        case QtInfoMsg:
            lmsg = QString("%1Info: %2 (%3:%4)").arg(category, msg, file, QString("%1").arg(line));
            logpri = LOG_INFO;
            break;
    }

    if (!lmsg.isEmpty()) {
        QString fmsg = QTime::currentTime().toString("h:mm:ss") + " "+lmsg;
        debugMessages << fmsg;
        while (debugMessages.count()>5000)
            debugMessages.removeFirst();
        fmsg.append('\n');

        if (runnedFromQtCreator())
            fprintf(stderr, "%s", fmsg.toLocal8Bit().constData());
        else {
            if (!syslogOpened) {
                syslogOpened = true;
                openlog("jpreader", LOG_PID, LOG_USER);
            }
            syslog(logpri, "%s", lmsg.toLocal8Bit().constData());
        }

        if (gSet!=nullptr && gSet->logWindow!=nullptr)
            QMetaObject::invokeMethod(gSet->logWindow,"updateMessages");
    }

    loggerMutex.unlock();
}

bool checkAndUnpackUrl(QUrl& url)
{
    // Extract jump url for pixiv
    if (url.host().endsWith("pixiv.net") && url.path().startsWith("/jump")) {
        QUrl ju(url.query(QUrl::FullyDecoded));
        if (ju.isValid()) {
            url = ju;
            return true;
        }
    }

    // Url not modified
    return false;
}

QString detectMIME(const QString &filename)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE);
    magic_load(myt,nullptr);
    QByteArray bma = filename.toUtf8();
    const char* bm = bma.data();
    const char* mg = magic_file(myt,bm);
    if (mg==nullptr) {
        qCritical() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QString("text/plain");
    }
    QString mag(mg);
    magic_close(myt);
    return mag;
}

QString detectMIME(const QByteArray &buf)
{
    magic_t myt = magic_open(MAGIC_ERROR|MAGIC_MIME_TYPE);
    magic_load(myt,nullptr);
    const char* mg = magic_buffer(myt,buf.data(),static_cast<size_t>(buf.length()));
    if (mg==nullptr) {
        qCritical() << "libmagic error: " << magic_errno(myt) << QString::fromUtf8(magic_error(myt));
        return QString("text/plain");
    }
    QString mag(mg);
    magic_close(myt);
    return mag;
}

QString detectEncodingName(const QByteArray& content) {
    QString codepage = "";

    if (!gSet->settings.forcedCharset.isEmpty() &&
            QTextCodec::availableCodecs().contains(gSet->settings.forcedCharset.toLatin1()))
        return gSet->settings.forcedCharset;

    QTextCodec* enc = QTextCodec::codecForLocale();
    QByteArray icu_enc = "";
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector* csd = ucsdet_open(&status);
    ucsdet_setText(csd, content.constData(), content.length(), &status);
    const UCharsetMatch *ucm;
    ucm = ucsdet_detect(csd, &status);
    if (status==U_ZERO_ERROR && ucm!=nullptr) {
        const char* cname = ucsdet_getName(ucm,&status);
        if (status==U_ZERO_ERROR) icu_enc = QByteArray(cname);
    }
    if (QTextCodec::availableCodecs().contains(icu_enc)) {
        enc = QTextCodec::codecForName(icu_enc);
    }
    ucsdet_close(csd);

    codepage = QTextCodec::codecForHtml(content,enc)->name();
    if (codepage.contains("x-sjis",Qt::CaseInsensitive)) codepage="SJIS";
    return codepage;
}

QTextCodec* detectEncoding(const QByteArray& content) {
    QString codepage = detectEncodingName(content);

    if (!codepage.isEmpty())
        return QTextCodec::codecForName(codepage.toLatin1());
    else {
        if (!QTextCodec::codecForLocale())
            return QTextCodec::codecForName("UTF-8");
        else
            return QTextCodec::codecForLocale();
    }
}

QString detectDecodeToUnicode(const QByteArray& content)
{
    QTextCodec *cd = detectEncoding(content);
    return cd->toUnicode(content);
}


QString makeSimpleHtml(const QString &title, const QString &content, bool integratedTitle, const QUrl& origin)
{
    QString s = content;
    QString cnt = s.replace(QRegExp("\n{3,}"),"\n\n").replace("\n","<br />\n");
    QString cn="<html><head>";
    cn+="<META HTTP-EQUIV=\"Content-type\" CONTENT=\"text/html; charset=UTF-8;\">";
    cn+="<title>"+title+"</title></head>";
    cn+="<body>";
    if (integratedTitle) {
        cn+="<h3>";
        if (!origin.isEmpty())
            cn+="<a href=\""+origin.toString(QUrl::FullyEncoded)+"\">";
        cn+=title;
        if (!origin.isEmpty())
            cn+="</a>";
        cn+="</h3>";
    }
    cn+=cnt+"</body></html>";
    return cn;
}

QString getClipboardContent(bool noFormatting, bool plainpre) {
    QString res = "";
    QString cbContents = "";
    QString cbContentsUnformatted = "";

    QClipboard *cb = QApplication::clipboard();
    if (cb->mimeData(QClipboard::Clipboard)->hasText()) {
        cbContentsUnformatted = cb->mimeData(QClipboard::Clipboard)->text();
        if (cb->mimeData(QClipboard::Clipboard)->hasHtml())
            cbContents = cb->mimeData(QClipboard::Clipboard)->html();
        else
            cbContents = cb->mimeData(QClipboard::Clipboard)->text();
    }

    if (plainpre && !cbContentsUnformatted.isEmpty()) {
        res=cbContentsUnformatted;
        if (res.indexOf("\r\n")>=0)
            res.replace("\r\n","</p><p>");
        else
            res.replace("\n","</p><p>");
        res = "<p>"+res+"</p>";
        res = makeSimpleHtml("<...>",res);
    } else {
        if (noFormatting) {
            if (!cbContentsUnformatted.isEmpty())
                res = cbContentsUnformatted;
        } else {
            if (!cbContents.isEmpty())
                res = cbContents;
            else if (!cbContentsUnformatted.isEmpty())
                res = makeSimpleHtml("<...>",cbContentsUnformatted);
        }
    }
    return res;
}

QString fixMetaEncoding(const QString &data_utf8)
{
    int pos;
    QString header = data_utf8.left(512);
    QString dt = data_utf8;
    dt.remove(0,header.length());
    bool forceMeta = true;
    if ((pos = header.indexOf(QRegExp("http-equiv {0,}=",Qt::CaseInsensitive))) != -1) {
        if ((pos = header.lastIndexOf("<meta ", pos, Qt::CaseInsensitive)) != -1) {
            QRegExp rxcs("charset {0,}=",Qt::CaseInsensitive);
            pos = rxcs.indexIn(header,pos) + rxcs.matchedLength();
            if (pos > -1) {
                int pos2 = header.indexOf(QRegExp("['\"]",Qt::CaseInsensitive), pos+1);
                header.replace(pos, pos2-pos,"UTF-8");
                forceMeta = false;
            }
        }
    }
    if (forceMeta && ((pos = header.indexOf(QRegExp("</head",Qt::CaseInsensitive))) != -1)) {
        header.insert(pos,"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
    }
    dt = header+dt;
    return dt;
}

QString wordWrap(const QString &str, int wrapLength)
{
    QStringList stl = str.split(' ');
    QString ret = "";
    int cnt = 0;
    for (int i=0;i<stl.count();i++) {
        ret += stl.at(i) + " ";
        cnt += stl.at(i).length()+1;
        if (cnt>wrapLength) {
            ret += '\n';
            cnt = 0;
        }
    }
    stl.clear();
    return ret;
}

QString getOpenFileNameD (QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter)
{
    QFileDialog::Options opts = 0;
    if (gSet->settings.dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    QFileDialog dialog(parent,caption,dir,filter);
    dialog.setOptions(opts);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (openFileDialogSize.isValid())
        dialog.resize(openFileDialogSize);

    QString res;
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() == QDialog::Accepted) {
        if (selectedFilter)
            *selectedFilter = dialog.selectedNameFilter();
        if (!dialog.selectedFiles().isEmpty())
            res = dialog.selectedFiles().first();
    }

    openFileDialogSize = dialog.size();
    return res;
}

QStringList getOpenFileNamesD (QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter)
{
    QFileDialog::Options opts = 0;
    if (gSet->settings.dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    QFileDialog dialog(parent,caption,dir,filter);
    dialog.setOptions(opts);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    if (openFileDialogSize.isValid())
        dialog.resize(openFileDialogSize);

    QStringList res;
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() == QDialog::Accepted) {
        if (selectedFilter)
            *selectedFilter = dialog.selectedNameFilter();
         res = dialog.selectedFiles();
    }

    openFileDialogSize = dialog.size();
    return res;
}

QStringList getSuffixesFromFilter(const QString& filter)
{
    QStringList res;
    res.clear();
    if (filter.isEmpty()) return res;

    QStringList filters = filter.split(";;",QString::SkipEmptyParts);
    if (filters.isEmpty()) return res;

    for (int i=0;i<filters.count();i++) {
        QString ex = filters.at(i);

        if (ex.isEmpty()) continue;

        ex.remove(QRegExp("^.*\\("));
        ex.remove(QRegExp("\\).*$"));
        ex.remove(QRegExp("^.*\\."));
        res.append(ex.split(" "));
    }

    return res;
}

QString getSaveFileNameD (QWidget * parent, const QString & caption, const QString & dir, const QString & filter, QString * selectedFilter, QString preselectFileName )
{
    QFileDialog::Options opts = 0;
    if (gSet->settings.dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    QFileDialog dialog(parent,caption,dir,filter);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOptions(opts);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (saveFileDialogSize.isValid())
        dialog.resize(saveFileDialogSize);

    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);

    if (!preselectFileName.isEmpty())
        dialog.selectFile(preselectFileName);

    QString res;
    if (dialog.exec()==QDialog::Accepted) {
        QString userFilter = dialog.selectedNameFilter();
        if (selectedFilter!=nullptr)
            *selectedFilter=userFilter;
        if (!userFilter.isEmpty())
            dialog.setDefaultSuffix(getSuffixesFromFilter(userFilter).first());

        if (!dialog.selectedFiles().isEmpty())
            res = dialog.selectedFiles().first();
    }

    saveFileDialogSize = dialog.size();
    return res;
}

QString	getExistingDirectoryD ( QWidget * parent, const QString & caption, const QString & dir, QFileDialog::Options options )
{
    QFileDialog::Options opts = options;
    if (gSet->settings.dontUseNativeFileDialog)
        opts = QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons;

    return QFileDialog::getExistingDirectory(parent,caption,dir,opts);
}

QList<QStringList> encodingsByScript()
{
    QList<QStringList> enc;

    enc << (QStringList() << "Western European" << "ISO 8859-1" << "ISO 8859-15" <<
            "ISO 8859-14" << "cp 1252" << "CP850" << "x-MacRoman");
    enc << (QStringList() << "Central European" << "ISO 8859-2" << "ISO 8859-3" << "cp 1250"
            << "x-MacCentralEurope");
    enc << (QStringList() << "Baltic" << "ISO 8859-4" << "ISO 8859-13" << "cp 1257");
    enc << (QStringList() << "Turkish" << "cp 1254" << "ISO 8859-9" << "x-MacTurkish");
    enc << (QStringList() << "Cyrillic" << "KOI8-R" << "ISO 8859-5" << "cp 1251" << "KOI8-U"
            << "CP866" << "x-MacCyrillic");
    enc << (QStringList() << "Chinese" << "HZ" << "GBK" << "GB18030" << "BIG5" << "BIG5-HKSCS"
            << "ISO-2022-CN" << "ISO-2022-CN-EXT");
    enc << (QStringList() << "Korean" << "CP949" << "ISO-2022-KR");
    enc << (QStringList() << "Japanese" << "EUC-JP" << "SHIFT_JIS" << "ISO-2022-JP"
            << "ISO-2022-JP-2" << "ISO-2022-JP-1");
    enc << (QStringList() << "Greek" << "ISO 8859-7" << "cp 1253" << "x-MacGreek");
    enc << (QStringList() << "Arabic" << "ISO 8859-6" << "cp 1256");
    enc << (QStringList() << "Hebrew" << "ISO 8859-8" << "cp 1255" << "CP862");
    enc << (QStringList() << "Thai" << "CP874");
    enc << (QStringList() << "Vietnamese" << "CP1258");
    enc << (QStringList() << "Unicode" << "UTF-8" << "UTF-7" << "UTF-16" << "UTF-16BE" << "UTF-16LE"
            << "UTF-32" << "UTF-32BE" << "UTF-32LE");
    enc << (QStringList() << "Other" << "windows-1258" << "IBM874" << "TSCII" << "Macintosh");
    return enc;
}

QString bool2str(bool value)
{
    if (value)
        return QString("on");
    else
        return QString("off");
}

QString bool2str2(bool value)
{
    if (value)
        return QString("TRUE");
    else
        return QString("FALSE");
}

QString formatBytes(qint64 sz) {
    QString s;
    double msz;
    if (sz<1024.0) {
        msz=sz;
        s="b";
    } else if (sz<(1024.0 * 1024.0)) {
        msz=sz/1024.0;
        s="Kb";
    } else if (sz<(1024.0 * 1024.0 * 1024.0)) {
        msz=sz/(1024.0*1024.0);
        s="Mb";
    } else if (sz<(1024.0 * 1024.0 * 1024.0 * 1024.0)) {
        msz=sz/(1024.0 * 1024.0 * 1024.0);
        s="Gb";
    } else {
        msz=sz/(1024.0 * 1024.0 * 1024.0 * 1024.0);
        s="Tb";
    }
    s=QString("%1").arg(msz,0,'f',2)+" "+s;
    return s;
}

QString getTmpDir()
{
    QFileInfo fi(QDir::homePath() + QDir::separator() + "tmp");
    if (fi.exists() && fi.isDir() && fi.isWritable())
        return QDir::homePath() + QDir::separator() + "tmp";

    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

int compareStringLists(const QStringList &left, const QStringList &right)
{
    int diff = left.count()-right.count();
    if (diff!=0) return diff;

    for (int i=0;i<left.count();i++) {
        diff = QString::compare(left.at(i),right.at(i));
        if (diff!=0) return diff;
    }

    return 0;
}

void generateHTML(const CHTMLNode &src, QString &html, bool reformat, int depth)
{
    if (src.isTag && !src.tagName.isEmpty()) {
        html.append("<"+src.tagName);
        for (int i=0;i<src.attributesOrder.count();i++) {
            QString key = src.attributesOrder.at(i);
            QString val = src.attributes.value(key);
            if (!val.contains("\""))
                html.append(QString(" %1=\"%2\"").arg(key,val));
            else
                html.append(QString(" %1='%2'").arg(key,val));
        }
        html.append(">");
    } else {
        QString txt = src.text;
        // fix incorrect nested comments - pixiv
        if (src.isComment) {
            if ((txt.count("<!--")>1) || (txt.count("-->")>1))
                txt.clear();
            if ((txt.count("<")>1 || txt.count(">")>1) && (txt.count("<")!=txt.count(">")))
                txt.clear();
        }
        html.append(txt);
    }

    for (int i=0; i<src.children.count(); i++ )
        generateHTML(src.children.at(i),html,reformat,depth+1);

    html.append(src.closingText);
    if (reformat)
        html.append("\n");
}

QString extractFileTitle(const QString& fileContents)
{
    int pos;
    int start = -1;
    int stop = -1;
    if ((pos = fileContents.indexOf(QRegExp("<title {0,}>",Qt::CaseInsensitive))) != -1) {
        start = pos;
        if ((pos = fileContents.indexOf(QRegExp("</title {0,}>", Qt::CaseInsensitive))) != -1) {
            stop = pos;
            if (stop>start) {
                if ((stop-start)>255)
                    stop = start + 255;
                QString s = fileContents.mid(start,stop-start);
                s.remove(QRegExp("^<title {0,}>",Qt::CaseInsensitive));
                s.remove("\r");
                s.remove("\n");
                return s;
            }
        }
    }
    return QString();
}

// for url rules
QString convertPatternToRegExp(const QString &wildcardPattern) {
    QString pattern = wildcardPattern;
    return pattern.replace(QRegExp(QLatin1String("\\*+")), QLatin1String("*"))   // remove multiple wildcards
            .replace(QRegExp(QLatin1String("\\^\\|$")), QLatin1String("^"))        // remove anchors following separator placeholder
            .replace(QRegExp(QLatin1String("^(\\*)")), QLatin1String(""))          // remove leading wildcards
            .replace(QRegExp(QLatin1String("(\\*)$")), QLatin1String(""))          // remove trailing wildcards
            .replace(QRegExp(QLatin1String("(\\W)")), QLatin1String("\\\\1"))      // escape special symbols
            .replace(QRegExp(QLatin1String("^\\\\\\|\\\\\\|")),
                     QLatin1String("^[\\w\\-]+:\\/+(?!\\/)(?:[^\\/]+\\.)?"))       // process extended anchor at expression start
            .replace(QRegExp(QLatin1String("\\\\\\^")),
                     QLatin1String("(?:[^\\w\\d\\-.%]|$)"))                        // process separator placeholders
            .replace(QRegExp(QLatin1String("^\\\\\\|")), QLatin1String("^"))       // process anchor at expression start
            .replace(QRegExp(QLatin1String("\\\\\\|$")), QLatin1String("$"))       // process anchor at expression end
            .replace(QRegExp(QLatin1String("\\\\\\*")), QLatin1String(".*"))       // replace wildcards by .*
            ;
}

void sendStringToWidget(QWidget *widget, const QString& s)
{
    if (widget==nullptr) return;
    for (int i=0;i<s.length();i++) {
        Qt::KeyboardModifiers mod = Qt::NoModifier;
        const QChar c = s.at(i);
        if (c.isLetter() && c.isUpper()) mod = Qt::ShiftModifier;
        QTest::sendKeyEvent(QTest::KeyAction::Click,widget,Qt::Key_A,c,mod);
    }
}

void sendKeyboardInputToView(QWidget *widget, const QString& s)
{
    if (widget==nullptr) return;

    widget->setFocus();
    Q_FOREACH(QObject* obj, widget->children()) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        sendStringToWidget(w,s);
    }
    sendStringToWidget(widget,s);
}
