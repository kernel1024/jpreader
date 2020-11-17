#ifndef CLIWORKER_H
#define CLIWORKER_H

#include <QObject>
#include <QTextStream>
#include "global/structures.h"
#include "translator-workers/abstracttranslator.h"

class CCLIWorker : public QObject
{
    Q_OBJECT
private:
    CLangPair m_lang;
    QPointer<CAbstractTranslator> m_engine;
    QStringList m_preloadedText;
    QString m_error;
    QTextStream *m_in;
    QTextStream *m_out;
    QTextStream *m_err;

    bool m_subsentences { false };

    QString translate(const QString& text);
    QString translatePriv(const QString& text);
    void translateStrings(const QStringList& list);

public:
    explicit CCLIWorker(QObject *parent = nullptr);
    ~CCLIWorker() override;
    bool parseArguments();

public Q_SLOTS:
    void start();

Q_SIGNALS:
    void finished();

};

#endif // CLIWORKER_H
