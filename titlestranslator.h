#ifndef TITLESTRANSLATOR_H
#define TITLESTRANSLATOR_H

#include <QObject>

class CTitlesTranslator : public QObject
{
    Q_OBJECT
public:
    bool inProgress;
    explicit CTitlesTranslator(QObject *parent = nullptr);

private:
    bool stopReq;

signals:
    void gotTranslation(const QStringList &res);
    void updateProgress(const int pos);

public slots:
    void translateTitles(const QStringList &titles);
    void stop();

};

#endif // TITLESTRANSLATOR_H
