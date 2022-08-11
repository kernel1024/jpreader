#include <QIcon>
#include <QCloseEvent>

#include "workermonitor.h"
#include "ui_workermonitor.h"
#include "global/structures.h"
#include "global/control.h"
#include "global/ui.h"
#include "utils/genericfuncs.h"

namespace CDefaults {
const int workerMonitorColumnCount = 3;
const int workerMonitorUpdateTimeMS = 5000;
}

CWorkerMonitor::CWorkerMonitor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CWorkerMonitor)
{
    ui->setupUi(this);

    setWindowIcon(QIcon::fromTheme(QSL("system-run")));

    m_model = new CWorkerMonitorModel(this);
    ui->table->setModel(m_model);
    ui->table->verticalHeader()->hide();

    connect(ui->buttonAbort,&QPushButton::clicked,this,&CWorkerMonitor::abortWorker);

    m_workerStatusTimer.setInterval(CDefaults::workerMonitorUpdateTimeMS);
    m_workerStatusTimer.setSingleShot(false);
    connect(&m_workerStatusTimer,&QTimer::timeout,this,&CWorkerMonitor::updateWorkerTimes);
    m_workerStatusTimer.start();
}

CWorkerMonitor::~CWorkerMonitor()
{
    delete ui;
}

void CWorkerMonitor::workerStarted(CAbstractThreadWorker *worker)
{
    m_model->workerStarted(worker);
}

void CWorkerMonitor::workerAboutToFinished(CAbstractThreadWorker *worker)
{
    m_model->workerAboutToFinished(worker);
}

void CWorkerMonitor::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CWorkerMonitor::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    m_workerStatusTimer.stop();

    if (!m_firstResize) return;
    m_firstResize = false;

    const int minColumnWidth = 150;
    const int horizontalWindowMargin = 200;
    const int verticalWindowMargin = 100;

    int columnWidth = ui->table->width() / CDefaults::workerMonitorColumnCount;
    if (columnWidth < minColumnWidth)
        columnWidth = minColumnWidth;

    for (int i=0;i<CDefaults::workerMonitorColumnCount-1;i++)
        ui->table->setColumnWidth(i,columnWidth);

    ui->table->horizontalHeader()->setStretchLastSection(true);

    QRect geom = gSet->ui()->getLastMainWindowGeometry();
    if (!geom.isNull()) {
        QPoint p = geom.topLeft() + QPoint(horizontalWindowMargin,verticalWindowMargin);
        move(p);
    }
}

void CWorkerMonitor::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event)
    m_workerStatusTimer.stop();
}

void CWorkerMonitor::abortWorker()
{
    const QModelIndexList indexList = ui->table->selectionModel()->selectedRows(0);
    for (const auto &idx : indexList)
        m_model->abortWorker(idx);
}

void CWorkerMonitor::updateWorkerTimes()
{
    m_model->updateAllWorkerTimes();
}

CWorkerMonitorModel::CWorkerMonitorModel(CWorkerMonitor *parent)
    : QAbstractTableModel(parent),
      m_monitor(parent)
{
}

CWorkerMonitorModel::~CWorkerMonitorModel() = default;

Qt::ItemFlags CWorkerMonitorModel::flags(const QModelIndex &index) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return Qt::NoItemFlags;

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QVariant CWorkerMonitorModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    int idx = index.row();
    CWorkerMonitorItem t = m_data.at(idx);

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return t.description;
            case 1: return QSL("%1 / %2 requests")
                        .arg(CGenericFuncs::formatFileSize(t.loadedTotalSize))
                        .arg(t.loadedRequestCount);
            case 2: {
                const QDateTime now = QDateTime::currentDateTime();
                return QSL("%1 (since %2)")
                        .arg(CGenericFuncs::secsToString(t.started.secsTo(now)),
                             t.started.time().toString());
            }
            default: return QVariant();
        }
    }

    return QVariant();
}

QVariant CWorkerMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return tr("Description");
            case 1: return tr("Data transfer");
            case 2: return tr("Processing time");
            default: return QVariant();
        }
    }
    return QVariant();
}

int CWorkerMonitorModel::rowCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return m_data.count();
}

int CWorkerMonitorModel::columnCount(const QModelIndex &parent) const
{
    if (!checkIndex(parent))
        return 0;
    if (parent.isValid())
        return 0;

    return CDefaults::workerMonitorColumnCount;
}

void CWorkerMonitorModel::workerStarted(CAbstractThreadWorker *worker)
{
    connect(worker,&CAbstractThreadWorker::dataLoaded,
            this,&CWorkerMonitorModel::workerDataLoaded,Qt::QueuedConnection);

    int row = m_data.count();
    beginInsertRows(QModelIndex(),row,row);
    m_data.append(CWorkerMonitorItem(worker));
    endInsertRows();
}

void CWorkerMonitorModel::workerAboutToFinished(CAbstractThreadWorker *worker)
{
    int row = m_data.indexOf(CWorkerMonitorItem(worker));
    if (row<0) return;

    beginRemoveRows(QModelIndex(),row,row);
    m_data.removeAt(row);
    endRemoveRows();
}

void CWorkerMonitorModel::abortWorker(const QModelIndex &index)
{
    if (!checkIndex(index,CheckIndexOption::IndexIsValid)) return;

    int row = index.row();
    if (m_data.at(row).worker) {
        QMetaObject::invokeMethod(m_data.at(row).worker.data(),&CAbstractThreadWorker::abort,
                                  Qt::QueuedConnection);
    }
}

void CWorkerMonitorModel::updateAllWorkerTimes()
{
    const int timeColumn = 2;
    Q_EMIT dataChanged(index(0,timeColumn),
                       index(m_data.count()-1,timeColumn));
}

void CWorkerMonitorModel::workerDataLoaded(qint64 loadedTotalSize, qint64 loadedRequestCount, const QString &description)
{
    auto *worker = qobject_cast<CAbstractThreadWorker *>(sender());
    if (worker == nullptr) return;

    int idx = m_data.indexOf(CWorkerMonitorItem(worker));
    if (idx<0) return;

    m_data[idx].loadedTotalSize = loadedTotalSize;
    m_data[idx].loadedRequestCount = loadedRequestCount;
    m_data[idx].description = description;

    Q_EMIT dataChanged(index(idx,0),index(idx,CDefaults::workerMonitorColumnCount-1));
}

CWorkerMonitorItem::CWorkerMonitorItem(CAbstractThreadWorker *w)
    : worker(w)
{
    description = worker->workerDescription();
    started = QDateTime::currentDateTime();
}

bool CWorkerMonitorItem::operator==(const CWorkerMonitorItem &s) const
{
    return worker == s.worker;
}

bool CWorkerMonitorItem::operator!=(const CWorkerMonitorItem &s) const
{
    return !operator==(s);
}
