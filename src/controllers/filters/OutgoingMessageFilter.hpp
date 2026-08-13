// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/RapidjsonHelpers.hpp"
#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/serialize.hpp>
#include <QString>

#include <cstddef>
#include <optional>
#include <vector>

namespace chatterino {

struct OutgoingMessageFilter {
    bool enabled{};
    QString pattern;

    bool operator==(const OutgoingMessageFilter &) const = default;
};

enum class OutgoingMessageFilterBlockReason {
    Invalid,
    Match,
};

struct OutgoingMessageFilterBlock {
    std::size_t ruleNumber;
    OutgoingMessageFilterBlockReason reason;
};

std::optional<OutgoingMessageFilterBlock> filterOutgoingMessage(
    const QString &message, const std::vector<OutgoingMessageFilter> &filters);

}  // namespace chatterino

namespace pajlada {

template <>
struct Serialize<chatterino::OutgoingMessageFilter> {
    static rapidjson::Value get(
        const chatterino::OutgoingMessageFilter &value,
        rapidjson::Document::AllocatorType &a)
    {
        rapidjson::Value ret(rapidjson::kObjectType);
        chatterino::rj::set(ret, "enabled", value.enabled, a);
        chatterino::rj::set(ret, "pattern", value.pattern, a);
        return ret;
    }
};

template <>
struct Deserialize<chatterino::OutgoingMessageFilter> {
    static chatterino::OutgoingMessageFilter get(const rapidjson::Value &value,
                                                  bool *error = nullptr)
    {
        chatterino::OutgoingMessageFilter filter;
        if (!value.IsObject())
        {
            PAJLADA_REPORT_ERROR(error)
            return filter;
        }

        chatterino::rj::getSafe(value, "enabled", filter.enabled);
        chatterino::rj::getSafe(value, "pattern", filter.pattern);
        return filter;
    }
};

}  // namespace pajlada
