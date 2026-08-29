// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/TeamsStyle.hpp"

#include <gtest/gtest.h>

using namespace chatterino;

TEST(TeamsStyle, PaletteTokens)
{
    const auto light = teamsStylePalette(true);
    EXPECT_EQ(light.frame, QColor("#F4F1F2"));
    EXPECT_EQ(light.canvas, QColor("#FFFFFF"));
    EXPECT_EQ(light.primaryText, QColor("#242424"));
    EXPECT_EQ(light.accent, QColor("#5B5FC7"));

    const auto dark = teamsStylePalette(false);
    EXPECT_EQ(dark.frame, QColor("#222226"));
    EXPECT_EQ(dark.canvas, QColor("#2B2B30"));
    EXPECT_EQ(dark.raisedSurface, QColor("#404045"));
    EXPECT_EQ(dark.primaryText, QColor("#FFFFFF"));
    EXPECT_EQ(dark.accent, QColor("#9FA4FE"));
}

TEST(TeamsStyle, SemanticContrast)
{
    for (const bool light : {false, true})
    {
        const auto palette = teamsStylePalette(light);
        EXPECT_GE(teamsContrastRatio(palette.primaryText, palette.canvas),
                  4.5);
        EXPECT_GE(teamsContrastRatio(palette.secondaryText, palette.canvas),
                  4.5);
        EXPECT_GE(teamsContrastRatio(palette.accent, palette.canvas), 4.5);
        EXPECT_GE(
            teamsContrastRatio(palette.controlBorder, palette.canvas), 3.0);
        EXPECT_GE(teamsContrastRatio(palette.controlBorder,
                                     palette.raisedSurface),
                  3.0);
    }
}

TEST(TeamsStyle, ContrastCorrectionPreservesHue)
{
    const auto light = teamsStylePalette(true);
    const QColor lowContrast("#A2BEA8");
    const auto corrected = ensureTeamsContrast(
        lowContrast, light.canvas, 4.5, light.primaryText);

    EXPECT_GE(teamsContrastRatio(corrected, light.canvas), 4.5);
    EXPECT_NE(corrected, light.primaryText);
    EXPECT_NEAR(corrected.hslHueF(), lowContrast.hslHueF(), 0.001);
}

TEST(TeamsStyle, ParticipantCountFormatting)
{
    EXPECT_EQ(teamsCompactParticipantCount(0), "0");
    EXPECT_EQ(teamsCompactParticipantCount(999), "999");
    EXPECT_EQ(teamsCompactParticipantCount(1200), "1.2K");
    EXPECT_EQ(teamsCompactParticipantCount(12000), "12K");
    EXPECT_EQ(teamsCompactParticipantCount(1250000), "1.3M");

    EXPECT_EQ(teamsParticipantLabel(1), "1 participant");
    EXPECT_EQ(teamsParticipantLabel(1234), "1,234 participants");
    EXPECT_EQ(teamsParticipantOverflowCount(0), 0);
    EXPECT_EQ(teamsParticipantOverflowCount(1), 0);
    EXPECT_EQ(teamsParticipantOverflowCount(5), 4);
}

TEST(TeamsStyle, ComposerFrameKeepsCompleteOutlineInsideWidget)
{
    for (const auto scale : {1.0, 1.25, 1.5})
    {
        const QRectF widgetRect(0, 0, 685 * scale, 58 * scale);
        const auto frameRect = teamsComposerFrameRect(widgetRect);
        EXPECT_GT(frameRect.left(), widgetRect.left());
        EXPECT_GT(frameRect.top(), widgetRect.top());
        EXPECT_LT(frameRect.right(), widgetRect.right());
        EXPECT_LT(frameRect.bottom(), widgetRect.bottom());
    }
}

TEST(TeamsStyle, DisabledComposerIconIsDistinctFromEnabledAccent)
{
    for (const bool light : {false, true})
    {
        const auto palette = teamsStylePalette(light);
        const auto disabled = teamsDisabledIconColor(palette);
        EXPECT_NE(disabled, palette.accent);
        EXPECT_LT(teamsContrastRatio(disabled, palette.raisedSurface),
                  teamsContrastRatio(palette.accent, palette.raisedSurface));
    }
}

TEST(TeamsStyle, TeamsTabStripVisibilityAndOverflow)
{
    EXPECT_FALSE(teamsShouldShowTabStrip(0));
    EXPECT_FALSE(teamsShouldShowTabStrip(1));
    EXPECT_TRUE(teamsShouldShowTabStrip(2));
    EXPECT_TRUE(teamsShouldShowTabStrip(8));

    EXPECT_EQ(teamsTabScrollOffset(-10, 900, 600), 0);
    EXPECT_EQ(teamsTabScrollOffset(120, 900, 600), 120);
    EXPECT_EQ(teamsTabScrollOffset(800, 900, 600), 300);
    EXPECT_EQ(teamsTabScrollOffset(40, 500, 600), 0);
}
