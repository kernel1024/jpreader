/**
 * Project Arora ( https://github.com/arora/arora ). AdBlock facility.
 *
 * Copyright (c) 2009, Zsombor Gegesy <gzsombor@gmail.com>
 * Copyright (c) 2009, Benjamin C. Meyer <ben@meyerhome.net>
 *
 * Adaptation for JPReader by kernel1024
 **/

#ifndef CADBLOCKRULE_H
#define CADBLOCKRULE_H

#include <QObject>
#include <QString>
#include <QRegularExpression>
#include <QStringList>
#include <vector>

class CAdBlockRule
{
    friend QDataStream &operator<<(QDataStream &out, const CAdBlockRule &obj);
    friend QDataStream &operator>>(QDataStream &in, CAdBlockRule &obj);

public:
    CAdBlockRule() = default;
    CAdBlockRule(const CAdBlockRule& other) = default;
    CAdBlockRule(const QString &filter, const QString &listID);
    ~CAdBlockRule() = default;
    CAdBlockRule &operator=(const CAdBlockRule& other) = default;
    bool operator==(const CAdBlockRule &s) const;
    bool operator!=(const CAdBlockRule &s) const;

public:

    QString filter() const;
    void setFilter(const QString &filter);

    QString listID() const;

    bool isCSSRule() const { return m_cssRule; }
    bool networkMatch(const QString &encodedUrl) const;

    bool isException() const;
    void setException(bool exception);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    QString regExpPattern() const;
    void setPattern(const QString &pattern, bool isRegExp);

private:
    QString m_filter;

    QString m_listID;

    QRegularExpression m_regExp;
    QString m_plainRule;
    QStringList m_options;

    bool m_cssRule { false };
    bool m_exception { false };
    bool m_enabled { false };
};

using CAdBlockVector = std::vector<CAdBlockRule>;

namespace std {
template <>
struct hash<CAdBlockRule>
{
    std::size_t operator()(const CAdBlockRule& k) const
    {
        return (qHash(k.filter()) ^ (qHash(k.listID()) >> 1));
    }
};
}

Q_DECLARE_METATYPE(CAdBlockRule)

#endif // CADBLOCKRULE_H
