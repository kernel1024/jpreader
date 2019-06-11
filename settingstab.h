#ifndef SETTINGSTAB_H
#define SETTINGSTAB_H

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
#include "specwidgets.h"

namespace Ui {
    class SettingsTab;
}

class CLangPairModel : public QAbstractTableModel
{
    Q_OBJECT
private:
    QTableView* m_table;

public:
    CLangPairModel(QObject * parent, QTableView* table);

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

class CSettingsTab : public CSpecTabContainer
{
    Q_OBJECT

public:
    explicit CSettingsTab(QWidget *parent = nullptr);
    ~CSettingsTab() override;

private:
    Ui::SettingsTab *ui;
    int adblockSearchIdx { 0 };
    CLangPairModel *transModel;
    bool m_loadingInterlock { false };

    Q_DISABLE_COPY(CSettingsTab)

    void resizeEvent(QResizeEvent *event) override;

    QList<int> getSelectedRows(QTableWidget* table) const;
    void adblockFocusSearchedRule(QList<QTreeWidgetItem *> &items);
    void setupSettingsObservers();

    void updateCookiesTable();
    void updateAdblockList();
    void updateFontColorPreview();
    void updateNoScriptWhitelist();
    void updateUserScripts();
    void updateQueryHistory();
    void updateMainHistory();
    void updateSearchEngines();

    void saveSearchEngines();
    void saveUserScripts();
    void saveDictPaths();
    void saveNoScriptWhitelist();
    void clearAdblockWhitelist();

public:
    static CSettingsTab* instance();

public Q_SLOTS:
    void loadFromGlobal();

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

#endif // SETTINGSTAB_H
