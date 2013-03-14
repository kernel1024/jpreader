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
#include "specwidgets.h"

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
    QLineEdit* browser;
    QLineEdit* editor;
	QListWidget* qrList;
	QListWidget* bmList;
    QRadioButton* rbNifty;
    QRadioButton* rbGoogle;
    QRadioButton* rbAtlas;
    QCheckBox* cbSCP;
    QLineEdit* scpParams;
    QLineEdit* scpHost;
    QLineEdit* atlHost;
    QSpinBox* atlPort;
    QSpinBox* maxRecycled;
    QListWidget* hsList;
    QCheckBox* useJS;
    QListWidget* adList;
    QCheckBox* useAd;
    QCheckBox* useOverrideFont;
    QSpinBox* fontOverrideSize;
    QFontComboBox* fontOverride;
    QCheckBox* autoloadImages;
    QCheckBox* overrideStdFonts;
    QFontComboBox *fontStandard, *fontFixed, *fontSerif, *fontSansSerif;
    QCheckBox *overrideFontColor;
    QHotkeyEdit *gctxHotkey;

    explicit CSettingsDlg(QWidget *parent = 0);
    virtual ~CSettingsDlg();

private:
    Ui::SettingsDlg *ui;
    QColor overridedFontColor;

public:
    void updateFontColorPreview(const QColor &c);
    QColor getOverridedFontColor();

public slots:
    void selectDir();
    void selectBrowser();
    void selectEditor();
	void delQrs();
	void delBkm();
    void clearCookies();
    void clearHistory();
    void goHistory();
    void addAd();
    void delAd();
    void importAdOpera();
    void importAd();
    void fontColorDlg();
};

#endif // SETTINGSDLG_H
