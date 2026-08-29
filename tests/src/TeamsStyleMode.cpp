// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "common/TeamsStyleMode.hpp"

#include <gtest/gtest.h>

using namespace chatterino;

TEST(TeamsStyleMode, LayoutFileSelection)
{
    EXPECT_EQ(teamsWindowLayoutFileName(false), "window-layout.json");
    EXPECT_EQ(teamsWindowLayoutFileName(true), "teams-window-layout.json");
    EXPECT_TRUE(teamsShouldSeedLayout(true, false));
    EXPECT_FALSE(teamsShouldSeedLayout(true, true));
    EXPECT_FALSE(teamsShouldSeedLayout(false, false));
}

TEST(TeamsStyleMode, CanonicalChannelKey)
{
    EXPECT_EQ(teamsChannelKey(" Twitch ", " ForSen "), "twitch:forsen");
    EXPECT_EQ(teamsChannelKey("kick", "Some_Channel"),
              "kick:some_channel");
    EXPECT_TRUE(teamsChannelKey("twitch", "  ").isEmpty());
}

TEST(TeamsStyleMode, AliasLookupAndRemoval)
{
    std::unordered_map<QString, QString> aliases{
        {"twitch:forsen", " Project Chat "},
    };
    EXPECT_EQ(teamsResolvedIdentity(aliases, "twitch:forsen", "Forsen",
                                    "forsen"),
              "Project Chat");
    EXPECT_EQ(teamsResolvedIdentity(aliases, "twitch:other", "Other", "other"),
              "Other");
    EXPECT_EQ(teamsResolvedIdentity({}, {}, {}, {}), "Chat");

    aliases = teamsAliasesWithRename(std::move(aliases), "twitch:forsen", " ");
    EXPECT_FALSE(aliases.contains("twitch:forsen"));
}

TEST(TeamsStyleMode, IdentityInitials)
{
    EXPECT_EQ(teamsIdentityInitials("Project Chat"), "PC");
    EXPECT_EQ(teamsIdentityInitials("forsen"), "FO");
    EXPECT_EQ(teamsIdentityInitials("  42 room "), "4R");
    EXPECT_EQ(teamsIdentityInitials("---"), "C");
}
