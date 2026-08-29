// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitInput.hpp"

#include "common/Literals.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/Command.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/EmoteController.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/buttons/SvgButton.hpp"
#include "widgets/splits/Split.hpp"

#include <QApplication>
#include <QDebug>
#include <QString>

using namespace chatterino;
using ::testing::Exactly;

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication()
        : windowManager(this->args_, this->paths_, this->settings, this->theme,
                        this->fonts)
        , commands(this->paths_)
    {
    }

    HotkeyController *getHotkeys() override
    {
        return &this->hotkeys;
    }

    WindowManager *getWindows() override
    {
        return &this->windowManager;
    }

    AccountController *getAccounts() override
    {
        return &this->accounts;
    }

    CommandController *getCommands() override
    {
        return &this->commands;
    }

    EmoteController *getEmotes() override
    {
        return &this->emotes;
    }

    HotkeyController hotkeys;
    WindowManager windowManager;
    AccountController accounts;
    CommandController commands;
    mock::EmoteController emotes;
};

class SplitInputTest
    : public ::testing::TestWithParam<std::tuple<QString, QString>>
{
public:
    SplitInputTest()
        : split(new Split(nullptr))
        , input(this->split)
    {
    }

    MockApplication mockApplication;
    Split *split;
    SplitInput input;
};

}  // namespace

TEST(SplitInputTeams, ComposerFrameAndActions)
{
    MockApplication mockApplication;
    auto *split = new Split(nullptr);
    SplitInput input(split);
    input.resize(773, 110);
    input.setTeamsStyle(true);
    auto *frame = input.findChild<QWidget *>("teamsComposerFrame");
    ASSERT_NE(frame, nullptr);
    auto *textEdit = frame->findChild<QTextEdit *>();
    ASSERT_NE(textEdit, nullptr);
    for (const auto scale : {1.F, 1.25F, 1.5F})
    {
        input.setOverrideScale(scale);
        frame->resize(400, int(58 * scale));
        QApplication::processEvents();
        EXPECT_EQ(frame->minimumHeight(), int(58 * scale));
        EXPECT_EQ(frame->layout()->contentsMargins().top(), int(8 * scale));
        EXPECT_EQ(textEdit->font().family(), "Segoe UI");
        EXPECT_DOUBLE_EQ(textEdit->font().pointSizeF(), 11.0 * scale);
    }

    SvgButton *teamsEmoji = nullptr;
    SvgButton *teamsSend = nullptr;
    for (auto *button : input.findChildren<SvgButton *>())
    {
        if (button->accessibleName() == "Emoji")
        {
            teamsEmoji = button;
        }
        else if (button->accessibleName() == "Send message")
        {
            teamsSend = button;
        }
    }

    ASSERT_NE(teamsEmoji, nullptr);
    ASSERT_NE(teamsSend, nullptr);
    EXPECT_FALSE(teamsEmoji->isHidden());
    EXPECT_FALSE(teamsSend->isHidden());
    EXPECT_FALSE(teamsSend->enabled());
    EXPECT_EQ(teamsEmoji->width(), 48);
    EXPECT_EQ(teamsSend->height(), 60);

    input.setInputText("hello");
    QApplication::processEvents();
    EXPECT_TRUE(teamsSend->enabled());
}

TEST_P(SplitInputTest, Reply)
{
    std::tuple<QString, QString> params = this->GetParam();
    auto [inputText, expected] = params;
    ASSERT_EQ("", this->input.getInputText());
    this->input.setInputText(inputText);
    ASSERT_EQ(inputText, this->input.getInputText());

    auto *message = new Message();
    message->displayName = "forsen";
    auto reply = MessagePtr(message);
    this->input.setReply(reply, {});
    QString actual = this->input.getInputText();
    ASSERT_EQ(expected, actual) << "Input text after setReply should be '"
                                << expected << "', but got '" << actual << "'";
}

INSTANTIATE_TEST_SUITE_P(
    SplitInput, SplitInputTest,
    testing::Values(
        // Ensure message is retained
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "Test message",
            // Expected text after replying to forsen
            "@forsen Test message "),

        // Ensure mention is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure mention with space is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen ",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure mention is stripped, retain message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen Test message",
            // Expected text after replying to forsen
            "@forsen Test message "),

        // Ensure mention with comma is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen,",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure mention with comma is stripped, retain message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen Test message",
            // Expected text after replying to forsen
            "@forsen Test message "),

        // Ensure mention with comma and space is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen, ",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure it works with no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "",
            // Expected text after replying to forsen
            "@forsen ")));
