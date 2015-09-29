#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QKeyEvent>

#include "auxdictionary.h"
#include "goldendictmgr.h"
#include "globalcontrol.h"
#include "ui_auxdictionary.h"

CAuxDictionary::CAuxDictionary(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CAuxDictionary)
{
    ui->setupUi(this);

    forceFocusToEdit = false;

    QWebEnginePage* wp = new QSpecWebPage(gSet->webProfile,NULL);
    ui->viewArticles->setPage(wp);

    ui->btnClear->setIcon(QIcon::fromTheme("edit-clear"));

    wordFinder = new WordFinder(this);

    connect(ui->editWord, SIGNAL(editTextChanged(QString const &)),
            this, SLOT(translateInputChanged(QString const &)));

    connect(ui->editWord->lineEdit(), SIGNAL(returnPressed()),
            this, SLOT(translateInputFinished()));

    connect(ui->listWords, SIGNAL(itemSelectionChanged()),
            this, SLOT(wordListSelectionChanged()));

    connect(wordFinder, SIGNAL(updated()),
            this, SLOT(prefixMatchUpdated()));
    connect(wordFinder, SIGNAL(finished()),
            this, SLOT(prefixMatchFinished()));

    connect( ui->viewArticles,SIGNAL(loadFinished(bool)),this,SLOT(articleLoadFinished(bool)));

    keyFilter = new CAuxDictKeyFilter(this);
    ui->editWord->installEventFilter(keyFilter);
    connect(keyFilter, SIGNAL(keyPressed(int)),
            this, SLOT(editKeyPressed(int)));

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
    ui->editWord->setEditText(text);
}

void CAuxDictionary::showTranslationFor(const QString &text)
{
    QUrl req;
    req.setScheme( "gdlookup" );
    req.setHost( "localhost" );
    QUrlQuery requ;
    requ.addQueryItem( "word", text );
    req.setQuery(requ);

    QNetworkReply* rpl = gSet->dictNetMan->get(QNetworkRequest(req));
    connect(rpl,SIGNAL(finished()),this,SLOT(articleLoaded()));

    ui->viewArticles->setCursor( Qt::WaitCursor );
}

void CAuxDictionary::articleLoaded()
{
    QNetworkReply* rpl = qobject_cast<QNetworkReply *>(sender());
    if (rpl==NULL) return;
    QByteArray ba = rpl->readAll();
    rpl->close();
    ui->viewArticles->setHtml(QString::fromUtf8(ba));
}

void CAuxDictionary::editKeyPressed(int )
{
    forceFocusToEdit = true;
}

void CAuxDictionary::updateMatchResults(bool finished)
{
    WordFinder::SearchResults const & results = wordFinder->getResults();

    ui->listWords->setUpdatesEnabled( false );

    for( unsigned x = 0; x < results.size(); ++x )
    {
        QListWidgetItem * i = ui->listWords->item( x );

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

    while ( ui->listWords->count() > (int) results.size() )
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
    if ((ui->editWord->findText(text)<0) && !text.isEmpty())
        ui->editWord->addItem(ui->editWord->currentText());

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
    QString word = ui->editWord->currentText();

    if ( word.size() )
        showTranslationFor( word );
}

void CAuxDictionary::wordListSelectionChanged()
{
    QList< QListWidgetItem * > selected = ui->listWords->selectedItems();

    if ( selected.size() ) {
        QString newValue = selected.front()->text();

        if ((ui->editWord->findText(newValue)<0) && !newValue.isEmpty())
            ui->editWord->addItem(newValue);

        showTranslationFor(newValue);
    }
}

void CAuxDictionary::articleLoadFinished(bool )
{
    ui->viewArticles->unsetCursor();

    if (forceFocusToEdit)
        ui->editWord->setFocus();
}

void CAuxDictionary::showEmptyDictPage()
{
    ui->viewArticles->load(QUrl("about://blank"));
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
