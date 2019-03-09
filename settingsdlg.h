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
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
public slots:
    void addItem();
    void deleteItem();
};

class CLangPairDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    CLangPairDelegate(QObject *parent = nullptr);
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
    QLineEdit* hostingUrl;
    QLineEdit* hostingDir;
    QSpinBox* maxLimit;
    QSpinBox* maxHistory;
    QLineEdit* browser;
    QLineEdit* editor;
    QRadioButton* rbGoogle;
    QRadioButton* rbAtlas;
    QRadioButton* rbBingAPI;
    QRadioButton* rbYandexAPI;
    QRadioButton* rbGoogleGTX;
    QCheckBox* cbSCP;
    QLineEdit* scpParams;
    QComboBox* scpHost;
    QComboBox* atlHost;
    QSpinBox* atlPort;
    QSpinBox* atlRetryCount;
    QSpinBox* atlRetryTimeout;
    QLineEdit* atlToken;
    QComboBox* atlSSLProto;
    QSpinBox* maxRecycled;
    QCheckBox* useJS;
    QCheckBox* useAd;
    QCheckBox* useNoScript;
    QCheckBox* useOverrideFont;
    QSpinBox* fontOverrideSize;
    QFontComboBox* fontOverride;
    QCheckBox* autoloadImages;
    QCheckBox* overrideStdFonts;
    QFontComboBox *fontStandard, *fontFixed, *fontSerif, *fontSansSerif;
    QCheckBox *overrideFontColor;
    QKeySequenceEdit *gctxHotkey;
    QRadioButton* searchRecoll;
    QRadioButton* searchNone;
    QRadioButton* searchBaloo5;
    QCheckBox* debugLogNetReq;
    QCheckBox* debugDumpHtml;
    QCheckBox* visualShowTabCloseButtons;
    QLineEdit *proxyHost, *proxyLogin, *proxyPassword;
    QSpinBox* proxyPort;
    QCheckBox* proxyUse;
    QComboBox* proxyType;
    QCheckBox* proxyUseTranslator;
    QCheckBox* emptyRestore;
    QLineEdit* bingKey;
    QLineEdit* yandexKey;
    QCheckBox* createCoredumps;
    QCheckBox* overrideUserAgent;
    QComboBox* userAgent;
    QListWidget* dictPaths;
    QCheckBox* ignoreSSLErrors;
    QCheckBox* visualShowFavicons;
    QCheckBox* enablePlugins;
    QSpinBox* cacheSize;
    QCheckBox* jsLogConsole;
    QSpinBox* maxRecent;
    QCheckBox* dontUseNativeFileDialogs;
    QSpinBox* adblockMaxWhiteListItems;
    QCheckBox *pdfExtractImages;
    QSpinBox *pdfImageQuality;
    QSpinBox *pdfImageMaxSize;
    QCheckBox* pixivNovelExtractor;

    QStringList loadedDicts;

    explicit CSettingsDlg(QWidget *parent = nullptr);
    virtual ~CSettingsDlg();

private:
    Ui::SettingsDlg *ui;
    QColor overridedFontColor;
    QList<QNetworkCookie> cookiesList;
    QVector<CAdBlockRule> adblockList;
    CLangPairModel *transModel;
    int adblockSearchIdx;

    void resizeEvent(QResizeEvent *event);

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

public slots:
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
