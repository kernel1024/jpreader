#ifndef AUXDICTIONARY_H
#define AUXDICTIONARY_H

#include <QDialog>
#include <QStringListModel>
#include <QTextBrowser>

namespace Ui {
class CAuxDictionary;
}

class WordFinder;

class CAuxDictKeyFilter : public QObject
{
    Q_OBJECT
public:
    CAuxDictKeyFilter(QObject *parent = nullptr);
protected:
    bool eventFilter(QObject *obj, QEvent *event);
signals:
    void keyPressed(int key);
};

class CAuxDictionary : public QDialog
{
    Q_OBJECT

public:
    explicit CAuxDictionary(QWidget *parent = nullptr);
    ~CAuxDictionary();

    void adjustSplitters();
    void findWord(const QString& text);

private:
    Ui::CAuxDictionary *ui;
    WordFinder *wordFinder;
    CAuxDictKeyFilter* keyFilter;
    QStringListModel* wordHistoryModel;
    QTextBrowser* viewArticles;
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
    void articleLoadFinished();
    void showEmptyDictPage();
    void restoreWindow();
    void editKeyPressed(int key);
    void dictLoadUrl(const QUrl& url);

};

#endif // AUXDICTIONARY_H
