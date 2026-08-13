// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/OutgoingMessageFilterModel.hpp"

#include "util/StandardItemHelper.hpp"

#include <QRegularExpression>

namespace chatterino {

namespace {

bool isValid(const QString &pattern)
{
    const QRegularExpression regex(
        pattern,
        QRegularExpression::PatternOption::UseUnicodePropertiesOption);
    return !pattern.isEmpty() && regex.isValid();
}

void updateValidItem(QStandardItem *item, const QString &pattern)
{
    const bool valid = isValid(pattern);
    setBoolItem(item, valid, false, false);
    setStringItem(item, valid ? "Valid" : "Show errors", false, false);
}

}  // namespace

OutgoingMessageFilterModel::OutgoingMessageFilterModel(QObject *parent)
    : SignalVectorModel<OutgoingMessageFilter>(3, parent)
{
}

bool OutgoingMessageFilterModel::setData(const QModelIndex &index,
                                         const QVariant &value, int role)
{
    const bool changed =
        SignalVectorModel<OutgoingMessageFilter>::setData(index, value, role);
    if (changed && index.column() == 1)
    {
        const auto validIndex = index.siblingAtColumn(2);
        Q_EMIT this->dataChanged(
            validIndex, validIndex, {Qt::DisplayRole, Qt::CheckStateRole});
    }
    return changed;
}

OutgoingMessageFilter OutgoingMessageFilterModel::getItemFromRow(
    std::vector<QStandardItem *> &row, const OutgoingMessageFilter &original)
{
    (void)original;
    const auto pattern = row[1]->data(Qt::DisplayRole).toString();
    updateValidItem(row[2], pattern);
    return {
        .enabled = row[0]->data(Qt::CheckStateRole).toBool(),
        .pattern = pattern,
    };
}

void OutgoingMessageFilterModel::getRowFromItem(
    const OutgoingMessageFilter &item, std::vector<QStandardItem *> &row)
{
    setBoolItem(row[0], item.enabled);
    setStringItem(row[1], item.pattern);
    updateValidItem(row[2], item.pattern);
}

}  // namespace chatterino
