#ifndef GENERICFUNCS_H
#define GENERICFUNCS_H

#include <QtCore>
#include <QtGui>
#include <QtXml>

#define UNUSED(x) (void)(x)

QString detectMIME(QString filename);
QString detectMIME(QByteArray buf);
QTextCodec* detectEncoding(QByteArray content);
QString detectEncodingName(QByteArray content);
QString makeSimpleHtml(QString title, QString content);
QByteArray XMLizeHTML(QByteArray html, QString encoding = QString("UTF-8"));
QString getClipboardContent(bool noFormatting = false, bool plainpre = false);
QString fixMetaEncoding(QString data_utf8);
QString wordWrap(const QString &str, int wrapLength);
QList<QStringList> encodingsByScript();

QString getOpenFileNameD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), const QString & filter = QString(), QString * selectedFilter = 0, QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QStringList getOpenFileNamesD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), const QString & filter = QString(), QString * selectedFilter = 0, QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QString getSaveFileNameD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), const QString & filter = QString(), QString * selectedFilter = 0, QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QString	getExistingDirectoryD ( QWidget * parent = 0, const QString & caption = QString(), const QString & dir = QString(), QFileDialog::Options options = QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);

#endif // GENERICFUNCS_H
