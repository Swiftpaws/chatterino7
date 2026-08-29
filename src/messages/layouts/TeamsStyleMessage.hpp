// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "messages/MessageElement.hpp"

namespace chatterino {

enum class TeamsAuthorRole {
    None,
    Badge,
    Username,
    Timestamp,
};

inline TeamsAuthorRole teamsAuthorRole(MessageElementFlags flags)
{
    if (flags.hasAny({MessageElementFlag::Badges}))
    {
        return TeamsAuthorRole::Badge;
    }
    if (flags.has(MessageElementFlag::Username))
    {
        return TeamsAuthorRole::Username;
    }
    if (flags.has(MessageElementFlag::Timestamp))
    {
        return TeamsAuthorRole::Timestamp;
    }
    return TeamsAuthorRole::None;
}

inline MessageElementFlags applyTeamsBadgeGate(MessageElementFlags flags,
                                                bool showBadges)
{
    if (!showBadges)
    {
        flags.unset(MessageElementFlag::Badges);
    }
    return flags;
}

}  // namespace chatterino
