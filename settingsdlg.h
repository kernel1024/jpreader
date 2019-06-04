#ifndef SETTINGSDLG_H
#define SETTINGSDLG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QRadioButton>
#include <QCheckBox>
#include <QFontComboBox>
#include <QColor>
#include <QNetworkCookie>
#include <QTableWidget>
#include <QTreeWidgetItem>
#include <QKeySequenceEdit>
#include <QStyledItemDelegate>
#include "structures.h"
#include "adblockrule.h"

namespace Ui {
    class SettingsDlg;
}

class CLangPairModel : public QAbstractTableModel
{
    Q_OBJECT
private:
    CLangPairVector m_list;
    QTableView* m_table;

public:
    CLangPairModel(QObject * parent, const CLangPairVector& list, QTableView* table);
    CLangPairVector getLangPairList();
private:
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
public Q_SLOTS:
    void addItem();
    void deleteItem();
};

class CLangPairDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit CLangPairDelegate(QObject *parent = nullptr);
private:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class CSettingsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CSettingsDlg(QWidget *parent = nullptr);
    ~CSettingsDlg() override;

private:
    Ui::SettingsDlg *ui;
    QColor overridedFontColor { Qt::black };
    int adblockSearchIdx { 0 };
    QList<QNetworkCookie> cookiesList;
    QVector<CAdBlockRule> adblockList;
    CLangPairModel *transModel;
    QStringList loadedDicts;

    Q_DISABLE_COPY(CSettingsDlg)

    void resizeEvent(QResizeEvent *event) override;

    void updateCookiesTable();
    QList<int> getSelectedRows(QTableWidget* table) const;
    void updateAdblockList();
    void adblockFocusSearchedRule(QList<QTreeWidgetItem *> &items);
    void populateTabList();

public:
    void updateFontColorPreview(const QColor &c);
    QColor getOverridedFontColor();

    void setQueryHistory(const QStringList &history);
    void setAdblock(const QVector<CAdBlockRule> &adblock);
    void setMainHistory(const CUrlHolderVector &history);
    void setSearchEngines(const CStringHash &engines);
    void setUserScripts(const CStringHash &scripts);
    void setNoScriptWhitelist(const CStringSet &hosts);
    QStringList getQueryHistory();
    QVector<CAdBlockRule> getAdblock();
    CStringHash getSearchEngines();
    CStringHash getUserScripts();
    CLangPairVector getLangPairList();
    CStringSet getNoScriptWhitelist();

public Q_SLOTS:
    void loadFromGlobal();
    void saveToGlobal(QStringList &dictPaths);

    void selectDir();
    void selectBrowser();
    void selectEditor();
	void delQrs();
    void goHistory();
    void clearHistory();
    void addAd();
    void delAd();
    void delAdAll();
    void importAd();
    void exportAd();
    void fontColorDlg();
    void addDictPath();
    void delDictPath();
    void showLoadedDicts();
    void clearCookies();
    void getCookiesFromStore();
    void delCookies();
    void exportCookies();
    void cleanAtlCerts();
    void adblockSearch(const QString& text);
    void adblockSearchBwd();
    void adblockSearchFwd();
    void addSearchEngine();
    void delSearchEngine();
    void updateAtlCertLabel();
    void setDefaultSearch();
    void addUserScript();
    void deleteUserScript();
    void importUserScript();
    void editUserScript();
    void addNoScriptHost();
    void delNoScriptHost();
};

#endif // SETTINGSDLG_H
