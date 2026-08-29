// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/Theme.hpp"

#include <QColor>
#include <QLocale>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <cmath>

namespace chatterino {

inline QString teamsCompactParticipantCount(int count)
{
    if (count < 1000)
    {
        return QString::number(count);
    }

    const auto compact = [](double value, QChar suffix) {
        const auto precision = value < 10.0 ? 1 : 0;
        auto number = QString::number(value, 'f', precision);
        if (number.endsWith(".0"))
        {
            number.chop(2);
        }
        return number + suffix;
    };

    if (count < 1000000)
    {
        return compact(count / 1000.0, u'K');
    }
    return compact(count / 1000000.0, u'M');
}

inline QString teamsParticipantLabel(int count)
{
    static const QLocale english(QLocale::English, QLocale::UnitedStates);
    return english.toString(count) +
           (count == 1 ? QStringLiteral(" participant")
                       : QStringLiteral(" participants"));
}

inline int teamsParticipantOverflowCount(int total)
{
    return std::max(total - 1, 0);
}

inline QRectF teamsComposerFrameRect(const QRectF &rect)
{
    return rect.adjusted(0.75, 0.75, -0.75, -0.75);
}

inline bool teamsShouldShowTabStrip(int pageCount)
{
    return pageCount >= 2;
}

inline int teamsTabScrollOffset(int requested, int contentWidth,
                                int viewportWidth)
{
    return std::clamp(requested, 0,
                      std::max(0, contentWidth - viewportWidth));
}

struct TeamsStylePalette {
    QColor frame;
    QColor canvas;
    QColor raisedSurface;
    QColor primaryText;
    QColor secondaryText;
    QColor divider;
    QColor accent;
    QColor selection;
    QColor controlBorder;
    QColor icon;
    QColor hover;
};

inline QColor compositeTeamsColor(const QColor &foreground,
                                  const QColor &background)
{
    const auto alpha = foreground.alphaF();
    return QColor::fromRgbF(
        foreground.redF() * alpha + background.redF() * (1 - alpha),
        foreground.greenF() * alpha + background.greenF() * (1 - alpha),
        foreground.blueF() * alpha + background.blueF() * (1 - alpha));
}

inline QColor teamsDisabledIconColor(const TeamsStylePalette &palette)
{
    auto disabled = palette.primaryText;
    disabled.setAlphaF(0.38F);
    return compositeTeamsColor(disabled, palette.raisedSurface);
}

inline TeamsStylePalette teamsStylePalette(bool light)
{
    if (light)
    {
        return {
            .frame = QColor("#F4F1F2"),
            .canvas = QColor("#FFFFFF"),
            .raisedSurface = QColor("#F5F5F5"),
            .primaryText = QColor("#242424"),
            .secondaryText = QColor("#616161"),
            .divider = QColor("#E5E5E5"),
            .accent = QColor("#5B5FC7"),
            .selection = QColor("#E4E3FF"),
            .controlBorder = QColor("#898989"),
            .icon = QColor("#424242"),
            .hover = QColor("#E8E8E8"),
        };
    }

    const QColor canvas("#2B2B30");
    return {
        .frame = QColor("#222226"),
        .canvas = canvas,
        .raisedSurface = QColor("#404045"),
        .primaryText = QColor("#FFFFFF"),
        .secondaryText = compositeTeamsColor(QColor(255, 255, 255, 189),
                                             canvas),
        .divider = compositeTeamsColor(QColor(255, 255, 255, 36), canvas),
        .accent = QColor("#9FA4FE"),
        .selection = compositeTeamsColor(QColor(159, 164, 254, 102), canvas),
        .controlBorder = QColor("#96969B"),
        .icon = QColor("#FFFFFF"),
        .hover = QColor("#48484D"),
    };
}

inline TeamsStylePalette currentTeamsStylePalette()
{
    return teamsStylePalette(getTheme()->isLightTheme());
}

inline qreal teamsRelativeLuminance(const QColor &color)
{
    const auto channel = [](qreal value) {
        return value <= 0.04045 ? value / 12.92
                                : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.redF()) +
           0.7152 * channel(color.greenF()) +
           0.0722 * channel(color.blueF());
}

inline qreal teamsContrastRatio(QColor foreground, const QColor &background)
{
    if (foreground.alpha() != 255)
    {
        foreground = compositeTeamsColor(foreground, background);
    }
    const auto light = std::max(teamsRelativeLuminance(foreground),
                                teamsRelativeLuminance(background));
    const auto dark = std::min(teamsRelativeLuminance(foreground),
                               teamsRelativeLuminance(background));
    return (light + 0.05) / (dark + 0.05);
}

inline QColor ensureTeamsContrast(QColor color, const QColor &background,
                                  qreal minimumRatio,
                                  const QColor &fallback)
{
    color.setAlpha(255);
    if (teamsContrastRatio(color, background) >= minimumRatio)
    {
        return color;
    }

    float hue = 0;
    float saturation = 0;
    float lightness = 0;
    float alpha = 0;
    color.getHslF(&hue, &saturation, &lightness, &alpha);
    if (hue < 0)
    {
        saturation = 0;
    }

    const auto blackRatio = teamsContrastRatio(Qt::black, background);
    const auto whiteRatio = teamsContrastRatio(Qt::white, background);
    const qreal target = whiteRatio > blackRatio ? 1.0 : 0.0;
    qreal low = 0;
    qreal high = 1;
    QColor best;
    for (int i = 0; i < 24; ++i)
    {
        const qreal amount = (low + high) / 2;
        const qreal candidateLightness =
            lightness + (target - lightness) * amount;
        auto candidate = QColor::fromHslF(
            hue < 0 ? 0 : hue, saturation, candidateLightness, 1);
        if (teamsContrastRatio(candidate, background) >= minimumRatio)
        {
            best = candidate;
            high = amount;
        }
        else
        {
            low = amount;
        }
    }
    return best.isValid() ? best : fallback;
}

}  // namespace chatterino
