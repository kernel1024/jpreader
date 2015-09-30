#ifndef AUXDICTIONARY_H
#define AUXDICTIONARY_H

#include <QDialog>
#include <QStringListModel>
#include <QWebEngineView>

namespace Ui {
class CAuxDictionary;
}

class WordFinder;

class CAuxDictKeyFilter : public QObject
{
    Q_OBJECT
public:
    CAuxDictKeyFilter(QObject *parent = 0);
protected:
    bool eventFilter(QObject *obj, QEvent *event);
signals:
    void keyPressed(int key);
};

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
    CAuxDictKeyFilter* keyFilter;
    QStringListModel* wordHistoryModel;
    QWebEngineView* viewArticles;
    bool forceFocusToEdit;
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
    void editKeyPressed(int key);

};

#endif // AUXDICTIONARY_H
