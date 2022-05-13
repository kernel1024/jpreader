#ifndef CPROMTONEFREETRANSLATOR_H
#define CPROMTONEFREETRANSLATOR_H

#include <QObject>
#include "abstracttranslator.h"

class CPromtOneFreeTranslator : public CAbstractTranslator
{
    Q_OBJECT
    Q_DISABLE_COPY(CPromtOneFreeTranslator)

private:
    int m_failedQueriesCounter { 0 };
    QString tranStringInternal(const QString& src);
    QString promtLangCode(const QString& bcp47name) const;

public:
    CPromtOneFreeTranslator(QObject *parent, const CLangPair& lang);
    ~CPromtOneFreeTranslator() override;

    bool initTran() override;
    QString tranStringPrivate(const QString& src) override;
    void doneTranPrivate(bool lazyClose) override;
    bool isReady() override;
    CStructures::TranslationEngine engine() override;

};

#endif // CPROMTONEFREETRANSLATOR_H
