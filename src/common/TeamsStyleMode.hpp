// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/QStringHash.hpp"

#include <QString>
#include <QRegularExpression>

#include <unordered_map>
#include <utility>

namespace chatterino {

inline QString teamsWindowLayoutFileName(bool teamsStyleMode)
{
    return teamsStyleMode ? QStringLiteral("teams-window-layout.json")
                          : QStringLiteral("window-layout.json");
}

inline bool teamsShouldSeedLayout(bool teamsStyleMode,
                                  bool teamsLayoutExists)
{
    return teamsStyleMode && !teamsLayoutExists;
}

inline QString teamsChannelKey(QString provider, QString canonicalName)
{
    provider = provider.trimmed().toLower();
    canonicalName = canonicalName.trimmed().toLower();
    if (provider.isEmpty() || canonicalName.isEmpty())
    {
        return {};
    }
    return provider + ':' + canonicalName;
}

inline QString teamsResolvedIdentity(
    const std::unordered_map<QString, QString> &aliases, const QString &key,
    QString displayName, QString loginName)
{
    if (!key.isEmpty())
    {
        if (const auto alias = aliases.find(key); alias != aliases.end() &&
                                                   !alias->second.trimmed().isEmpty())
        {
            return alias->second.trimmed();
        }
    }
    displayName = displayName.trimmed();
    if (!displayName.isEmpty())
    {
        return displayName;
    }
    loginName = loginName.trimmed();
    return loginName.isEmpty() ? QStringLiteral("Chat") : loginName;
}

inline std::unordered_map<QString, QString> teamsAliasesWithRename(
    std::unordered_map<QString, QString> aliases, const QString &key,
    QString alias)
{
    if (key.isEmpty())
    {
        return aliases;
    }
    alias = alias.trimmed();
    if (alias.isEmpty())
    {
        aliases.erase(key);
    }
    else
    {
        aliases.insert_or_assign(key, std::move(alias));
    }
    return aliases;
}

inline QString teamsIdentityInitials(const QString &identity)
{
    const auto words = identity.split(QRegularExpression("\\s+"),
                                      Qt::SkipEmptyParts);
    QString initials;
    for (const auto &word : words)
    {
        for (const auto ch : word)
        {
            if (ch.isLetterOrNumber())
            {
                initials += ch.toUpper();
                break;
            }
        }
        if (initials.size() == 2)
        {
            break;
        }
    }
    if (initials.size() < 2 && words.size() == 1)
    {
        for (const auto ch : words.front())
        {
            if (ch.isLetterOrNumber() &&
                (initials.isEmpty() || ch.toUpper() != initials.front()))
            {
                initials += ch.toUpper();
            }
            if (initials.size() == 2)
            {
                break;
            }
        }
    }
    return initials.isEmpty() ? QStringLiteral("C") : initials;
}

}  // namespace chatterino
