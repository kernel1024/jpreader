#ifndef TRANSLATORSTATISTICSTAB_H
#define TRANSLATORSTATISTICSTAB_H

#include <QWidget>
#include "global/structures.h"
#include "utils/specwidgets.h"
#include "qcustomplot/qcustomplot.h"

namespace Ui {
class CTranslatorStatisticsTab;
}

class CTranslatorStatisticsTab : public CSpecTabContainer
{
    Q_OBJECT
private:
    double m_dayWidth { 0.0 };
    QDate m_minimalDate;
    Ui::CTranslatorStatisticsTab *ui;
    Q_DISABLE_COPY(CTranslatorStatisticsTab)

    double setGraphData(CStructures::TranslationEngine engine, QCPGraph *graph,
                        const QDate &start, const QDate &end, CStructures::DateRange range);
    double dateToTick(const QDate &date) const;

public:
    explicit CTranslatorStatisticsTab(QWidget *parent = nullptr);
    ~CTranslatorStatisticsTab() override;

    static CTranslatorStatisticsTab* instance();
    QString getTimeRangeString() const;

public Q_SLOTS:
    void updateGraph();
    void radioRangeChanged(bool checked);
    void save();

};

class CCPAxisTickerSize : public QCPAxisTicker
{
    Q_GADGET

public:
    CCPAxisTickerSize() = default;

protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override;
};

#endif // TRANSLATORSTATISTICSTAB_H
