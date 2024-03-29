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

#include <algorithm>

#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>

#include "userscript.h"
#include "utils/genericfuncs.h"
#include "global/structures.h"
#include "utils/specwidgets.h"


CUserScript::CUserScript(const QString &name, const QString &source)
    : m_name(name)
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
        if (!line.startsWith(QSL("//")))
            continue;

        line = line.mid(2).trimmed();
        if (line.startsWith(QSL("==UserScript==")))
        {
            hasHeader = true;
            continue;
        }

        if (!line.startsWith(u'@'))
            continue;

        line = line.mid(1);

        const QString keyword(line.section(u' ', 0, 0));

        if (keyword == QSL("description")) {
            m_description = line.section(u' ', 1, -1).trimmed();

        } else if (keyword == QSL("exclude")) {
            m_excludeRules.append(line.section(u' ', 1, -1).trimmed());

        } else if (keyword == QSL("homepage")) {
            m_homePage = QUrl(line.section(u' ', 1, -1).trimmed());

        } else if (keyword == QSL("include")) {
            m_includeRules.append(line.section(u' ', 1, -1).trimmed());

        } else if (keyword == QSL("match")) {
            line = line.section(u' ', 1, -1).trimmed();

            static const QRegularExpression urlRegex(QSL("^.+://.*/.*"));
            if (urlRegex.match(line).hasMatch()
                    && (!line.startsWith(u'*') || line.at(1) == u':'))
            {
                const QString scheme(line.left(line.indexOf(QSL("://"))));

                if (scheme == QSL("*") ||
                        scheme == QSL("http") ||
                        scheme == QSL("https") ||
                        scheme == QSL("file") ||
                        scheme == QSL("ftp") ||
                        scheme == CMagicFileSchemeHandler::getScheme().toLower())
                {
                    const QString pathAndDomain(line.mid(line.indexOf(QSL("://")) + 3));
                    const QString domain(pathAndDomain.left(pathAndDomain.indexOf(u'/')));

                    if (domain.indexOf(u'*') < 0 ||
                            (domain.indexOf(u'*') == 0 &&
                             (domain.length() == 1 ||
                              (domain.length() > 1 && domain.at(1) == u'.'))))
                    {
                        m_matchRules.append(line);
                        continue;
                    }
                }
            }

            qWarning() << "Invalid match rule for User Script, line:" << line;

        } else if (keyword == QSL("name")) {
            m_title = line.section(u' ', 1, -1).trimmed();

        } else if (keyword == QSL("noframes")) {
            m_shouldRunOnSubFrames = true;

        } else if (keyword == QSL("run-at")) {
            const QString injectionTime(line.section(u' ', 1, -1).trimmed());

            if (injectionTime == QSL("document-start")) {
                m_injectionTime = DocumentCreationTime;

            } else if (injectionTime == QSL("document-idle")) {
                m_injectionTime = DeferredTime;

            } else if (injectionTime == QSL("context-menu")) {
                m_runFromContextMenu = true;
                m_injectionTime = DocumentReadyTime;

            } else if (injectionTime == QSL("translator")) {
                m_runByTranslator = true;
                m_injectionTime = DocumentReadyTime;

            } else {
                m_injectionTime = DocumentReadyTime;
            }
        } else if (keyword == QSL("updateURL")) {
            m_updateUrl = QUrl(line.section(u' ', 1, -1).trimmed());

        } else if (keyword == QSL("version")) {
            m_version = line.section(u' ', 1, -1).trimmed();
        }
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
    if (url.scheme() != QSL("http") &&
            url.scheme() != QSL("https") &&
            url.scheme() != QSL("file") &&
            url.scheme() != QSL("ftp") &&
            url.scheme() != CMagicFileSchemeHandler::getScheme().toLower())
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
    const QString uenc = url.toString(QUrl::RemoveUserInfo | QUrl::RemovePort |
                                      QUrl::RemoveFragment | QUrl::StripTrailingSlash);

    return std::any_of(rules.constBegin(),rules.constEnd(),[uenc](const QString& rule){
        QRegularExpression regexp(CGenericFuncs::convertPatternToRegExp(rule),
                                    QRegularExpression::CaseInsensitiveOption);
        return uenc.contains(regexp);
    });
}

bool CUserScript::shouldRunOnSubFrames() const
{
    return m_shouldRunOnSubFrames;
}

bool CUserScript::shouldRunFromContextMenu() const
{
    return m_runFromContextMenu;
}

bool CUserScript::shouldRunByTranslator() const
{
    return m_runByTranslator;
}
