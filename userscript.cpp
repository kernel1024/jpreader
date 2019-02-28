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
      m_shouldRunOnSubFrames(true)
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
}

CUserScript::CUserScript(const QString &name, const QString &source)
    : m_name(name),
      m_injectionTime(DocumentReadyTime),
      m_shouldRunOnSubFrames(true)
{
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
        if (!line.startsWith(QLatin1String("//")))
            continue;

        line = line.mid(2).trimmed();
        if (line.startsWith(QLatin1String("==UserScript==")))
        {
            hasHeader = true;
            continue;
        }

        if (!line.startsWith(QLatin1Char('@')))
            continue;

        line = line.mid(1);

        const QString keyword(line.section(QLatin1Char(' '), 0, 0));

        if (keyword == QLatin1String("description"))
            m_description = line.section(QLatin1Char(' '), 1, -1).trimmed();
        else if (keyword == QLatin1String("exclude"))
            m_excludeRules.append(line.section(QLatin1Char(' '), 1, -1).trimmed());
        else if (keyword == QLatin1String("homepage"))
            m_homePage = QUrl(line.section(QLatin1Char(' '), 1, -1).trimmed());
        else if (keyword == QLatin1String("include"))
            m_includeRules.append(line.section(QLatin1Char(' '), 1, -1).trimmed());
        else if (keyword == QLatin1String("match"))
        {
            line = line.section(QLatin1Char(' '), 1, -1).trimmed();

            if (QRegularExpression(QLatin1String("^.+://.*/.*")).match(line).hasMatch()
                    && (!line.startsWith(QLatin1Char('*')) || line.at(1) == QLatin1Char(':')))
            {
                const QString scheme(line.left(line.indexOf(QLatin1String("://"))));

                if (scheme == QLatin1String("*") ||
                        scheme == QLatin1String("http") ||
                        scheme == QLatin1String("https") ||
                        scheme == QLatin1String("file") ||
                        scheme == QLatin1String("ftp"))
                {
                    const QString pathAndDomain(line.mid(line.indexOf(QLatin1String("://")) + 3));
                    const QString domain(pathAndDomain.left(pathAndDomain.indexOf(QLatin1Char('/'))));

                    if (domain.indexOf(QLatin1Char('*')) < 0 ||
                            (domain.indexOf(QLatin1Char('*')) == 0 &&
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
        else if (keyword == QLatin1String("name"))
            m_title = line.section(QLatin1Char(' '), 1, -1).trimmed();
        else if (keyword == QLatin1String("noframes"))
            m_shouldRunOnSubFrames = true;
        else if (keyword == QLatin1String("run-at"))
        {
            const QString injectionTime(line.section(QLatin1Char(' '), 1, -1));

            if (injectionTime == QLatin1String("document-start"))
                m_injectionTime = DocumentCreationTime;
            else if (injectionTime == QLatin1String("document-idle"))
                m_injectionTime = DeferredTime;
            else
                m_injectionTime = DocumentReadyTime;
        }
        else if (keyword == QLatin1String("updateURL"))
            m_updateUrl = QUrl(line.section(QLatin1Char(' '), 1, -1).trimmed());
        else if (keyword == QLatin1String("version"))
            m_version = line.section(QLatin1Char(' '), 1, -1).trimmed();
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
    if (url.scheme() != QLatin1String("http") &&
            url.scheme() != QLatin1String("https") &&
            url.scheme() != QLatin1String("file") &&
            url.scheme() != QLatin1String("ftp"))
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
