/**
 * Project Arora ( https://github.com/arora/arora ). AdBlock facility.
 *
 * Copyright (c) 2009, Zsombor Gegesy <gzsombor@gmail.com>
 * Copyright (c) 2009, Benjamin C. Meyer <ben@meyerhome.net>
 *
 * Adaptation for JPReader by kernel1024
 **/

#include <QUrl>
#include <QDebug>
#include "adblockrule.h"
#include "genericfuncs.h"

CAdBlockRule::CAdBlockRule()
    : m_cssRule(false), m_exception(false), m_enabled(false)
{
    m_listID = QString();
    setFilter(QString());
}

CAdBlockRule::CAdBlockRule(const CAdBlockRule &other)
    : m_cssRule(false), m_exception(false), m_enabled(false)
{
    m_listID = other.m_listID;
    setFilter(other.filter());
}

CAdBlockRule::CAdBlockRule(const QString &filter, const QString &listID)
    : m_cssRule(false), m_exception(false), m_enabled(false)
{
    m_listID = listID;
    setFilter(filter);
}

CAdBlockRule &CAdBlockRule::operator=(const CAdBlockRule &other)
{
    m_listID = other.m_listID;
    setFilter(other.filter());
    return *this;
}

bool CAdBlockRule::operator==(const CAdBlockRule &s) const
{
    return ((m_filter==s.m_filter) && (m_listID==s.m_listID));
}

bool CAdBlockRule::operator!=(const CAdBlockRule &s) const
{
    return !operator==(s);
}

QDataStream &operator<<(QDataStream &out, const CAdBlockRule &obj)
{
    out << obj.m_filter << obj.m_listID;
    return out;
}

QDataStream &operator>>(QDataStream &in, CAdBlockRule &obj)
{
    in >> obj.m_filter >> obj.m_listID;
    obj.setFilter(obj.m_filter);
    return in;
}

QString CAdBlockRule::filter() const
{
    return m_filter;
}

void CAdBlockRule::setFilter(const QString &filter)
{
    m_filter = filter;

    m_cssRule = false;
    m_enabled = true;
    m_exception = false;
    m_plainRule.clear();
    bool regExpRule = false;

    if (filter.startsWith(QLatin1String("!"))
            || filter.trimmed().isEmpty())
        m_enabled = false;

    if (filter.contains(QLatin1String("##")) || filter.contains(QLatin1String("#@#")))
        m_cssRule = true;

    QString parsedLine = filter;
    if (parsedLine.startsWith(QLatin1String("@@"))) {
        m_exception = true;
        parsedLine = parsedLine.mid(2);
    }
    if (parsedLine.startsWith(QLatin1Char('/'))) {
        if (parsedLine.endsWith(QLatin1Char('/'))) {
            parsedLine = parsedLine.mid(1);
            parsedLine = parsedLine.left(parsedLine.size() - 1);
            regExpRule = true;
        }
    }
    int options = parsedLine.indexOf(QLatin1String("$"), 0);
    if (options >= 0) {
        m_options = parsedLine.mid(options + 1).split(QLatin1Char(','));
        parsedLine = parsedLine.left(options);
    }

    bool hasWildcards = parsedLine.contains(QRegExp(QStringLiteral("[\\*\\$]")));
    if (!regExpRule && m_options.isEmpty() && !hasWildcards &&
        (!parsedLine.contains('^') || parsedLine.endsWith('^'))) {
        m_plainRule = parsedLine;
        if (m_plainRule.startsWith(QStringLiteral("||")))
            m_plainRule = m_plainRule.mid(2);
        if (m_plainRule.endsWith('^'))
            m_plainRule = m_plainRule.left(m_plainRule.length()-1);
    }

    setPattern(parsedLine, regExpRule);

    if (m_options.contains(QStringLiteral("match-case"))) {
        m_regExp.setCaseSensitivity(Qt::CaseSensitive);
        m_options.removeOne(QStringLiteral("match-case"));
    }
}

QString CAdBlockRule::listID() const
{
    if (m_listID.isEmpty())
        return QObject::tr("User list");

    return m_listID;
}

bool CAdBlockRule::networkMatch(const QString &encodedUrl) const
{
    if (m_cssRule)
        return false;

    if (!m_enabled)
        return false;

    if (!m_plainRule.isEmpty())
        if (encodedUrl.contains(m_plainRule,Qt::CaseInsensitive))
            return true;

    bool matched = encodedUrl.contains(m_regExp);

    if (matched
            && !m_options.isEmpty()) {

        // we only support domain right now
        if (m_options.count() == 1) {
            QUrl url = QUrl::fromEncoded(encodedUrl.toUtf8());
            QString host = url.host();
            for (const QString &option : qAsConst(m_options)) {
                if (option.startsWith(QLatin1String("domain="))) {
                    const QStringList domainOptions = option.mid(7).split(QLatin1Char('|'));
                    for (const QString& domainOption : qAsConst(domainOptions)) {
                        bool negate = domainOption.at(0) == QLatin1Char('~');
                        bool hostMatched;
                        if (negate)
                            hostMatched = domainOption.mid(1) == host;
                        else
                            hostMatched = domainOption == host;
                        if (hostMatched && !negate) {
#if defined(ADBLOCKRULE_DEBUG)
                            qDebug() << "CAdBlockRule::" << __FUNCTION__ << encodedUrl
                                     << "hostMatched && !negate" << filter();
#endif
                            return true;
                        }
                        if (!hostMatched && negate) {
#if defined(ADBLOCKRULE_DEBUG)
                            qDebug() << "CAdBlockRule::" << __FUNCTION__ << encodedUrl
                                     << "!hostMatched && negate" << filter();
#endif
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }
#if defined(ADBLOCKRULE_DEBUG)
    if (matched)
        qDebug() << "CAdBlockRule::" << __FUNCTION__ << encodedUrl << "MATCHED" << filter();
#endif

    return matched;
}

bool CAdBlockRule::isException() const
{
    return m_exception;
}

void CAdBlockRule::setException(bool exception)
{
    m_exception = exception;
}

bool CAdBlockRule::isEnabled() const
{
    return m_enabled;
}

void CAdBlockRule::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        m_filter = QStringLiteral("!") + m_filter;
    } else {
        m_filter = m_filter.mid(1);
    }
}

QString CAdBlockRule::regExpPattern() const
{
    return m_regExp.pattern();
}

void CAdBlockRule::setPattern(const QString &pattern, bool isRegExp)
{
    m_regExp = QRegExp(isRegExp ? pattern : convertPatternToRegExp(pattern),
                       Qt::CaseInsensitive, QRegExp::RegExp2);
}

