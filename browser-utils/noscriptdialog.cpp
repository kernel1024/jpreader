#include "noscriptdialog.h"
#include "global/control.h"
#include "global/contentfiltering.h"
#include "utils/genericfuncs.h"
#include "ui_noscriptdialog.h"

CNoScriptDialog::CNoScriptDialog(QWidget *parent, const QString &origin) :
    QDialog(parent),
    ui(new Ui::CNoScriptDialog),
    m_origin(origin)
{
    ui->setupUi(this);

    connect(ui->buttonBox,&QDialogButtonBox::accepted,this,&CNoScriptDialog::acceptScripts);

    updateHostsList();
}

CNoScriptDialog::~CNoScriptDialog()
{
    delete ui;
}

void CNoScriptDialog::updateHostsList()
{
    ui->listScripts->clear();

    const CStringSet scripts = gSet->contentFilter()->getNoScriptPageHosts(m_origin);
    for (const QString& host : qAsConst(scripts)) {
        auto *itm = new QListWidgetItem(host);
        itm->setFlags(itm->flags() | Qt::ItemIsUserCheckable);
        if (gSet->contentFilter()->containsNoScriptWhitelist(host)) {
            itm->setCheckState(Qt::Checked);
        } else {
            itm->setCheckState(Qt::Unchecked);
        }

       ui->listScripts->addItem(itm);
    }
}

void CNoScriptDialog::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    const int maxLabelLength = 200;

    ui->labelUrl->setText(fontMetrics().elidedText(m_origin,Qt::ElideRight,
                                                   qMax(maxLabelLength,ui->labelUrl->width())));
}

void CNoScriptDialog::acceptScripts()
{
    for (int i=0;i<ui->listScripts->count();i++) {
        QListWidgetItem* itm = ui->listScripts->item(i);
        const QString host = itm->text();
        if ((itm->checkState() == Qt::Checked) &&
                (!gSet->contentFilter()->containsNoScriptWhitelist(host))) {

            gSet->contentFilter()->addNoScriptWhitelistHost(host);

        } else if ((itm->checkState() == Qt::Unchecked) &&
                   (gSet->contentFilter()->containsNoScriptWhitelist(host))) {

            gSet->contentFilter()->removeNoScriptWhitelistHost(host);

        }
    }

    accept();
}
