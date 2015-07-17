#ifndef AUXDICTIONARY_H
#define AUXDICTIONARY_H

#include <QDialog>


namespace Ui {
class CAuxDictionary;
}

class WordFinder;

class CAuxDictionary : public QDialog
{
    Q_OBJECT

public:
    explicit CAuxDictionary(QWidget *parent = 0);
    ~CAuxDictionary();

    void adjustSplitters();
    void findWord(const QString& text);

private:
    Ui::CAuxDictionary *ui;
    WordFinder *wordFinder;
    void showTranslationFor(const QString& text);
    void updateMatchResults(bool finished);

protected:
    void closeEvent(QCloseEvent * event);

public slots:
    void translateInputChanged(const QString& text);
    void translateInputFinished();
    void wordListSelectionChanged();
    void prefixMatchUpdated();
    void prefixMatchFinished();
    void articleLoadFinished(bool ok);
    void showEmptyDictPage();
    void restoreWindow();
    void articleLoaded();

};

#endif // AUXDICTIONARY_H
