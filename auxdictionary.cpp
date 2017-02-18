#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>
#include <QKeyEvent>
#include <QCompleter>
#include <goldendictlib/goldendictmgr.hh>

#include "auxdictionary.h"
#include "genericfuncs.h"
#include "globalcontrol.h"
#include "ui_auxdictionary.h"

CAuxDictionary::CAuxDictionary(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CAuxDictionary)
{
    ui->setupUi(this);

    viewArticles = ui->browser;
    forceFocusToEdit = false;

    ui->btnClear->setIcon(QIcon::fromTheme("edit-clear"));

    wordFinder = new WordFinder(this);

    wordHistoryModel = new QStringListModel();
    ui->editWord->setCompleter(new QCompleter(wordHistoryModel));

    connect(ui->editWord, &QLineEdit::textChanged, this, &CAuxDictionary::translateInputChanged);
    connect(ui->editWord, &QLineEdit::returnPressed, this, &CAuxDictionary::translateInputFinished);

    connect(ui->listWords, &QListWidget::itemSelectionChanged,
            this, &CAuxDictionary::wordListSelectionChanged);

    connect(wordFinder, &WordFinder::updated, this, &CAuxDictionary::prefixMatchUpdated);
    connect(wordFinder, &WordFinder::finished, this, &CAuxDictionary::prefixMatchFinished);

    connect(viewArticles, &QTextBrowser::textChanged, this, &CAuxDictionary::articleLoadFinished);
    connect(viewArticles, &QTextBrowser::anchorClicked, this, &CAuxDictionary::dictLoadUrl);

    keyFilter = new CAuxDictKeyFilter(this);
    ui->editWord->installEventFilter(keyFilter);
    connect(keyFilter, &CAuxDictKeyFilter::keyPressed, this, &CAuxDictionary::editKeyPressed);

    wordFinder->clear();
}

CAuxDictionary::~CAuxDictionary()
{
    delete ui;
}

void CAuxDictionary::adjustSplitters()
{
    QList<int> sz;
    sz << width()/2 << width()/2;
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
    QUrl req;
    req.setScheme( "gdlookup" );
    req.setHost( "localhost" );
    QUrlQuery requ;
    requ.addQueryItem( "word", text );
    req.setQuery(requ);
    dictLoadUrl(req);

    viewArticles->setCursor( Qt::WaitCursor );
}

void CAuxDictionary::editKeyPressed(int )
{
    forceFocusToEdit = true;
}

void CAuxDictionary::dictLoadUrl(const QUrl &url)
{
    QNetworkRequest rq(url);
    QNetworkReply* rpl = gSet->dictNetMan->get(rq);

    connect(rpl,&QNetworkReply::finished,[this,rpl](){
        QByteArray rplb;
        if (rpl->error()==QNetworkReply::NoError)
            rplb = rpl->readAll();
        else
            rplb = makeSimpleHtml(tr("Error"),
                                  tr("Dictionary request failed for query '%1'.")
                                  .arg(rpl->url().toString())).toUtf8();
        viewArticles->setHtml(rplb);
        rpl->deleteLater();
    });

}

void CAuxDictionary::updateMatchResults(bool finished)
{
    WordFinder::SearchResults const & results = wordFinder->getResults();

    ui->listWords->setUpdatesEnabled( false );

    for( unsigned x = 0; x < results.size(); ++x )
    {
        QListWidgetItem * i = ui->listWords->item( static_cast<int>(x) );

        if ( !i )
        {
            i = new QListWidgetItem( results[ x ].first, ui->listWords );

            if ( results[ x ].second )
            {
                QFont f = i->font();
                f.setItalic( true );
                i->setFont( f );
            }
            ui->listWords->addItem( i );
        }
        else
        {
            if ( i->text() != results[ x ].first )
                i->setText( results[ x ].first );

            QFont f = i->font();
            if ( f.italic() != results[ x ].second )
            {
                f.setItalic( results[ x ].second );
                i->setFont( f );
            }
        }
        if (i->text().at(0).direction() == QChar::DirR)
            i->setTextAlignment(Qt::AlignRight);
        if (i->text().at(0).direction() == QChar::DirL)
            i->setTextAlignment(Qt::AlignLeft);
    }

    while ( ui->listWords->count() > static_cast<int>(results.size()) )
    {
        // Chop off any extra items that were there
        QListWidgetItem * i = ui->listWords->takeItem( ui->listWords->count() - 1 );

        if ( i )
            delete i;
        else
            break;
    }

    if ( ui->listWords->count() )
    {
        ui->listWords->scrollToItem( ui->listWords->item( 0 ), QAbstractItemView::PositionAtTop );
        ui->listWords->setCurrentItem( 0, QItemSelectionModel::Clear );
    }

    ui->listWords->setUpdatesEnabled( true );

    if ( finished )
    {
        ui->listWords->unsetCursor();

        if (ui->listWords->count()>0)
            ui->listWords->setCurrentRow(0);

        if ( !wordFinder->getErrorString().isEmpty() )
            QMessageBox::critical(this,tr("JPReader"),
                                  tr( "WARNING: %1" ).arg( wordFinder->getErrorString() ) );
    }
}

void CAuxDictionary::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void CAuxDictionary::translateInputChanged(const QString &text)
{
    showEmptyDictPage();

    if ( ui->listWords->selectionModel()->hasSelection() )
        ui->listWords->setCurrentItem( 0, QItemSelectionModel::Clear );

    QString req = text.trimmed();

    if ( !req.size() )
    {
        // An empty request always results in an empty result
        wordFinder->cancel();
        ui->listWords->clear();
        ui->listWords->unsetCursor();

        return;
    }

    ui->listWords->setCursor( Qt::WaitCursor );

    wordFinder->prefixMatch( req, gSet->dictManager->dictionaries );
}

void CAuxDictionary::prefixMatchUpdated()
{
    updateMatchResults(false);
}

void CAuxDictionary::prefixMatchFinished()
{
    updateMatchResults(true);
}

void CAuxDictionary::translateInputFinished()
{
    QString word = ui->editWord->text();

    if ((!(wordHistoryModel->stringList().contains(word))) && !word.isEmpty()) {
        QStringList h = wordHistoryModel->stringList();
        h.append(word);
        wordHistoryModel->setStringList(h);
    }

    if ( word.size() )
        showTranslationFor( word );
}

void CAuxDictionary::wordListSelectionChanged()
{
    QList< QListWidgetItem * > selected = ui->listWords->selectedItems();

    if ( selected.size() ) {
        QString newValue = selected.front()->text();

        showTranslationFor(newValue);
    }
}

void CAuxDictionary::articleLoadFinished()
{
    viewArticles->unsetCursor();

    if (forceFocusToEdit)
        ui->editWord->setFocus();
}

void CAuxDictionary::showEmptyDictPage()
{
    viewArticles->clear();
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
        QKeyEvent *ev = static_cast<QKeyEvent *>(event);
        if (ev!=NULL)
            emit keyPressed(ev->key());
    }
    return QObject::eventFilter(obj,event);
}
