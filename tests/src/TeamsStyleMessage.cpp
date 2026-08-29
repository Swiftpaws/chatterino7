// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "messages/layouts/TeamsStyleMessage.hpp"

#include <gtest/gtest.h>

using namespace chatterino;

TEST(TeamsStyleMessage, AuthorRowOrder)
{
    EXPECT_EQ(teamsAuthorRole(MessageElementFlag::BadgeSubscription),
              TeamsAuthorRole::Badge);
    EXPECT_EQ(teamsAuthorRole(MessageElementFlag::Username),
              TeamsAuthorRole::Username);
    EXPECT_EQ(teamsAuthorRole(MessageElementFlag::Timestamp),
              TeamsAuthorRole::Timestamp);
    EXPECT_EQ(teamsAuthorRole(MessageElementFlag::Text),
              TeamsAuthorRole::None);
}

TEST(TeamsStyleMessage, BadgeGate)
{
    MessageElementFlags flags{MessageElementFlag::Text,
                              MessageElementFlag::BadgeSubscription,
                              MessageElementFlag::BadgeVanity};

    const auto shown = applyTeamsBadgeGate(flags, true);
    EXPECT_TRUE(shown.has(MessageElementFlag::BadgeSubscription));
    EXPECT_TRUE(shown.has(MessageElementFlag::BadgeVanity));

    const auto hidden = applyTeamsBadgeGate(flags, false);
    EXPECT_FALSE(hidden.hasAny({MessageElementFlag::Badges}));
    EXPECT_TRUE(hidden.has(MessageElementFlag::Text));
}
