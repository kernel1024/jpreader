#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QStringList>
#include <QWidget>
#include <QFileDialog>
#include "translator.h"

extern QStringList debugMessages;

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

QString detectMIME(const QString& filename);
QString detectMIME(const QByteArray& buf);
QTextCodec* detectEncoding(const QByteArray &content);
QString detectEncodingName(const QByteArray &content);
QString detectDecodeToUnicode(const QByteArray &content);
QString makeSimpleHtml(const QString& title, const QString& content, bool integratedTitle = false, const QUrl &origin = QUrl());
QString getClipboardContent(bool noFormatting = false, bool plainpre = false);
QString fixMetaEncoding(const QString& data_utf8);
QString wordWrap(const QString &str, int wrapLength);
QString formatSize(qint64 size);
QString formatSize(const QString& size);
QString elideString(const QString& text, int maxlen);
QString highlightSnippet(const QString& snippet, const QStringList& terms);
QVector<QStringList> encodingsByScript();
QString getTmpDir();
bool checkAndUnpackUrl(QUrl& url);
int getRandomTCPPort();
bool runnedFromQtCreator();
int compareStringLists(const QStringList& left, const QStringList& right);
void generateHTML(const CHTMLNode &src, QString &html, bool reformat = false, int depth = 0);
QString extractFileTitle(const QString& fileContents);
QString convertPatternToRegExp(const QString &wildcardPattern);
void sendKeyboardInputToView(QWidget *widget, const QString& s);

QString getOpenFileNameD ( QWidget * parent = nullptr, const QString & caption = QString(),
                           const QString & dir = QString(), const QString & filter = QString(),
                           QString * selectedFilter = nullptr);

QStringList getOpenFileNamesD ( QWidget * parent = nullptr, const QString & caption = QString(),
                               const QString & dir = QString(), const QString & filter = QString(),
                               QString * selectedFilter = nullptr);

QString getSaveFileNameD (QWidget * parent = nullptr, const QString & caption = QString(),
                          const QString & dir = QString(), const QString & filter = QString(),
                          QString * selectedFilter = nullptr, QString preselectFileName = QString());

QString	getExistingDirectoryD ( QWidget * parent = nullptr, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);


inline QStringList getSupportedImageExtensions()
{
    static const QStringList supportedImageExtensions { "jpeg", "jpg", "jpe", "gif", "bmp", "png" };
    return supportedImageExtensions;
}


inline QString bool2str(bool value)
{
    if (value)
        return QStringLiteral("on");

    return QStringLiteral("off");
}

inline QString bool2str2(bool value)
{
    if (value)
        return QStringLiteral("TRUE");

    return QStringLiteral("FALSE");
}

inline int numDigits(const int n) {
    if ((n >= 0) && (n < 10))
        return 1;

    if ((n >= -9) && (n < 0))
        return 2;

    if (n<0)
        return 2 + numDigits(abs(n) / 10);

    return 1 + numDigits(n / 10);
}

inline QString formatBytes(qint64 sz) {
    QString s;
    double msz;
    if (sz<1024.0) {
        msz=sz;
        s=QStringLiteral("b");
    } else if (sz<(1024.0 * 1024.0)) {
        msz=sz/1024.0;
        s=QStringLiteral("Kb");
    } else if (sz<(1024.0 * 1024.0 * 1024.0)) {
        msz=sz/(1024.0*1024.0);
        s=QStringLiteral("Mb");
    } else if (sz<(1024.0 * 1024.0 * 1024.0 * 1024.0)) {
        msz=sz/(1024.0 * 1024.0 * 1024.0);
        s=QStringLiteral("Gb");
    } else {
        msz=sz/(1024.0 * 1024.0 * 1024.0 * 1024.0);
        s=QStringLiteral("Tb");
    }
    s=QStringLiteral("%1 %2").arg(msz,0,'f',2).arg(s);
    return s;
}

#endif // GENERICFUNCS_H
