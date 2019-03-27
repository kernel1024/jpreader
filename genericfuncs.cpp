#include <QTextCodec>
#include <QStringList>
#include <QMimeData>
#include <QString>
#include <QTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTcpServer>
#include <QRegExp>
#include <QtTest>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "genericfuncs.h"
#include "globalcontrol.h"

extern "C" {
#include <unistd.h>
#include <magic.h>
#include <unicode/utypes.h>
#include <unicode/localpointer.h>
#include <unicode/uenum.h>
#include <unicode/ucsdet.h>
#include <syslog.h>
}

static QSize openFileDialogSize = QSize();
static QSize saveFileDialogSize = QSize();

bool runnedFromQtCreator()
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
    QString file(context.file);
    QString category(context.category);
    if (category==QStringLiteral("default"))
        category.clear();
    else
        category.append(' ');
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
        QString fmsg = QStringLiteral("%1 %2").arg(QTime::currentTime().toString("h:mm:ss"),lmsg);
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

        if (gSet && gSet->logWindow)
            QMetaObject::invokeMethod(gSet->logWindow,&CLogDisplay::updateMessages);
    }

    loggerMutex.unlock();
}

bool checkAndUnpackUrl(QUrl& url)
{
    // Extract jump url for pixiv
    if (url.host().endsWith(QStringLiteral("pixiv.net")) && url.path().startsWith(QStringLiteral("/jump"))) {
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
        return QStringLiteral("text/plain");
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
        return QStringLiteral("text/plain");
    }
    QString mag(mg);
    magic_close(myt);
    return mag;
}

QString detectEncodingName(const QByteArray& content) {
    QString codepage;

    if (!gSet->settings.forcedCharset.isEmpty() &&
            QTextCodec::availableCodecs().contains(gSet->settings.forcedCharset.toLatin1()))
        return gSet->settings.forcedCharset;

    QTextCodec* enc = QTextCodec::codecForLocale();
    QByteArray icu_enc;
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector* csd = ucsdet_open(&status);
    ucsdet_setText(csd, content.constData(), content.length(), &status);
    const UCharsetMatch *ucm;
    ucm = ucsdet_detect(csd, &status);
    if (status==U_ZERO_ERROR && ucm) {
        const char* cname = ucsdet_getName(ucm,&status);
        if (status==U_ZERO_ERROR) icu_enc = QByteArray(cname);
    }
    if (QTextCodec::availableCodecs().contains(icu_enc)) {
        enc = QTextCodec::codecForName(icu_enc);
    }
    ucsdet_close(csd);

    codepage = QTextCodec::codecForHtml(content,enc)->name();
    if (codepage.contains(QStringLiteral("x-sjis"),Qt::CaseInsensitive)) codepage=QStringLiteral("SJIS");
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


QString makeSimpleHtml(const QString &title, const QString &content,
                       bool integratedTitle, const QUrl& origin)
{
    QString s = content;
    QString cnt = s.replace(QRegExp(QStringLiteral("\n{3,}")),QStringLiteral("\n\n"))
            .replace(QStringLiteral("\n"),QStringLiteral("<br />\n"));
    QString cn(QStringLiteral("<html><head><META HTTP-EQUIV=\"Content-type\" "
                              "CONTENT=\"text/html; charset=UTF-8;\">"));
    cn.append(QStringLiteral("<title>%1</title></head><body>").arg(title));
    if (integratedTitle) {
        cn.append(QStringLiteral("<h3>"));
        if (!origin.isEmpty())
            cn.append(QStringLiteral("<a href=\"%1\">%2</a>")
                      .arg(origin.toString(QUrl::FullyEncoded),title));
        cn.append(QStringLiteral("</h3>"));
    }
    cn.append(cnt);
    cn.append(QStringLiteral("</body></html>"));
    return cn;
}

QString getClipboardContent(bool noFormatting, bool plainpre) {
    QString res;
    QString cbContents;
    QString cbContentsUnformatted;

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
        if (res.indexOf(QStringLiteral("\r\n"))>=0)
            res.replace(QStringLiteral("\r\n"),QStringLiteral("</p><p>"));
        else
            res.replace(QStringLiteral("\n"),QStringLiteral("</p><p>"));
        res = QStringLiteral("<p>%1</p>").arg(res);
        res = makeSimpleHtml(QStringLiteral("<...>"),res);
    } else {
        if (noFormatting) {
            if (!cbContentsUnformatted.isEmpty())
                res = cbContentsUnformatted;
        } else {
            if (!cbContents.isEmpty())
                res = cbContents;
            else if (!cbContentsUnformatted.isEmpty())
                res = makeSimpleHtml(QStringLiteral("<...>"),cbContentsUnformatted);
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
    if ((pos = header.indexOf(QRegExp(QStringLiteral("http-equiv {0,}="),Qt::CaseInsensitive))) != -1) {
        if ((pos = header.lastIndexOf(QStringLiteral("<meta "), pos, Qt::CaseInsensitive)) != -1) {
            QRegExp rxcs(QStringLiteral("charset {0,}="),Qt::CaseInsensitive);
            pos = rxcs.indexIn(header,pos) + rxcs.matchedLength();
            if (pos > -1) {
                int pos2 = header.indexOf(QRegExp(QStringLiteral("['\"]"),Qt::CaseInsensitive), pos+1);
                header.replace(pos, pos2-pos,QStringLiteral("UTF-8"));
                forceMeta = false;
            }
        }
    }
    if (forceMeta && ((pos = header.indexOf(QRegExp(QStringLiteral("</head"),Qt::CaseInsensitive))) != -1)) {
        header.insert(pos,QStringLiteral("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"));
    }
    dt = header+dt;
    return dt;
}

QString wordWrap(const QString &str, int wrapLength)
{
    QStringList stl = str.split(' ');
    QString ret;
    int cnt = 0;
    for (int i=0;i<stl.count();i++) {
        ret += stl.at(i) + ' ';
        cnt += stl.at(i).length()+1;
        if (cnt>wrapLength) {
            ret += '\n';
            cnt = 0;
        }
    }
    stl.clear();
    return ret;
}

QString formatSize(const QString& size)
{
    bool ok;
    qint64 sz = size.toLong(&ok);
    if (ok)
        return formatSize(sz);

    return size;
}

QString formatSize(qint64 size)
{
    if (size<1024) return QStringLiteral("%1 bytes").arg(size);
    if (size<1024*1024) return QStringLiteral("%1 Kb").arg(size/1024);
    if (size<1024*1024*1024) return QStringLiteral("%1 Mb").arg(size/(1024*1024));
    return QStringLiteral("%1 Gb").arg(size/(1024*1024*1024));
}

QString elideString(const QString& text, int maxlen)
{
    if (text.length()<maxlen)
        return text;

    return QStringLiteral("%1...").arg(text.left(maxlen));
}

QString highlightSnippet(const QString& snippet, const QStringList& terms)
{
    QString res = snippet;

    for (int i=0;i<terms.count();i++) {
        QString ITerm = terms.value(i).trimmed();
        QString snpColor = CSettings::snippetColors[i % CSettings::snippetColors.count()].name();
        int start = 0;
        int pos;
        while ((pos = res.indexOf(ITerm,start,Qt::CaseInsensitive))>=0) {
            QString tag = QStringLiteral("<font color='%1'>").arg(snpColor);
            int pos2 = pos + tag.length() + ITerm.length();
            res.insert(pos,tag);
            res.insert(pos2,QStringLiteral("</font>"));
            start = pos2;
        }
    }
    return res;
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

    QStringList filters = filter.split(QStringLiteral(";;"),QString::SkipEmptyParts);
    if (filters.isEmpty()) return res;

    for (int i=0;i<filters.count();i++) {
        QString ex = filters.at(i);

        if (ex.isEmpty()) continue;

        ex.remove(QRegExp(QStringLiteral("^.*\\(")));
        ex.remove(QRegExp(QStringLiteral("\\).*$")));
        ex.remove(QRegExp(QStringLiteral("^.*\\.")));
        res.append(ex.split(' '));
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
        if (selectedFilter)
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

QVector<QStringList> encodingsByScript()
{
    const static QVector<QStringList> enc = {
        { "Western European", "ISO 8859-1", "ISO 8859-15", "ISO 8859-14", "cp 1252", "CP850", "x-MacRoman" },
        { "Central European", "ISO 8859-2", "ISO 8859-3", "cp 1250", "x-MacCentralEurope" },
        { "Baltic", "ISO 8859-4", "ISO 8859-13", "cp 1257" },
        { "Turkish", "cp 1254", "ISO 8859-9", "x-MacTurkish" },
        { "Cyrillic", "KOI8-R", "ISO 8859-5", "cp 1251", "KOI8-U", "CP866", "x-MacCyrillic" },
        { "Chinese", "HZ", "GBK", "GB18030", "BIG5", "BIG5-HKSCS", "ISO-2022-CN", "ISO-2022-CN-EXT" },
        { "Korean", "CP949", "ISO-2022-KR" },
        { "Japanese", "EUC-JP", "SHIFT_JIS", "ISO-2022-JP", "ISO-2022-JP-2", "ISO-2022-JP-1" },
        { "Greek", "ISO 8859-7", "cp 1253", "x-MacGreek" },
        { "Arabic", "ISO 8859-6", "cp 1256" },
        { "Hebrew", "ISO 8859-8", "cp 1255", "CP862" },
        { "Thai", "CP874" },
        { "Vietnamese", "CP1258" },
        { "Unicode", "UTF-8", "UTF-7", "UTF-16", "UTF-16BE", "UTF-16LE", "UTF-32", "UTF-32BE", "UTF-32LE" },
        { "Other", "windows-1258", "IBM874", "TSCII", "Macintosh" }
    };

    return enc;
}

QString getTmpDir()
{
    QFileInfo fi(QDir::homePath() + QDir::separator() + QStringLiteral("tmp"));
    if (fi.exists() && fi.isDir() && fi.isWritable())
        return QDir::homePath() + QDir::separator() + QStringLiteral("tmp");

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
        html.append(QStringLiteral("<")+src.tagName);
        for (const QString &key : qAsConst(src.attributesOrder)) {
            const QString val = src.attributes.value(key);
            if (!val.contains('"'))
                html.append(QStringLiteral(" %1=\"%2\"").arg(key,val));
            else
                html.append(QStringLiteral(" %1='%2'").arg(key,val));
        }
        html.append(QStringLiteral(">"));
    } else {
        QString txt = src.text;
        // fix incorrect nested comments - pixiv
        if (src.isComment) {
            if ((txt.count(QStringLiteral("<!--"))>1) || (txt.count(QStringLiteral("-->"))>1))
                txt.clear();
            if ((txt.count(QStringLiteral("<"))>1 || txt.count(QStringLiteral(">"))>1)
                && (txt.count(QStringLiteral("<"))!=txt.count(QStringLiteral(">"))))
                txt.clear();
        }
        html.append(txt);
    }

    for (const CHTMLNode &node : qAsConst(src.children))
        generateHTML(node,html,reformat,depth+1);

    html.append(src.closingText);
    if (reformat)
        html.append('\n');
}

QString extractFileTitle(const QString& fileContents)
{
    int pos;
    int start = -1;
    int stop = -1;
    if ((pos = fileContents.indexOf(QRegExp(QStringLiteral("<title {0,}>"),Qt::CaseInsensitive))) != -1) {
        start = pos;
        if ((pos = fileContents.indexOf(QRegExp(QStringLiteral("</title {0,}>"), Qt::CaseInsensitive))) != -1) {
            stop = pos;
            if (stop>start) {
                if ((stop-start)>255)
                    stop = start + 255;
                QString s = fileContents.mid(start,stop-start);
                s.remove(QRegExp(QStringLiteral("^<title {0,}>"),Qt::CaseInsensitive));
                s.remove('\r');
                s.remove('\n');
                return s;
            }
        }
    }
    return QString();
}

// for url rules
QString convertPatternToRegExp(const QString &wildcardPattern) {
    QString pattern = wildcardPattern;
    return pattern.replace(QRegExp(QStringLiteral("\\*+")), QStringLiteral("*"))   // remove multiple wildcards
            .replace(QRegExp(QStringLiteral("\\^\\|$")), QStringLiteral("^"))        // remove anchors following separator placeholder
            .replace(QRegExp(QStringLiteral("^(\\*)")), QString())          // remove leading wildcards
            .replace(QRegExp(QStringLiteral("(\\*)$")), QString())          // remove trailing wildcards
            .replace(QRegExp(QStringLiteral("(\\W)")), QStringLiteral("\\\\1"))      // escape special symbols
            .replace(QRegExp(QStringLiteral("^\\\\\\|\\\\\\|")),
                     QStringLiteral("^[\\w\\-]+:\\/+(?!\\/)(?:[^\\/]+\\.)?"))       // process extended anchor at expression start
            .replace(QRegExp(QStringLiteral("\\\\\\^")),
                     QStringLiteral("(?:[^\\w\\d\\-.%]|$)"))                        // process separator placeholders
            .replace(QRegExp(QStringLiteral("^\\\\\\|")), QStringLiteral("^"))       // process anchor at expression start
            .replace(QRegExp(QStringLiteral("\\\\\\|$")), QStringLiteral("$"))       // process anchor at expression end
            .replace(QRegExp(QStringLiteral("\\\\\\*")), QStringLiteral(".*"))       // replace wildcards by .*
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
    for (QObject *obj : widget->children()) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w)
            sendStringToWidget(w,s);
    }
    sendStringToWidget(widget,s);
}
