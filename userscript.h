/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#ifndef OTTER_USERSCRIPT_H
#define OTTER_USERSCRIPT_H

#include <QObject>
#include <QString>
#include <QUrl>

class CUserScript
{
public:
    enum InjectionTime
    {
        DocumentCreationTime = 0,
        DocumentReadyTime,
        DeferredTime
    };

    CUserScript() = default;
    CUserScript(const CUserScript& other);
    explicit CUserScript(const QString &name, const QString &source = QString());
    ~CUserScript() = default;
    CUserScript &operator=(const CUserScript& other) = default;

    QString getName() const;
    QString getTitle() const;
    QString getDescription() const;
    QString getVersion() const;
    QString getSource() const;
    void setSource(const QString& src);
    QUrl getHomePage() const;
    QUrl getUpdateUrl() const;
    QStringList getExcludeRules() const;
    QStringList getIncludeRules() const;
    QStringList getMatchRules() const;
    InjectionTime getInjectionTime() const;
    bool isEnabledForUrl(const QUrl &url) const;
    bool shouldRunOnSubFrames() const;
    bool shouldRunFromContextMenu() const;

protected:
    bool checkUrl(const QUrl &url, const QStringList &rules) const;

private:
    QString m_name;
    QString m_source;
    QString m_title;
    QString m_description;
    QString m_version;
    QUrl m_homePage;
    QUrl m_updateUrl;
    QStringList m_excludeRules;
    QStringList m_includeRules;
    QStringList m_matchRules;
    InjectionTime m_injectionTime { DocumentReadyTime };
    bool m_shouldRunOnSubFrames { true };
    bool m_runFromContextMenu { false };

};

Q_DECLARE_METATYPE(CUserScript)

#endif
