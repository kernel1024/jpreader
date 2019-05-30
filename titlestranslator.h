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

Q_SIGNALS:
    void gotTranslation(const QStringList &res);
    void updateProgress(int pos);

public Q_SLOTS:
    void translateTitles(const QStringList &titles);
    void stop();

};

#endif // TITLESTRANSLATOR_H
