#ifndef ZSCROLLAREA_H
#define ZSCROLLAREA_H

#include <QScrollArea>
#include <QTimer>

class ZScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    explicit ZScrollArea(QWidget *parent = nullptr);
    ~ZScrollArea() override = default;
    
Q_SIGNALS:
    void sizeChanged(const QSize& size);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    QTimer resizeTimer;
    QSize savedSize;

    Q_DISABLE_COPY(ZScrollArea)
};

#endif // ZSCROLLAREA_H
