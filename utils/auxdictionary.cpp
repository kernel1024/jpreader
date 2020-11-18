#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>
#include <QKeyEvent>
#include <QCompleter>

#include "auxdictionary.h"
#include "genericfuncs.h"
#include "global/control.h"
#include "ui_auxdictionary.h"

CAuxDictionary::CAuxDictionary(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CAuxDictionary)
{
    ui->setupUi(this);

    viewArticles = ui->browser;

    ui->btnClear->setIcon(QIcon::fromTheme(QSL("edit-clear")));

    wordHistoryModel = new QStringListModel(this);
    ui->editWord->setCompleter(new QCompleter(wordHistoryModel));

    connect(ui->editWord, &QLineEdit::textChanged, this, &CAuxDictionary::translateInputChanged);
    connect(ui->editWord, &QLineEdit::returnPressed, this, &CAuxDictionary::translateInputFinished);

    connect(ui->listWords, &QListWidget::itemSelectionChanged,
            this, &CAuxDictionary::wordListSelectionChanged);
    connect(ui->listWords, &QListWidget::itemDoubleClicked,
            this, &CAuxDictionary::wordListLookupItem);

    connect(viewArticles, &QTextBrowser::textChanged, this, &CAuxDictionary::articleLoadFinished);
    connect(viewArticles, &QTextBrowser::anchorClicked, this, &CAuxDictionary::articleLinkClicked);

    connect(gSet->dictionaryManager(),&ZDict::ZDictController::articleComplete,
            this,&CAuxDictionary::articleReady,Qt::QueuedConnection);
    connect(gSet->dictionaryManager(),&ZDict::ZDictController::wordListComplete,
            this,&CAuxDictionary::updateMatchResults,Qt::QueuedConnection);
    connect(this,&CAuxDictionary::stopDictionaryWork,
            gSet->dictionaryManager(),&ZDict::ZDictController::cancelActiveWork);

    keyFilter = new CAuxDictKeyFilter(this);
    ui->editWord->installEventFilter(keyFilter);
    connect(keyFilter, &CAuxDictKeyFilter::keyPressed, this, &CAuxDictionary::editKeyPressed);
}

CAuxDictionary::~CAuxDictionary()
{
    delete ui;
}

void CAuxDictionary::adjustSplitters()
{
    QList<int> sz({ width()/2, width()/2 });
    ui->verticalSplitter->setSizes(sz);
}

void CAuxDictionary::findWord(const QString &text)
{
    restoreWindow();
    forceFocusToEdit = false;
    ui->editWord->setText(text);
    translateInputFinished();
}

void CAuxDictionary::showTranslationFor(const QString &text)
{
    viewArticles->clear();
    gSet->dictionaryManager()->loadArticleAsync(text);
}

void CAuxDictionary::articleReady(const QString &text)
{
    if (!text.isEmpty())
        viewArticles->setHtml(text);
}

void CAuxDictionary::editKeyPressed(int key)
{
    Q_UNUSED(key)

    forceFocusToEdit = true;
}

void CAuxDictionary::updateMatchResults(const QStringList &words)
{

    ui->listWords->setUpdatesEnabled(false);

    for (int i=0; i<words.count(); i++) {
        QListWidgetItem * item = ui->listWords->item(i);

        if (item == nullptr) {
            item = new QListWidgetItem(words.at(i), ui->listWords);
            ui->listWords->addItem(item);
        } else {
            if (item->text() != words.at(i))
                item->setText(words.at(i));
        }
    }

    while (ui->listWords->count() > words.count()) {
        QListWidgetItem *i = ui->listWords->takeItem(ui->listWords->count() - 1);
        if (!i)
            break;

        delete i;
    }

    if (ui->listWords->count() > 0) {
        ui->listWords->scrollToItem(ui->listWords->item(0), QAbstractItemView::PositionAtTop);
        ui->listWords->setCurrentItem(nullptr, QItemSelectionModel::Clear);
    }

    ui->listWords->setUpdatesEnabled(true);

    if (ui->listWords->count()>0)
        ui->listWords->setCurrentRow(0);
}

void CAuxDictionary::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CAuxDictionary::translateInputChanged(const QString &text)
{
    viewArticles->clear();

    if (ui->listWords->selectionModel()->hasSelection())
        ui->listWords->setCurrentItem(nullptr, QItemSelectionModel::Clear);

    Q_EMIT stopDictionaryWork();

    QString req = text.trimmed();
    if (req.isEmpty()) {
        ui->listWords->clear();
        return;
    }

    gSet->dictionaryManager()->wordLookupAsync(text);
}

void CAuxDictionary::translateInputFinished()
{
    QString word = ui->editWord->text();

    if ((!(wordHistoryModel->stringList().contains(word))) && !word.isEmpty()) {
        QStringList h = wordHistoryModel->stringList();
        h.append(word);
        wordHistoryModel->setStringList(h);
    }

    if (!word.isEmpty())
        showTranslationFor(word);
}

void CAuxDictionary::wordListSelectionChanged()
{
    QList<QListWidgetItem *> selected = ui->listWords->selectedItems();

    if (!selected.empty()) {
        QString newValue = selected.front()->text();
        showTranslationFor(newValue);
    }
}

void CAuxDictionary::wordListLookupItem(QListWidgetItem *item)
{
    ui->editWord->setText(item->text());
    translateInputFinished();
}

void CAuxDictionary::articleLinkClicked(const QUrl &url)
{
    QUrlQuery requ(url);
    QString word = requ.queryItemValue(QSL("word"));
    if (word.startsWith('%')) {
        QByteArray bword = word.toLatin1();
        if (!bword.isNull() && !bword.isEmpty())
            word = QUrl::fromPercentEncoding(bword);
    }

    if (!word.isEmpty())
        showTranslationFor(word);
}

void CAuxDictionary::articleLoadFinished()
{
    if (forceFocusToEdit)
        ui->editWord->setFocus();
}

void CAuxDictionary::restoreWindow()
{
    show();
    activateWindow();
}


CAuxDictKeyFilter::CAuxDictKeyFilter(QObject *parent)
    : QObject(parent)
{

}

bool CAuxDictKeyFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type()==QEvent::KeyPress) {
        auto *ev = dynamic_cast<QKeyEvent *>(event);
        if (ev)
            Q_EMIT keyPressed(ev->key());
    }
    return QObject::eventFilter(obj,event);
}
