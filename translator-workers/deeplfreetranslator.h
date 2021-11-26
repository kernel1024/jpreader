#ifndef CDEEPLFREETRANSLATOR_H
#define CDEEPLFREETRANSLATOR_H

#include <QObject>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QScopedPointer>
#include "abstracttranslator.h"

class CDeeplFreeTranslator : public CAbstractTranslator
{
    Q_OBJECT
    Q_DISABLE_COPY(CDeeplFreeTranslator)

private:
    int m_failedQueriesCounter { 0 };
    QString tranStringInternal(const QString& src);

public:
    CDeeplFreeTranslator(QObject *parent, const CLangPair& lang);
    ~CDeeplFreeTranslator() override;

    bool initTran() override;
    QString tranStringPrivate(const QString& src) override;
    void doneTranPrivate(bool lazyClose) override;
    bool isReady() override;
    CStructures::TranslationEngine engine() override;

};

#endif // CDEEPLFREETRANSLATOR_H
