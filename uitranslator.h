#ifndef UITRANSLATOR_H
#define UITRANSLATOR_H

#include <QTranslator>
#include <QObject>
#include "structures.h"

class CUITranslator : public QTranslator
{
    Q_OBJECT
private:
    QString m_fileDialogNewFolderName;
public:
    CUITranslator(QObject *parent = nullptr);
    ~CUITranslator() override;
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;

    void setFileDialogNewFolderName(const QString &fileDialogNewFolderName);
};

#endif // UITRANSLATOR_H
