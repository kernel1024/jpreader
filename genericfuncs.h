#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QWidget>
#include <QFileDialog>

#define UNUSED(x) (void)(x)

extern QStringList debugMessages;

void stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

QString detectMIME(QString filename);
QString detectMIME(QByteArray buf);
QTextCodec* detectEncoding(QByteArray content);
QString detectEncodingName(QByteArray content);
QString makeSimpleHtml(QString title, QString content);
QString getClipboardContent(bool noFormatting = false, bool plainpre = false);
QString fixMetaEncoding(QString data_utf8);
QString wordWrap(const QString &str, int wrapLength);
QList<QStringList> encodingsByScript();
QString bool2str(bool value);
QString bool2str2(bool value);
QString formatBytes(qint64 sz);

QString getOpenFileNameD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), const QString & filter = QString(), QString * selectedFilter = 0, QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QStringList getOpenFileNamesD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), const QString & filter = QString(), QString * selectedFilter = 0, QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QString getSaveFileNameD (QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), const QString & filter = QString(), QString * selectedFilter = 0, QFileDialog::Options options = QFileDialog::DontUseNativeDialog , QString preselectFileName = QString());
QString	getExistingDirectoryD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), QFileDialog::Options options = QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);

#endif // GENERICFUNCS_H
