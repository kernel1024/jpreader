/**************************************************************************
* Parts of this file taken from Otter Browser.
*
*
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

#include "userscript.h"
#include "genericfuncs.h"
#include <QRegularExpression>
#include <QRegExp>
#include <QTextStream>
#include <QDebug>

CUserScript::CUserScript()
    : m_name(QString()),
      m_injectionTime(DocumentReadyTime),
      m_shouldRunOnSubFrames(true),
      m_runFromContextMenu(false)
{

}

CUserScript::CUserScript(const CUserScript &other)
{
    m_name = other.m_name;
    m_source = other.m_source;
    m_title = other.m_title;
    m_description = other.m_description;
    m_version = other.m_version;
    m_homePage = other.m_homePage;
    m_updateUrl = other.m_updateUrl;
    m_excludeRules = other.m_excludeRules;
    m_includeRules = other.m_includeRules;
    m_matchRules = other.m_matchRules;
    m_injectionTime = other.m_injectionTime;
    m_shouldRunOnSubFrames = other.m_shouldRunOnSubFrames;
    m_runFromContextMenu = other.m_runFromContextMenu;
}

CUserScript::CUserScript(const QString &name, const QString &source)
    : m_injectionTime(DocumentReadyTime),
      m_shouldRunOnSubFrames(true),
      m_runFromContextMenu(false)
{
    m_name = name;
    if (!source.isEmpty())
        setSource(source);
}

QString CUserScript::getName() const
{
    return m_name;
}

QString CUserScript::getTitle() const
{
    return m_title;
}

QString CUserScript::getDescription() const
{
    return m_description;
}

QString CUserScript::getVersion() const
{
    return m_version;
}

QString CUserScript::getSource() const
{
    return m_source;
}

void CUserScript::setSource(const QString &src)
{
    m_source = src;

    QTextStream stream(&m_source,QIODevice::ReadOnly);
    bool hasHeader(false);

    while (!stream.atEnd())
    {
        QString line(stream.readLine().trimmed());
        if (!line.startsWith(QStringLiteral("//")))
            continue;

        line = line.mid(2).trimmed();
        if (line.startsWith(QStringLiteral("==UserScript==")))
        {
            hasHeader = true;
            continue;
        }

        if (!line.startsWith('@'))
            continue;

        line = line.mid(1);

        const QString keyword(line.section(' ', 0, 0));

        if (keyword == QStringLiteral("description"))
            m_description = line.section(' ', 1, -1).trimmed();
        else if (keyword == QStringLiteral("exclude"))
            m_excludeRules.append(line.section(' ', 1, -1).trimmed());
        else if (keyword == QStringLiteral("homepage"))
            m_homePage = QUrl(line.section(' ', 1, -1).trimmed());
        else if (keyword == QStringLiteral("include"))
            m_includeRules.append(line.section(' ', 1, -1).trimmed());
        else if (keyword == QStringLiteral("match"))
        {
            line = line.section(' ', 1, -1).trimmed();

            if (QRegularExpression(QStringLiteral("^.+://.*/.*")).match(line).hasMatch()
                    && (!line.startsWith('*') || line.at(1) == QLatin1Char(':')))
            {
                const QString scheme(line.left(line.indexOf(QStringLiteral("://"))));

                if (scheme == QStringLiteral("*") ||
                        scheme == QStringLiteral("http") ||
                        scheme == QStringLiteral("https") ||
                        scheme == QStringLiteral("file") ||
                        scheme == QStringLiteral("ftp"))
                {
                    const QString pathAndDomain(line.mid(line.indexOf(QStringLiteral("://")) + 3));
                    const QString domain(pathAndDomain.left(pathAndDomain.indexOf('/')));

                    if (domain.indexOf('*') < 0 ||
                            (domain.indexOf('*') == 0 &&
                             (domain.length() == 1 ||
                              (domain.length() > 1 && domain.at(1) == QLatin1Char('.')))))
                    {
                        m_matchRules.append(line);
                        continue;
                    }
                }
            }

            qWarning() << "Invalid match rule for User Script, line:" << line;
        }
        else if (keyword == QStringLiteral("name"))
            m_title = line.section(' ', 1, -1).trimmed();
        else if (keyword == QStringLiteral("noframes"))
            m_shouldRunOnSubFrames = true;
        else if (keyword == QStringLiteral("run-at"))
        {
            const QString injectionTime(line.section(' ', 1, -1).trimmed());

            if (injectionTime == QStringLiteral("document-start"))
                m_injectionTime = DocumentCreationTime;
            else if (injectionTime == QStringLiteral("document-idle"))
                m_injectionTime = DeferredTime;
            else if (injectionTime == QStringLiteral("context-menu")) {
                m_runFromContextMenu = true;
                m_injectionTime = DocumentReadyTime;
            } else
                m_injectionTime = DocumentReadyTime;
        }
        else if (keyword == QStringLiteral("updateURL"))
            m_updateUrl = QUrl(line.section(' ', 1, -1).trimmed());
        else if (keyword == QStringLiteral("version"))
            m_version = line.section(' ', 1, -1).trimmed();
    }

    if (m_title.isEmpty())
        m_title = m_name;

    if (!hasHeader)
        qWarning() << "Failed to locate header of user script file";
}

QUrl CUserScript::getHomePage() const
{
    return m_homePage;
}

QUrl CUserScript::getUpdateUrl() const
{
    return m_updateUrl;
}

QStringList CUserScript::getExcludeRules() const
{
    return m_excludeRules;
}

QStringList CUserScript::getIncludeRules() const
{
    return m_includeRules;
}

QStringList CUserScript::getMatchRules() const
{
    return m_matchRules;
}

CUserScript::InjectionTime CUserScript::getInjectionTime() const
{
    return m_injectionTime;
}

bool CUserScript::isEnabledForUrl(const QUrl &url) const
{
    if (url.scheme() != QStringLiteral("http") &&
            url.scheme() != QStringLiteral("https") &&
            url.scheme() != QStringLiteral("file") &&
            url.scheme() != QStringLiteral("ftp"))
    {
        return false;
    }

    bool isEnabled(!(m_includeRules.length() > 0 || m_matchRules.length() > 0));

    if (checkUrl(url, m_matchRules))
        isEnabled = true;

    if (!isEnabled && checkUrl(url, m_includeRules))
        isEnabled = true;

    if (checkUrl(url, m_excludeRules))
        isEnabled = false;

    return isEnabled;
}

bool CUserScript::checkUrl(const QUrl &url, const QStringList &rules) const
{
    QString uenc = url.toString(QUrl::RemoveUserInfo | QUrl::RemovePort |
                             QUrl::RemoveFragment | QUrl::StripTrailingSlash);

    for (int i = 0; i < rules.length(); ++i)
    {
        QString rule(rules[i]);

        QRegExp m_regexp(convertPatternToRegExp(rule), Qt::CaseInsensitive, QRegExp::RegExp2);

        if (uenc.contains(m_regexp))
            return true;
    }

    return false;
}

bool CUserScript::shouldRunOnSubFrames() const
{
    return m_shouldRunOnSubFrames;
}

bool CUserScript::shouldRunFromContextMenu() const
{
    return m_runFromContextMenu;
}
