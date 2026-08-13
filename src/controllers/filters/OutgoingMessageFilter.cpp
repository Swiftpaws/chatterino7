// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/OutgoingMessageFilter.hpp"

#include <QRegularExpression>

namespace chatterino {

std::optional<OutgoingMessageFilterBlock> filterOutgoingMessage(
    const QString &message, const std::vector<OutgoingMessageFilter> &filters)
{
    for (std::size_t i = 0; i < filters.size(); ++i)
    {
        const auto &filter = filters[i];
        if (!filter.enabled)
        {
            continue;
        }

        QRegularExpression regex(
            filter.pattern,
            QRegularExpression::PatternOption::UseUnicodePropertiesOption);
        if (filter.pattern.isEmpty() || !regex.isValid())
        {
            return OutgoingMessageFilterBlock{
                i + 1, OutgoingMessageFilterBlockReason::Invalid};
        }
        if (regex.match(message).hasMatch())
        {
            return OutgoingMessageFilterBlock{
                i + 1, OutgoingMessageFilterBlockReason::Match};
        }
    }

    return std::nullopt;
}

}  // namespace chatterino
