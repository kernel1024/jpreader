#ifndef AUXDICTIONARY_H
#define AUXDICTIONARY_H

#include <QDialog>
#include <QStringListModel>
#include <QTextBrowser>
#include <QListWidgetItem>

namespace Ui {
class CAuxDictionary;
}

class WordFinder;

class CAuxDictKeyFilter : public QObject
{
    Q_OBJECT
public:
    explicit CAuxDictKeyFilter(QObject *parent = nullptr);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
Q_SIGNALS:
    void keyPressed(int key);
};

class CAuxDictionary : public QDialog
{
    Q_OBJECT

public:
    explicit CAuxDictionary(QWidget *parent = nullptr);
    ~CAuxDictionary() override;

    void adjustSplitters();
    void findWord(const QString& text);

private:
    Ui::CAuxDictionary *ui;
    WordFinder *wordFinder;
    CAuxDictKeyFilter* keyFilter;
    QStringListModel* wordHistoryModel;
    QTextBrowser* viewArticles;
    bool forceFocusToEdit { false };
    void showTranslationFor(const QString& text);
    void updateMatchResults(bool finished);

    Q_DISABLE_COPY(CAuxDictionary)

protected:
    void closeEvent(QCloseEvent * event) override;

public Q_SLOTS:
    void translateInputChanged(const QString& text);
    void translateInputFinished();
    void wordListSelectionChanged();
    void wordListLookupItem(QListWidgetItem* item);
    void prefixMatchUpdated();
    void prefixMatchFinished();
    void articleLoadFinished();
    void showEmptyDictPage();
    void restoreWindow();
    void editKeyPressed(int key);
    void dictLoadUrl(const QUrl& url);

};

#endif // AUXDICTIONARY_H
