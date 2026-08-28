// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/SignalVectorModel.hpp"
#include "controllers/filters/OutgoingMessageFilter.hpp"

namespace chatterino {

class OutgoingMessageFilterModel
    : public SignalVectorModel<OutgoingMessageFilter>
{
public:
    explicit OutgoingMessageFilterModel(QObject *parent);
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role) override;

protected:
    OutgoingMessageFilter getItemFromRow(
        std::vector<QStandardItem *> &row,
        const OutgoingMessageFilter &original) override;
    void getRowFromItem(const OutgoingMessageFilter &item,
                        std::vector<QStandardItem *> &row) override;
};

}  // namespace chatterino
