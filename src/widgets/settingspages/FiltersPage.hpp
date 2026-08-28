// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

#include <QStringListModel>

class QVBoxLayout;

namespace chatterino {

class EditableModelView;

class FiltersPage : public SettingsPage
{
public:
    FiltersPage();

    void onShow() final;
    bool filterElements(const QString &query) override;

private:
    void channelTableCellClicked(const QModelIndex &clicked,
                                 EditableModelView *view);
    void outgoingTableCellClicked(const QModelIndex &clicked,
                                  EditableModelView *view);

    QStringListModel userListModel_;
    EditableModelView *channelView_{};
    EditableModelView *outgoingView_{};
};

}  // namespace chatterino
