#ifndef WORKERMONITOR_H
#define WORKERMONITOR_H

#include <QWidget>
#include <QTableView>
#include <QPointer>
#include <QDateTime>
#include <QTimer>
#include "abstractthreadworker.h"

class CWorkerMonitorItem
{
public:
    QString description;
    qint64 loadedTotalSize { 0L };
    qint64 loadedRequestCount { 0L };
    QPointer<CAbstractThreadWorker> worker;
    QDateTime started;

    CWorkerMonitorItem() = default;
    CWorkerMonitorItem(const CWorkerMonitorItem& other) = default;
    explicit CWorkerMonitorItem(CAbstractThreadWorker* w);
    ~CWorkerMonitorItem() = default;
    CWorkerMonitorItem &operator=(const CWorkerMonitorItem& other) = default;
    bool operator==(const CWorkerMonitorItem &s) const;
    bool operator!=(const CWorkerMonitorItem &s) const;
};

Q_DECLARE_METATYPE(CWorkerMonitorItem)

namespace Ui {
class CWorkerMonitor;
}

class CWorkerMonitorModel;

class CWorkerMonitor : public QWidget
{
    Q_OBJECT

public:
    explicit CWorkerMonitor(QWidget *parent = nullptr);
    ~CWorkerMonitor() override;
    void workerStarted(CAbstractThreadWorker *worker);
    void workerAboutToFinished(CAbstractThreadWorker *worker);

private:
    Ui::CWorkerMonitor *ui;
    CWorkerMonitorModel *m_model;
    QTimer m_workerStatusTimer;
    bool m_firstResize { true };

    Q_DISABLE_COPY(CWorkerMonitor)

protected:
    void closeEvent(QCloseEvent * event) override;
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

private Q_SLOTS:
    void abortWorker();
    void updateWorkerTimes();

};

class CWorkerMonitorModel : public QAbstractTableModel
{
    Q_OBJECT

private:
    CWorkerMonitor* m_monitor;
    QList<CWorkerMonitorItem> m_data;

    Q_DISABLE_COPY(CWorkerMonitorModel)

public:
    explicit CWorkerMonitorModel(CWorkerMonitor* parent);
    ~CWorkerMonitorModel() override;

    Qt::ItemFlags flags(const QModelIndex & index) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    int rowCount( const QModelIndex & parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;

    void workerStarted(CAbstractThreadWorker *worker);
    void workerAboutToFinished(CAbstractThreadWorker *worker);
    void abortWorker(const QModelIndex& index);
    void updateAllWorkerTimes();

private Q_SLOTS:
    void workerDataLoaded(qint64 loadedTotalSize, qint64 loadedRequestCount,
                          const QString& description);

};

#endif // WORKERMONITOR_H
