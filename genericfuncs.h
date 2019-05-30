#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QStringList>
#include <QWidget>
#include <QFileDialog>

const int httpCodeFound = 200;
const int httpCodeRedirect = 300;
const int httpCodeClientError = 400;
const int httpCodeServerError = 500;

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
QString formatFileSize(qint64 size);
QString formatFileSize(const QString& size);
QString elideString(const QString& text, int maxlen, Qt::TextElideMode mode = Qt::ElideRight);
QString highlightSnippet(const QString& snippet, const QStringList& terms);
const QVector<QStringList> &encodingsByScript();
const QStringList &getSupportedImageExtensions();
QString getTmpDir();
bool checkAndUnpackUrl(QUrl& url);
int getRandomTCPPort();
bool runnedFromQtCreator();
int compareStringLists(const QStringList& left, const QStringList& right);
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
                          QString * selectedFilter = nullptr, const QString & preselectFileName = QString());

QString	getExistingDirectoryD ( QWidget * parent = nullptr, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);


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
    const int base = 10;
    const int minusBase = ((-1)*base)+1;

    if ((n >= 0) && (n < base))
        return 1;

    if ((n >= minusBase) && (n < 0))
        return 2;

    if (n<0)
        return 2 + numDigits(abs(n) / base);

    return 1 + numDigits(n / base);
}

#endif // GENERICFUNCS_H
