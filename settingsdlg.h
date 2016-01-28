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
#include "globalcontrol.h"
#include "adblockrule.h"

namespace Ui {
    class SettingsDlg;
}

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
    QLineEdit* bingID;
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

    QStringList loadedDicts;

    explicit CSettingsDlg(QWidget *parent = 0);
    virtual ~CSettingsDlg();

private:
    Ui::SettingsDlg *ui;
    QColor overridedFontColor;
    QList<QNetworkCookie> cookiesList;
    QList<CAdBlockRule> adblockList;
    int adblockSearchIdx;

    void updateCookiesTable();
    void setCookies(const QByteArray& cookies);
    QList<int> getSelectedRows(QTableWidget* table);
    void updateAdblockList();
    void adblockFocusSearchedRule(QList<QTreeWidgetItem *> items);

public:
    void updateFontColorPreview(const QColor &c);
    QColor getOverridedFontColor();

    void setBookmarks(QBookmarksMap bookmarks);
    void setQueryHistory(QStringList history);
    void setAdblock(QList<CAdBlockRule> adblock);
    void setMainHistory(QUHList history);
    void setSearchEngines(QStrHash engines);
    QBookmarksMap getBookmarks();
    QStringList getQueryHistory();
    QList<CAdBlockRule> getAdblock();
    QStrHash getSearchEngines();

public slots:
    void selectDir();
    void selectBrowser();
    void selectEditor();
	void delQrs();
	void delBkm();
    void editBkm();
    void goHistory();
    void clearHistory();
    void addAd();
    void delAd();
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
};

#endif // SETTINGSDLG_H
