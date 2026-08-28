// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitInput.hpp"

#include "common/Literals.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/Command.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/filters/OutgoingMessageFilter.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/EmoteController.hpp"
#include "providers/kick/KickChannel.hpp"
#include "providers/kick/KickChatServer.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"

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

    KickChatServer *getKickChatServer() override
    {
        return &this->kickChatServer;
    }

    HotkeyController hotkeys;
    WindowManager windowManager;
    AccountController accounts;
    CommandController commands;
    mock::EmoteController emotes;
    KickChatServer kickChatServer;
};

class SplitInputTest : public ::testing::Test
{
public:
    class TestChannel : public Channel
    {
    public:
        TestChannel()
            : Channel("test", Type::None)
        {
        }

        void sendMessage(const QString &message) override
        {
            this->sentMessages.push_back(message);
        }

        std::vector<QString> sentMessages;
    };

    class TestSplitInput : public SplitInput
    {
    public:
        using SplitInput::SplitInput;

        void send()
        {
            this->handleSendMessage({});
        }

        bool hasReply() const
        {
            return this->replyTarget_ != nullptr;
        }
    };

    SplitInputTest()
        : channel(std::make_shared<TestChannel>())
        , split(new Split(nullptr))
        , input(this->split)
    {
        this->split->setChannel(IndirectChannel(this->channel));
    }

    void SetUp() override
    {
        this->clearOutgoingFilters();
    }

    void TearDown() override
    {
        this->clearOutgoingFilters();
    }

    void clearOutgoingFilters()
    {
        auto &filters = getSettings()->outgoingMessageFilters;
        while (!filters.raw().empty())
        {
            filters.removeAt(0);
        }
    }

    MockApplication mockApplication;
    std::shared_ptr<TestChannel> channel;
    Split *split;
    TestSplitInput input;
};

class SplitInputReplyTest
    : public SplitInputTest,
      public ::testing::WithParamInterface<std::tuple<QString, QString>>
{
};

}  // namespace

TEST_P(SplitInputReplyTest, Reply)
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

TEST_F(SplitInputTest, MatchingOutgoingFilterRetainsInput)
{
    getSettings()->outgoingMessageFilters.append({
        .enabled = true,
        .pattern = "blocked",
    });
    this->input.setInputText("this is blocked");

    this->input.send();

    EXPECT_TRUE(this->channel->sentMessages.empty());
    EXPECT_EQ(this->input.getInputText(), "this is blocked");
    ASSERT_EQ(this->channel->countMessages(), 1);
    EXPECT_EQ(this->channel->getLastMessage()->messageText,
              "Message not sent: matched outgoing filter rule 1.");
}

TEST_F(SplitInputTest, NonMatchingOutgoingFilterSendsAndClears)
{
    getSettings()->outgoingMessageFilters.append({
        .enabled = true,
        .pattern = "blocked",
    });
    this->input.setInputText("allowed");

    this->input.send();

    ASSERT_EQ(this->channel->sentMessages.size(), 1);
    EXPECT_EQ(this->channel->sentMessages[0], "allowed");
    EXPECT_TRUE(this->input.getInputText().isEmpty());
}

TEST_F(SplitInputTest, MatchingOutgoingFilterRetainsReply)
{
    getSettings()->outgoingMessageFilters.append({
        .enabled = true,
        .pattern = "blocked",
    });
    auto reply = std::make_shared<Message>();
    reply->displayName = "forsen";
    auto replyChannel = std::make_shared<KickChannel>("test");
    this->split->setChannel(IndirectChannel(replyChannel));
    this->input.setReply(reply, replyChannel);
    this->input.setInputText("@forsen blocked");

    this->input.send();

    ASSERT_EQ(replyChannel->countMessages(), 1);
    EXPECT_EQ(replyChannel->getLastMessage()->messageText,
              "Message not sent: matched outgoing filter rule 1.");
    EXPECT_EQ(this->input.getInputText(), "@forsen blocked");
    EXPECT_TRUE(this->input.hasReply());
}

TEST_F(SplitInputTest, FiltersCommandExpandedText)
{
    this->mockApplication.commands.items.append(
        Command{"/expand", "blocked output"});
    getSettings()->outgoingMessageFilters.append({
        .enabled = true,
        .pattern = "blocked output",
    });
    this->input.setInputText("/expand");

    this->input.send();

    EXPECT_TRUE(this->channel->sentMessages.empty());
    EXPECT_EQ(this->input.getInputText(), "/expand");
}

INSTANTIATE_TEST_SUITE_P(
    SplitInput, SplitInputReplyTest,
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
