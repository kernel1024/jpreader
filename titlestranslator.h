#ifndef TITLESTRANSLATOR_H
#define TITLESTRANSLATOR_H

#include <QObject>

class CTitlesTranslator : public QObject
{
    Q_OBJECT
public:
    explicit CTitlesTranslator(QObject *parent = nullptr);

private:
    bool stopReq {false};

signals:
    void gotTranslation(const QStringList &res);
    void updateProgress(int pos);

public slots:
    void translateTitles(const QStringList &titles);
    void stop();

};

#endif // TITLESTRANSLATOR_H
