// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWindow.hpp"

#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>

namespace chatterino {

class PixmapButton;
class LabelButton;
class Theme;
class UpdateDialog;
class SplitNotebook;
class Channel;
class TeamsChatHeader;
class TeamsTabStrip;
class Split;

/**
 * @exposeenum c2.WindowType
 */
enum class WindowType { Main, Popup, Attached };

class Window : public BaseWindow
{
    Q_OBJECT

public:
    explicit Window(WindowType type, QWidget *parent);

    WindowType getType();
    bool isTeamsStyle() const;
    void bindTeamsSplit(Split *split);
    void openTeamsNewChat();
    SplitNotebook &getNotebook();

    void setPopupID(size_t id);
    std::optional<size_t> popupID() const;

    pajlada::Signals::NoArgSignal closed;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    void themeChangedEvent() override;

private:
    void addCustomTitlebarButtons();
    void addDebugStuff(
        std::map<QString, std::function<QString(std::vector<QString>)>>
            &actions);
    void addShortcuts() override;
    void addLayout();
    void showTeamsMenu(QWidget *anchor);
    void updateTeamsIdentity();
    void syncTeamsFromNotebook();
    void updateTeamsTabTitles();
    void onAccountSelected();
    void addMenuBar();

    WindowType type_;
    bool teamsStyle_ = false;

    SplitNotebook *notebook_;
    TeamsChatHeader *teamsHeader_ = nullptr;
    TeamsTabStrip *teamsTabStrip_ = nullptr;
    QWidget *teamsTitlebarMore_ = nullptr;
    Split *teamsSplit_ = nullptr;
    LabelButton *userLabel_ = nullptr;
    std::shared_ptr<UpdateDialog> updateDialogHandle_;

    pajlada::Signals::SignalHolder signalHolder_;
    pajlada::Signals::SignalHolder teamsChannelSignalHolder_;
    pajlada::Signals::SignalHolder teamsSplitSignalHolder_;

    // this is only used on Windows and only on the main window, for the one used otherwise, see SplitNotebook in Notebook.hpp
    PixmapButton *streamerModeTitlebarIcon_ = nullptr;
    void updateStreamerModeIcon();

    std::optional<size_t> popupID_;

    friend class Notebook;
};

}  // namespace chatterino
