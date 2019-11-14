#include <QMutex>
#include <QSharedPointer>
#include "settings.h"
#include "globalcontrol.h"
#include "translatorstatisticstab.h"
#include "ui_translatorstatisticstab.h"

CTranslatorStatisticsTab::CTranslatorStatisticsTab(QWidget *parent) :
    CSpecTabContainer(parent),
    ui(new Ui::CTranslatorStatisticsTab)
{
    ui->setupUi(this);

    m_dayWidth = (dateToTick(QDate::currentDate().addDays(1)) - dateToTick(QDate::currentDate()));

    setTabTitle(tr("Translator statistics"));

    ui->radioRangeWeek->setChecked(true);

    connect(ui->radioRangeWeek, &QRadioButton::toggled, this, &CTranslatorStatisticsTab::radioRangeChanged);
    connect(ui->radioRangeMonth, &QRadioButton::toggled, this, &CTranslatorStatisticsTab::radioRangeChanged);
    connect(ui->radioRangeThisMonth, &QRadioButton::toggled, this, &CTranslatorStatisticsTab::radioRangeChanged);
    connect(ui->radioRangeYear, &QRadioButton::toggled, this, &CTranslatorStatisticsTab::radioRangeChanged);
    connect(ui->radioRangeAll, &QRadioButton::toggled, this, &CTranslatorStatisticsTab::radioRangeChanged);

    connect(gSet, &CGlobalControl::translationStatisticsChanged, this, &CTranslatorStatisticsTab::updateGraph);
}

CTranslatorStatisticsTab::~CTranslatorStatisticsTab()
{
    delete ui;
}

CTranslatorStatisticsTab *CTranslatorStatisticsTab::instance()
{
    static CTranslatorStatisticsTab* inst = nullptr;
    static QMutex lock;
    QMutexLocker locker(&lock);

    if (gSet==nullptr || gSet->activeWindow()==nullptr)
        return nullptr;

    CMainWindow* wnd = gSet->activeWindow();
    if (inst==nullptr) {
        inst = new CTranslatorStatisticsTab(wnd);
        inst->bindToTab(wnd->tabMain);

        connect(inst,&CTranslatorStatisticsTab::destroyed,gSet,[](){
            inst = nullptr;
        });

        inst->updateGraph();
    }

    return inst;
}

QString CTranslatorStatisticsTab::getTimeRangeString() const
{
    QString res = tr("all time");
    if (ui->radioRangeWeek->isChecked()) {
        res = tr("one week");
    } else if (ui->radioRangeMonth->isChecked()) {
        res = tr("last month");
    } else if (ui->radioRangeThisMonth->isChecked()) {
        res = tr("this month");
    } else if (ui->radioRangeYear->isChecked()) {
        res = tr("one year");
    }
    return res;
}

double CTranslatorStatisticsTab::setGraphData(CStructures::TranslationEngine engine, QCPGraph* graph,
                                              const QDate &start, const QDate &end, CStructures::DateRange range)
{
    const auto baseData = gSet->settings()->translatorStatistics.value(engine);

    const int monthsCount = 12;

    QMap<QDate,quint64> filteredData;
    const auto itstart = baseData.lowerBound(start);
    const auto itend = baseData.upperBound(end);

    // Group actual daily-based data to specified time range
    for (auto it = itstart; it != itend; ++it) {

        // Group by day
        if (range == CStructures::DateRange::drWeek ||
                range == CStructures::DateRange::drMonth ||
                range == CStructures::DateRange::drThisMonth) {
            filteredData[it.key()] = it.value();

        // Group by month
        } else if (range == CStructures::DateRange::drYear) {
            filteredData[QDate(it.key().year(),it.key().month(),1)] += it.value();

        // Group by year
        } else if (range == CStructures::DateRange::drAll) {
            filteredData[QDate(it.key().year(),1,1)] += it.value();

        }
    }

    // Add zero values if translator was not used on specified date
    if (range == CStructures::DateRange::drWeek ||
            range == CStructures::DateRange::drMonth ||
            range == CStructures::DateRange::drThisMonth) {
        for (qint64 i=0;i<=start.daysTo(end);i++) {
            QDate d = start.addDays(i);
            if (!filteredData.contains(d))
                filteredData[d] = 0;
        }
    } else if (range == CStructures::DateRange::drYear) {
        for (int i=0;i<=monthsCount;i++) {
            QDate d = start.addMonths(i);
            d = QDate(d.year(),d.month(),1);
            if (!filteredData.contains(d))
                filteredData[d] = 0;
        }
    } else if (range == CStructures::DateRange::drAll) {
        for (int i=(m_minimalDate.year()-1);i<=end.year();i++) {
            QDate d(i,1,1);
            if (!filteredData.contains(d))
                filteredData[d] = 0;
        }
    }

    // Display accumulated data to graph
    double acc = 0.0;
    for (auto it = filteredData.constKeyValueBegin(), end = filteredData.constKeyValueEnd(); it != end; ++it) {
        acc += static_cast<double>((*it).second);
        graph->addData(dateToTick((*it).first), acc);
    }

    // Rescale value axis
    bool foundRange;
    const double valueRangeFactor = 1.2;
    QCPRange dataRange = graph->getValueRange(foundRange);
    QCPRange axisRange = graph->valueAxis()->range();
    if (foundRange && (dataRange.upper*valueRangeFactor) > axisRange.upper) {
        graph->valueAxis()->setRange(axisRange.lower,
                                    qMax((dataRange.upper*valueRangeFactor),axisRange.upper));
    }

    return acc;
}

double CTranslatorStatisticsTab::dateToTick(const QDate &date) const
{
    const double oneK = 1000.0;
    return static_cast<double>(QDateTime(date).toMSecsSinceEpoch())/oneK;
}

void CTranslatorStatisticsTab::updateGraph()
{
    const QString titleText = tr("Translator statistics - %1");
    const QChar sigma(0x2211);
    const qreal titleFontSizeFactor = 1.2;
    const QMargins titleMargins(10,10,10,10);
    const QMargins legendMargins(10,10,10,10);
    const int weekDaysCount = 7;
    const int monthsCount = 12;
    const double weekRangeExtraFactor = 0.5;
    const double rowCompactVerticalStretchFactor = 0.001;
    const int graphBrushAlpha = 50;

    ui->plot->clearGraphs();

    // Reset grid
    QCPLayoutGrid* grid = ui->plot->plotLayout();
    grid->clear();

    // Add graph title
    auto title = new QCPTextElement(ui->plot);
    QFont titleFont = font();
    titleFont.setPointSizeF(titleFont.pointSizeF()*titleFontSizeFactor);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setMargins(titleMargins);
    title->setText(titleText.arg(getTimeRangeString()));
    grid->addElement(0,0,title);
    grid->setRowStretchFactor(0,rowCompactVerticalStretchFactor);

    // Add plot area
    auto axisRect = new QCPAxisRect(ui->plot,true);
    grid->addElement(1,0,axisRect);

    // Add legend
    auto legendLayout = new QCPLayoutGrid;
    grid->addElement(2,0,legendLayout);
    grid->setRowStretchFactor(2,rowCompactVerticalStretchFactor);
    legendLayout->setMargins(legendMargins);
    auto legend = new QCPLegend;
    legendLayout->addElement(legend);
    legend->setLayer(QSL("legend"));
    legend->setBorderPen(QPen(QColor(Qt::black),1));
    legend->setFillOrder(QCPLegend::foColumnsFirst);
    legend->setVisible(true);

    // Key axis range calculation
    QDate end = QDate::currentDate();
    CStructures::DateRange range = CStructures::DateRange::drAll;
    QSharedPointer<QCPAxisTickerText> xTicker(new QCPAxisTickerText);
    ui->plot->xAxis->setTicker(xTicker);
    QSharedPointer<CCPAxisTickerSize> yTicker(new CCPAxisTickerSize);
    ui->plot->yAxis->setTicker(yTicker);
    ui->plot->yAxis->grid()->setSubGridVisible(true);
    double rangeExtra = 0.0;

    m_minimalDate = end;
    for (auto it = CStructures::translationEngines().constBegin(),
         end = CStructures::translationEngines().constEnd(); it != end; ++it) {
        if (!gSet->settings()->translatorStatistics.contains(it.key()))
            continue;
        QDate minDate = gSet->settings()->translatorStatistics.value(it.key()).firstKey();
        if (minDate.toJulianDay()<m_minimalDate.toJulianDay())
            m_minimalDate = minDate;
    }
    QDate start = m_minimalDate.addYears(-2);

    if (ui->radioRangeWeek->isChecked()) {
        start = end.addDays(-1*weekDaysCount);
        rangeExtra = m_dayWidth * weekRangeExtraFactor;
        range = CStructures::DateRange::drWeek;
        for (qint64 i=0;i<=start.daysTo(end);i++) {
            QDate d = start.addDays(i);
            xTicker->addTick(dateToTick(d),d.toString(QSL("d\nMMM")));
        }
    } else if (ui->radioRangeMonth->isChecked()) {
        start = end.addMonths(-1);
        rangeExtra = m_dayWidth;
        range = CStructures::DateRange::drMonth;
        for (qint64 i=0;i<=start.daysTo(end);i++) {
            QDate d = start.addDays(i);
            xTicker->addTick(dateToTick(d),d.toString(QSL("d\nMMM")));
        }
    } else if (ui->radioRangeThisMonth->isChecked()) {
        start = QDate(end.year(),end.month(),1);
        rangeExtra = m_dayWidth;
        range = CStructures::DateRange::drThisMonth;
        for (qint64 i=0;i<=start.daysTo(end);i++) {
            QDate d = start.addDays(i);
            xTicker->addTick(dateToTick(d),d.toString(QSL("d\nMMM")));
        }
    } else if (ui->radioRangeYear->isChecked()) {
        start = end.addYears(-1);
        range = CStructures::DateRange::drYear;
        for (int i=0;i<=monthsCount;i++) {
            QDate d = start.addMonths(i);
            d = QDate(d.year(),d.month(),1);
            xTicker->addTick(dateToTick(d),d.toString(QSL("MMM\nyyyy")));
        }
    }

    // Axis configuration
    ui->plot->yAxis->setRange(0,1);
    ui->plot->xAxis->setRange(QCPRange(dateToTick(start) - rangeExtra,
                                       dateToTick(end) + rangeExtra));
    ui->plot->yAxis->setLabel(tr("Translated characters"));
    ui->plot->yAxis->setNumberPrecision(1);

    // Add graphs and their data
    int idx = 0;
    for (auto it = CStructures::translationEngines().constBegin(),
         iend = CStructures::translationEngines().constEnd(); it != iend; ++it) {

        auto graph = new QCPGraph(ui->plot->xAxis, ui->plot->yAxis);

        // Graph
        QColor color = CSettings::snippetColors.at(idx % CSettings::snippetColors.count());
        graph->setPen(QPen(color,2));
        color.setAlpha(graphBrushAlpha);
        graph->setBrush(color);

        // Graph selection
        auto decorator = new QCPSelectionDecorator;
        decorator->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus));
        decorator->setUsedScatterProperties(QCPScatterStyle::spShape);
        decorator->setBrush(QBrush(color,Qt::Dense4Pattern));
        graph->setSelectionDecorator(decorator);

        // Legend item
        double sum = setGraphData(it.key(), graph, start, end, range);
        graph->setName(QSL("%1\n%2: %3")
                       .arg(it.value())
                       .arg(sigma)
                       .arg(CGenericFuncs::formatSize(sum,1)));
        graph->addToLegend(legend);

        idx++;
    }

    // Change scale for key axis
    if (ui->radioRangeAll->isChecked()) {
        for (int i=(m_minimalDate.year()-1);i<=end.year();i++) {
            QDate d(i,1,1);
            xTicker->addTick(dateToTick(d),d.toString(QSL("yyyy")));
        }
    }

    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    ui->plot->replot();
}

void CTranslatorStatisticsTab::radioRangeChanged(bool checked)
{
    if (checked)
        updateGraph();
}

void CTranslatorStatisticsTab::save()
{
    static const QStringList filters ( { tr("PDF file (*.pdf)"),
                                         tr("PNG file (*.png)"),
                                         tr("Jpeg file (*.jpg)"),
                                         tr("BMP file (*.bmp)") } );

    QString fname = CGenericFuncs::getSaveFileNameD(this,tr("Save graph to file"),CGenericFuncs::getTmpDir(),
                                                    filters);

    if (fname.isNull() || fname.isEmpty()) return;
    QFileInfo fi(fname);
    if (fi.suffix().toLower().compare(QSL("pdf"),Qt::CaseInsensitive)==0) {
        ui->plot->savePdf(fname);
    } else if (fi.suffix().toLower().compare(QSL("png"),Qt::CaseInsensitive)==0) {
        ui->plot->savePng(fname);
    } else if (fi.suffix().toLower().compare(QSL("jpg"),Qt::CaseInsensitive)==0) {
        ui->plot->saveJpg(fname);
    } else if (fi.suffix().toLower().compare(QSL("bmp"),Qt::CaseInsensitive)==0) {
        ui->plot->saveBmp(fname);
    }
}

QString CCPAxisTickerSize::getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision)
{
    Q_UNUSED(locale)
    Q_UNUSED(formatChar)

    return CGenericFuncs::formatSize(tick, precision);
}
