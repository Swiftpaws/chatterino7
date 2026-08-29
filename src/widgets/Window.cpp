// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/Window.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/Common.hpp"
#include "common/Modes.hpp"
#include "common/QLogging.hpp"
#include "common/TeamsStyleMode.hpp"
#include "common/Version.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Resources.hpp"
#include "singletons/Settings.hpp"
#include "singletons/StreamerMode.hpp"
#include "singletons/Theme.hpp"
#include "singletons/Updates.hpp"
#include "singletons/WindowManager.hpp"
#include "util/RapidJsonSerializeQSize.hpp"
#include "widgets/AccountSwitchPopup.hpp"
#include "widgets/buttons/DimButton.hpp"
#include "widgets/buttons/InitUpdateButton.hpp"
#include "widgets/buttons/LabelButton.hpp"
#include "widgets/buttons/PixmapButton.hpp"
#include "widgets/buttons/TitlebarButton.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/dialogs/switcher/QuickSwitcherPopup.hpp"
#include "widgets/dialogs/UpdateDialog.hpp"
#include "widgets/dialogs/WelcomeDialog.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/helper/TeamsStyle.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/ClosedSplits.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#ifndef NDEBUG
#    include "providers/twitch/PubSubManager.hpp"
#    include "providers/twitch/PubSubMessages.hpp"
#    include "util/SampleData.hpp"

#    include <rapidjson/document.h>
#endif

#include <algorithm>

#include <QApplication>
#include <QDesktopServices>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QStandardItemModel>
#include <QSvgRenderer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace chatterino {

namespace {

class TeamsMoreButton final : public DimButton
{
public:
    explicit TeamsMoreButton(BaseWidget *parent = nullptr)
        : DimButton(parent)
    {
        this->setDim(Dim::None);
        this->setMouseEffectColor(QColor("#E8E8E8"));
    }

protected:
    void paintContent(QPainter &painter) override
    {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor color = this->property("frameForeground").value<QColor>();
        if (!color.isValid())
        {
            color = QColor("#242424");
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);

        const qreal radius = std::max<qreal>(1.25, 1.45 * this->scale());
        const qreal gap = 6 * this->scale();
        const QPointF center(this->width() / 2.0, this->height() / 2.0);
        for (int offset = -1; offset <= 1; ++offset)
        {
            painter.drawEllipse(
                QPointF(center.x() + offset * gap, center.y()), radius,
                radius);
        }

        if (this->property("teamsBordered").toBool())
        {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, 1));
            painter.drawRoundedRect(this->rect().adjusted(1, 1, -1, -1), 5,
                                    5);
        }
    }
};

class TeamsParticipantStack final : public QWidget
{
public:
    explicit TeamsParticipantStack(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        this->setFixedSize(54, 44);
    }

    void setInitials(QString initials)
    {
        this->initials_ = std::move(initials);
        this->update();
    }

    void setParticipantCount(std::optional<int> count)
    {
        this->participantCount_ = count;
        this->update();
    }

    void applyPalette(const TeamsStylePalette &palette)
    {
        this->background_ = palette.divider;
        this->foreground_ = ensureTeamsContrast(
            palette.primaryText, this->background_, 4.5,
            palette.primaryText);
        this->countBackground_ = palette.selection;
        this->countForeground_ = ensureTeamsContrast(
            palette.primaryText, this->countBackground_, 4.5,
            palette.primaryText);
        this->outline_ = palette.canvas;
        this->update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QFont font("Segoe UI", 11);
        painter.setFont(font);
        painter.setPen(Qt::NoPen);
        painter.setBrush(this->background_);
        painter.drawEllipse(QRectF(0, 0, 44, 44));
        painter.setPen(this->foreground_);
        painter.drawText(QRectF(0, 0, 44, 44), Qt::AlignCenter,
                         this->initials_);

        if (this->participantCount_)
        {
            const QRectF countRect(24, 27, 30, 16);
            painter.setPen(QPen(this->outline_, 2));
            painter.setBrush(this->countBackground_);
            painter.drawRoundedRect(countRect, 8, 8);

            QFont countFont("Segoe UI", 7, QFont::DemiBold);
            painter.setFont(countFont);
            painter.setPen(this->countForeground_);
            painter.drawText(
                countRect, Qt::AlignCenter,
                "+" + teamsCompactParticipantCount(
                          teamsParticipantOverflowCount(
                              *this->participantCount_)));
        }
    }

private:
    QString initials_ = "C";
    QColor background_{"#F5F5F5"};
    QColor foreground_{"#242424"};
    QColor countBackground_{"#E4E3FF"};
    QColor countForeground_{"#242424"};
    QColor outline_{"#FFFFFF"};
    std::optional<int> participantCount_;
};

}  // namespace

class TeamsChatHeader final : public BaseWidget
{
public:
    explicit TeamsChatHeader(QWidget *parent = nullptr)
        : BaseWidget(parent)
        , participantsStack_(new TeamsParticipantStack(this))
        , more_(new TeamsMoreButton(this))
    {
        this->setFixedHeight(64);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(30, 0, 28, 0);
        layout->setSpacing(10);

        layout->addWidget(this->participantsStack_);

        auto *identityLayout = new QVBoxLayout();
        identityLayout->setContentsMargins(0, 0, 0, 0);
        identityLayout->setSpacing(1);

        this->title_ = new QLabel("Project Chat", this);
        this->title_->setFont(QFont("Segoe UI", 15, QFont::DemiBold));
        this->title_->setStyleSheet("color: #242424; background: transparent;");
        identityLayout->addWidget(this->title_);

        this->participantCount_ = new QLabel(this);
        this->participantCount_->setFont(QFont("Segoe UI", 10));
        this->participantCount_->hide();
        identityLayout->addWidget(this->participantCount_);
        layout->addLayout(identityLayout);

        layout->addStretch(1);

        this->more_->setFixedSize(42, 42);
        this->more_->setProperty("teamsBordered", true);
        layout->addWidget(this->more_);
    }

    void setIdentity(const QString &identity, const QString &initials)
    {
        this->title_->setText(identity);
        this->participantsStack_->setInitials(initials);
    }

    void setParticipantCount(std::optional<int> count)
    {
        this->participantsStack_->setParticipantCount(count);
        const auto visible = count.has_value();
        this->participantCount_->setVisible(visible);
        if (visible)
        {
            this->participantCount_->setText(teamsParticipantLabel(*count));
        }
    }

    void applyPalette(const TeamsStylePalette &palette)
    {
        this->palette_ = palette;
        this->title_->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(palette.primaryText.name()));
        this->participantCount_->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(palette.secondaryText.name()));
        this->participantsStack_->applyPalette(palette);
        this->more_->setProperty("frameForeground", palette.icon);
        this->more_->setMouseEffectColor(palette.hover);
        this->more_->update();
        this->update();
    }

    TeamsMoreButton *moreButton() const
    {
        return this->more_;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(this->rect(), this->palette_.canvas);
        painter.fillRect(0, this->height() - 1, this->width(), 1,
                         this->palette_.divider);
    }

private:
    QLabel *title_ = nullptr;
    QLabel *participantCount_ = nullptr;
    TeamsParticipantStack *participantsStack_ = nullptr;
    TeamsMoreButton *more_ = nullptr;
    TeamsStylePalette palette_ = teamsStylePalette(true);
};

class TeamsTabStrip final : public BaseWidget
{
public:
    explicit TeamsTabStrip(SplitNotebook *notebook, Window *window)
        : BaseWidget(window)
        , notebook_(notebook)
        , dismissIcon_(QStringLiteral(":/buttons/teamsDismiss.svg"), this)
    {
        this->setMouseTracking(true);
        this->setFixedHeight(36);
        this->dismissIcon_.setAspectRatioMode(Qt::KeepAspectRatio);
    }

    void sync()
    {
        this->setVisible(
            teamsShouldShowTabStrip(this->notebook_->getPageCount()));
        this->clampScrollOffset();
        this->ensureSelectedVisible();
        this->update();
    }

    void applyPalette(const TeamsStylePalette &palette)
    {
        this->palette_ = palette;
        this->update();
    }

protected:
    void scaleChangedEvent(float scale) override
    {
        this->setFixedHeight(int(36 * std::max(1.F, scale)));
        this->sync();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        BaseWidget::resizeEvent(event);
        this->clampScrollOffset();
        this->ensureSelectedVisible();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(this->rect(), this->palette_.canvas);
        painter.fillRect(0, this->height() - 1, this->width(), 1,
                         this->palette_.divider);

        painter.save();
        painter.setClipRect(this->tabsViewport());
        const auto tabs = this->tabRects();
        QFont font("Segoe UI", int(10 * this->teamsScale()));
        painter.setFont(font);
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
        {
            const auto &entry = tabs[i];
            const auto selected =
                entry.page == this->notebook_->getSelectedPage();
            const auto hovered = i == this->hoveredIndex_;
            if (hovered)
            {
                painter.fillRect(entry.rect, this->palette_.hover);
            }

            auto textRect = entry.rect.adjusted(
                int(12 * this->teamsScale()), 0,
                int((hovered ? -30 : -12) * this->teamsScale()), 0);
            painter.setPen(selected ? this->palette_.primaryText
                                    : this->palette_.secondaryText);
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                             entry.title);

            if (selected)
            {
                painter.fillRect(entry.rect.left(),
                                 entry.rect.bottom() - int(2 * this->teamsScale()),
                                 entry.rect.width(), int(3 * this->teamsScale()),
                                 this->palette_.accent);
            }

            if (hovered)
            {
                const auto iconSide = int(12 * this->teamsScale());
                QRect iconRect(0, 0, iconSide, iconSide);
                iconRect.moveCenter(entry.closeRect.center());
                this->renderTintedIcon(painter, this->dismissIcon_,
                                       iconRect, this->palette_.icon);
            }
        }
        painter.restore();

    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
        {
            return;
        }
        const auto tabs = this->tabRects();
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
        {
            if (!tabs[i].rect.contains(event->pos()))
            {
                continue;
            }
            if (i == this->hoveredIndex_ &&
                tabs[i].closeRect.contains(event->pos()) &&
                this->notebook_->getPageCount() > 1)
            {
                this->notebook_->removePage(tabs[i].page);
                return;
            }
            this->notebook_->select(tabs[i].page);
            this->draggedPage_ = tabs[i].page;
            this->dragStart_ = event->pos();
            return;
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const auto tabs = this->tabRects();
        auto hovered = -1;
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
        {
            if (tabs[i].rect.contains(event->pos()))
            {
                hovered = i;
                break;
            }
        }
        if (hovered != this->hoveredIndex_)
        {
            this->hoveredIndex_ = hovered;
            this->update();
        }

        if (this->draggedPage_ == nullptr ||
            (event->pos() - this->dragStart_).manhattanLength() <
                QApplication::startDragDistance())
        {
            return;
        }
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
        {
            if (event->pos().x() < tabs[i].rect.center().x())
            {
                this->notebook_->rearrangePage(this->draggedPage_, i);
                this->dragStart_ = event->pos();
                return;
            }
        }
        this->notebook_->rearrangePage(
            this->draggedPage_, this->notebook_->getPageCount() - 1);
        this->dragStart_ = event->pos();
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        this->draggedPage_ = nullptr;
    }

    void leaveEvent(QEvent *) override
    {
        this->hoveredIndex_ = -1;
        this->update();
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const auto delta = event->angleDelta().y() != 0
                               ? event->angleDelta().y()
                               : event->angleDelta().x();
        this->scrollOffset_ -= delta / 120 * int(84 * this->teamsScale());
        this->clampScrollOffset();
        this->update();
        event->accept();
    }

private:
    float teamsScale() const
    {
        return std::max(1.F, this->scale());
    }

    struct TabRect {
        QWidget *page = nullptr;
        QString title;
        QRect rect;
        QRect closeRect;
    };

    QRect tabsViewport() const
    {
        return QRect(int(8 * this->teamsScale()), 0,
                     this->width() - int(16 * this->teamsScale()),
                     this->height());
    }

    std::vector<TabRect> tabRects() const
    {
        std::vector<TabRect> result;
        QFontMetrics metrics(QFont("Segoe UI", int(10 * this->teamsScale())));
        auto x = this->tabsViewport().left() - this->scrollOffset_;
        for (int i = 0; i < this->notebook_->getPageCount(); ++i)
        {
            auto *page = this->notebook_->getPageAt(i);
            auto *container = dynamic_cast<SplitContainer *>(page);
            QString title = QStringLiteral("Chat");
            if (container != nullptr && !container->getSplits().empty())
            {
                const auto channel =
                    container->getSplits().front()->getSelectedChannel();
                if (channel && !channel->isEmpty())
                {
                    const auto provider = channel->isTwitchChannel()
                                              ? QStringLiteral("twitch")
                                          : channel->isKickChannel()
                                              ? QStringLiteral("kick")
                                              : QStringLiteral("channel");
                    title = teamsResolvedIdentity(
                        getSettings()->teamsStyleAliases.getValue(),
                        teamsChannelKey(provider, channel->getName()),
                        channel->getDisplayName(), channel->getName());
                }
            }
            const auto width = std::clamp(
                metrics.horizontalAdvance(title) + int(38 * this->teamsScale()),
                int(84 * this->teamsScale()), int(180 * this->teamsScale()));
            const QRect rect(x, 0, width, this->height() - 1);
            const auto closeSide = int(28 * this->teamsScale());
            QRect closeRect(rect.right() - closeSide, rect.top(), closeSide,
                            rect.height());
            result.push_back({page, title, rect, closeRect});
            x += width + int(2 * this->teamsScale());
        }
        return result;
    }

    int maximumScrollOffset() const
    {
        const auto tabs = this->tabRects();
        if (tabs.empty())
        {
            return 0;
        }
        const auto contentWidth = tabs.back().rect.right() +
                                  this->scrollOffset_ -
                                  this->tabsViewport().left();
        return std::max(0, contentWidth - this->tabsViewport().width());
    }

    void clampScrollOffset()
    {
        this->scrollOffset_ = teamsTabScrollOffset(
            this->scrollOffset_, this->maximumScrollOffset() +
                                     this->tabsViewport().width(),
            this->tabsViewport().width());
    }

    void ensureSelectedVisible()
    {
        for (const auto &entry : this->tabRects())
        {
            if (entry.page != this->notebook_->getSelectedPage())
            {
                continue;
            }
            if (entry.rect.left() < this->tabsViewport().left())
            {
                this->scrollOffset_ -=
                    this->tabsViewport().left() - entry.rect.left();
            }
            else if (entry.rect.right() > this->tabsViewport().right())
            {
                this->scrollOffset_ +=
                    entry.rect.right() - this->tabsViewport().right();
            }
            this->clampScrollOffset();
            return;
        }
    }

    static void renderTintedIcon(QPainter &painter, QSvgRenderer &renderer,
                                 const QRect &bounds, const QColor &color)
    {
        QImage image(bounds.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter iconPainter(&image);
        renderer.render(&iconPainter, image.rect());
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(image.rect(), color);
        iconPainter.end();
        painter.drawImage(bounds.topLeft(), image);
    }

    SplitNotebook *notebook_ = nullptr;
    QSvgRenderer dismissIcon_;
    TeamsStylePalette palette_ = teamsStylePalette(true);
    QWidget *draggedPage_ = nullptr;
    QPoint dragStart_;
    int hoveredIndex_ = -1;
    int scrollOffset_ = 0;
};

Window::Window(WindowType type, QWidget *parent)
    : BaseWindow(
          {BaseWindow::EnableCustomFrame, BaseWindow::ClearBuffersOnDpiChange},
          parent)
    , type_(type)
    , teamsStyle_(type == WindowType::Main &&
                  getSettings()->teamsStyleMode.getValue())
    , notebook_(new SplitNotebook(this))
{
    this->addCustomTitlebarButtons();
    if (this->teamsStyle_)
    {
        this->setCustomFrameHeight(38);
    }
    this->addShortcuts();
    this->addLayout();

#ifdef Q_OS_MACOS
    this->addMenuBar();
#endif

    this->signalHolder_.managedConnect(
        getApp()->getAccounts()->twitch.currentUserChanged, [this] {
            this->onAccountSelected();
        });
    this->onAccountSelected();

    if (this->teamsStyle_)
    {
        const auto palette = currentTeamsStylePalette();
        this->setCustomFrameColors(palette.frame, palette.primaryText);
        this->updateTeamsIdentity();
        getSettings()->teamsStyleAliases.connect(
            [this](const auto &, auto) {
                this->updateTeamsTabTitles();
            },
            this->signalHolder_);
        getSettings()->teamsStyleShowBadges.connect(
            [](const bool &, auto) {
                getApp()->getWindows()->forceLayoutChannelViews();
                getApp()->getWindows()->invalidateChannelViewBuffers();
            },
            this->signalHolder_);
        this->signalHolder_.managedConnect(
            this->notebook_->pageCountChanged, [this](int) {
                this->syncTeamsFromNotebook();
            });
        this->signalHolder_.managedConnect(
            this->notebook_->selectedPageChanged, [this](QWidget *) {
                this->syncTeamsFromNotebook();
            });
        this->signalHolder_.managedConnect(
            this->notebook_->pagesReordered, [this] {
                if (this->teamsTabStrip_ != nullptr)
                {
                    this->teamsTabStrip_->sync();
                }
            });
    }

    if (this->teamsStyle_)
    {
        this->resize(int(773 * this->scale()), int(931 * this->scale()));
    }
    else if (type == WindowType::Main)
    {
        this->resize(int(600 * this->scale()), int(500 * this->scale()));
#ifdef Q_OS_LINUX
        if (this->theme->window.background.alpha() != 255)
        {
            this->setAttribute(Qt::WA_TranslucentBackground);
        }
#endif
    }
    else
    {
        auto lastPopup = getSettings()->lastPopupSize.getValue();
        if (lastPopup.isEmpty())
        {
            // The size in the setting was invalid, use the default value
            lastPopup = getSettings()->lastPopupSize.getDefaultValue();
        }
        this->resize(lastPopup.width(), lastPopup.height());
    }

    this->signalHolder_.managedConnect(getApp()->getHotkeys()->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
    if (type == WindowType::Main || type == WindowType::Popup)
    {
        getSettings()->tabDirection.connect(
            [this](int val) {
                this->notebook_->setTabLocation(NotebookTabLocation(val));
            },
            this->signalHolder_);
    }
}

WindowType Window::getType()
{
    return this->type_;
}

bool Window::isTeamsStyle() const
{
    return this->teamsStyle_;
}

void Window::bindTeamsSplit(Split *split)
{
    if (!this->teamsStyle_ || split == nullptr)
    {
        return;
    }

    auto *container = qobject_cast<SplitContainer *>(split->parentWidget());
    if (container != this->notebook_->getSelectedPage())
    {
        this->updateTeamsTabTitles();
        return;
    }

    this->teamsSplitSignalHolder_.clear();
    this->teamsSplit_ = split;
    this->teamsSplitSignalHolder_.managedConnect(split->channelChanged,
                                                  [this] {
                                                      this->updateTeamsTabTitles();
                                                  });
    this->updateTeamsTabTitles();

    QTimer::singleShot(0, this, [this, split] {
        if (this->teamsSplit_ == split && split->getChannel()->isEmpty() &&
            this->notebook_->getPageCount() == 1)
        {
            split->showChangeChannelPopup("Choose channel", true,
                                          [](bool) {});
        }
    });
}

void Window::openTeamsNewChat()
{
    if (!this->teamsStyle_)
    {
        return;
    }

    auto *page = this->notebook_->addPage(true);
    auto *split = page->appendNewSplit(false);
    this->updateTeamsTabTitles();
    QPointer<SplitContainer> guardedPage(page);
    split->showChangeChannelPopup("New chat", true,
                                  [this, guardedPage](bool accepted) {
                                      if (!accepted && guardedPage != nullptr)
                                      {
                                          this->notebook_->removePage(
                                              guardedPage);
                                      }
                                      this->syncTeamsFromNotebook();
                                  });
}

void Window::syncTeamsFromNotebook()
{
    if (!this->teamsStyle_)
    {
        return;
    }

    auto *page = this->notebook_->getSelectedPage();
    if (page != nullptr && !page->getSplits().empty())
    {
        this->bindTeamsSplit(page->getSplits().front());
    }
    else
    {
        this->teamsSplitSignalHolder_.clear();
        this->teamsSplit_ = nullptr;
        this->updateTeamsTabTitles();
    }
    if (this->teamsTabStrip_ != nullptr)
    {
        this->teamsTabStrip_->sync();
    }
}

void Window::updateTeamsTabTitles()
{
    if (!this->teamsStyle_)
    {
        return;
    }

    const auto aliases = getSettings()->teamsStyleAliases.getValue();
    for (int i = 0; i < this->notebook_->getPageCount(); ++i)
    {
        auto *page = dynamic_cast<SplitContainer *>(
            this->notebook_->getPageAt(i));
        if (page == nullptr || page->getTab() == nullptr)
        {
            continue;
        }

        QString identity = "Chat";
        if (!page->getSplits().empty())
        {
            const auto channel = page->getSplits().front()->getSelectedChannel();
            if (channel && !channel->isEmpty())
            {
                const auto provider = channel->isTwitchChannel()
                                          ? QStringLiteral("twitch")
                                      : channel->isKickChannel()
                                          ? QStringLiteral("kick")
                                          : QStringLiteral("channel");
                identity = teamsResolvedIdentity(
                    aliases, teamsChannelKey(provider, channel->getName()),
                    channel->getDisplayName(), channel->getName());
            }
        }
        page->getTab()->resetCustomTitle();
        page->getTab()->setDefaultTitle(identity);
    }

    this->updateTeamsIdentity();
    if (this->teamsTabStrip_ != nullptr)
    {
        this->teamsTabStrip_->sync();
    }
}

SplitNotebook &Window::getNotebook()
{
    return *this->notebook_;
}

void Window::setPopupID(size_t id)
{
    this->popupID_ = id;
}

std::optional<size_t> Window::popupID() const
{
    return this->popupID_;
}

bool Window::event(QEvent *event)
{
    switch (event->type())
    {
        case QEvent::WindowActivate: {
            getApp()->getWindows()->selectedWindow_ = this;
            break;
        }

        case QEvent::WindowDeactivate: {
            auto *page = this->notebook_->getSelectedPage();

            if (page != nullptr)
            {
                std::vector<Split *> splits = page->getSplits();
                for (Split *split : splits)
                {
                    split->unpause();
                    split->updateLastReadMessage();
                }

                page->hideResizeHandles();
            }
        }
        break;

        default:;
    }

    return BaseWindow::event(event);
}

void Window::closeEvent(QCloseEvent *)
{
    if (isAppAboutToQuit())
    {
        qCWarning(chatterinoWidget)
            << "Window closeEvent ran when Application is already dead";
        return;
    }

    auto *app = getApp();

    if (this->type_ == WindowType::Main)
    {
        app->getWindows()->save();
        app->getWindows()->closeAll();
    }
    else
    {
        QRect rect = this->getBounds();
        QSize newSize(rect.width(), rect.height());
        getSettings()->lastPopupSize.setValue(newSize);
    }
    // Ensure selectedWindow_ is never an invalid pointer.
    // WindowManager will return the main window if no window is pointed to by
    // `selectedWindow_`.
    app->getWindows()->selectedWindow_ = nullptr;

    this->closed.invoke();

    if (this->type_ == WindowType::Main)
    {
        QApplication::exit();
    }
}

void Window::addLayout()
{
    auto *layout = new QVBoxLayout();

    if (this->teamsStyle_)
    {
        this->teamsHeader_ = new TeamsChatHeader(this);
        this->teamsHeader_->applyPalette(currentTeamsStylePalette());
        QObject::connect(this->teamsHeader_->moreButton(),
                         &Button::leftClicked, this, [this] {
                             this->showTeamsMenu(
                                 this->teamsHeader_->moreButton());
                         });
        layout->addWidget(this->teamsHeader_);
        this->teamsTabStrip_ = new TeamsTabStrip(this->notebook_, this);
        this->teamsTabStrip_->applyPalette(currentTeamsStylePalette());
        this->teamsTabStrip_->hide();
        layout->addWidget(this->teamsTabStrip_);
        this->notebook_->setTabsVisible(false);
        this->notebook_->setAllowUserTabManagement(false);
        this->notebook_->setShowAddButton(false);
    }

    layout->addWidget(this->notebook_);
    this->getLayoutContainer()->setLayout(layout);

    // set margin
    layout->setContentsMargins(0, 0, 0, 0);

    if (!this->teamsStyle_)
    {
        this->notebook_->setAllowUserTabManagement(true);
        this->notebook_->setShowAddButton(true);
    }
}

void Window::addCustomTitlebarButtons()
{
    if (!this->hasCustomWindowFrame())
    {
        return;
    }
    if (this->teamsStyle_)
    {
        auto *more = this->addTitleBarButton<TeamsMoreButton>([] {});
        this->teamsTitlebarMore_ = more;
        const auto palette = currentTeamsStylePalette();
        more->setProperty("frameForeground", palette.icon);
        more->setMouseEffectColor(palette.hover);
        QObject::connect(more, &Button::leftClicked, this, [this, more] {
            this->showTeamsMenu(more);
        });
        return;
    }
    if (this->type_ != WindowType::Main)
    {
        return;
    }

    // settings
    this->addTitleBarButton<TitleBarButton>(
        [this] {
            getApp()->getWindows()->showSettingsDialog(this);
        },
        TitleBarButtonStyle::Settings);

    // updates
    auto *update = this->addTitleBarButton<PixmapButton>([] {});

    initUpdateButton(*update, [] {}, this->signalHolder_);

    // account
    this->userLabel_ = this->addTitleBarLabel([this] {
        getApp()->getWindows()->showAccountSelectPopup(
            this->userLabel_->mapToGlobal(
                this->userLabel_->rect().bottomLeft()));
    });
    this->userLabel_->setMinimumWidth(20 * this->scale());

    // streamer mode
    this->streamerModeTitlebarIcon_ =
        this->addTitleBarButton<PixmapButton>([this] {
            getApp()->getWindows()->showSettingsDialog(
                this, SettingsDialogPreference::StreamerMode);
        });
    QObject::connect(getApp()->getStreamerMode(), &IStreamerMode::changed, this,
                     &Window::updateStreamerModeIcon);

    // Update initial state
    this->updateStreamerModeIcon();
}

void Window::updateStreamerModeIcon()
{
    // A duplicate of this code is in SplitNotebook class (in Notebook.{c,h}pp)
    // That one is the one near splits (on linux and mac or non-main windows on Windows)
    // This copy handles the TitleBar icon in Window (main window on Windows)
    if (this->streamerModeTitlebarIcon_ == nullptr)
    {
        return;
    }
#ifdef Q_OS_WIN
    assert(this->getType() == WindowType::Main);
    if (getTheme()->isLightTheme())
    {
        this->streamerModeTitlebarIcon_->setPixmap(
            getResources().buttons.streamerModeEnabledLight);
    }
    else
    {
        this->streamerModeTitlebarIcon_->setPixmap(
            getResources().buttons.streamerModeEnabledDark);
    }
    this->streamerModeTitlebarIcon_->setVisible(
        getApp()->getStreamerMode()->isEnabled());
#else
    // clang-format off
    assert(false && "Streamer mode TitleBar icon should not exist on non-Windows OSes");
    // clang-format on
#endif
}

void Window::themeChangedEvent()
{
    this->updateStreamerModeIcon();
    BaseWindow::themeChangedEvent();
    if (this->teamsStyle_)
    {
        const auto palette = currentTeamsStylePalette();
        this->setCustomFrameColors(palette.frame, palette.primaryText);
        if (this->teamsHeader_ != nullptr)
        {
            this->teamsHeader_->applyPalette(palette);
        }
        if (this->teamsTabStrip_ != nullptr)
        {
            this->teamsTabStrip_->applyPalette(palette);
        }
        if (this->teamsTitlebarMore_ != nullptr)
        {
            this->teamsTitlebarMore_->setProperty("frameForeground",
                                                   palette.icon);
            static_cast<TeamsMoreButton *>(this->teamsTitlebarMore_)
                ->setMouseEffectColor(palette.hover);
            this->teamsTitlebarMore_->update();
        }
        getApp()->getWindows()->invalidateChannelViewBuffers();
        getApp()->getWindows()->forceLayoutChannelViews();
    }
}

void Window::showTeamsMenu(QWidget *anchor)
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const auto palette = currentTeamsStylePalette();
    menu->setStyleSheet(
        QString("QMenu { background: %1; color: %2; border: 1px solid %3; "
                "padding: 4px; } QMenu::item { padding: 7px 28px 7px 10px; "
                "} QMenu::item:selected { background: %4; color: %2; } "
                "QMenu::separator { height: 1px; background: %5; margin: 4px "
                "8px; }")
            .arg(palette.raisedSurface.name(), palette.primaryText.name(),
                 palette.controlBorder.name(), palette.hover.name(),
                 palette.divider.name()));
    menu->addAction("New chat\u2026", this, [this] {
        this->openTeamsNewChat();
    });
    auto *changeChannel = menu->addAction("Change channel\u2026");
    changeChannel->setEnabled(this->teamsSplit_ != nullptr);
    QObject::connect(changeChannel, &QAction::triggered, this, [this] {
        this->teamsSplit_->showChangeChannelPopup("Change channel", false,
                                                  [](bool) {});
    });

    auto *renameChat = menu->addAction("Rename chat\u2026");
    renameChat->setEnabled(this->teamsSplit_ != nullptr &&
                           !this->teamsSplit_->getChannel()->isEmpty());
    QObject::connect(renameChat, &QAction::triggered, this, [this] {
        const auto channel = this->teamsSplit_->getSelectedChannel();
        if (!channel || channel->isEmpty())
        {
            return;
        }

        const auto provider = channel->isTwitchChannel()
                                  ? QStringLiteral("twitch")
                              : channel->isKickChannel()
                                  ? QStringLiteral("kick")
                                  : QStringLiteral("channel");
        const auto key = teamsChannelKey(provider, channel->getName());
        const auto aliases = getSettings()->teamsStyleAliases.getValue();
        const auto current = aliases.contains(key) ? aliases.at(key) : QString{};
        bool accepted = false;
        const auto value = QInputDialog::getText(
            this, "Rename chat", "Chat name:", QLineEdit::Normal, current,
            &accepted);
        if (!accepted)
        {
            return;
        }
        auto updated = teamsAliasesWithRename(aliases, key, value);
        getSettings()->teamsStyleAliases.setValue(std::move(updated));
    });

    menu->addSeparator();
    auto *showBadges = menu->addAction("Show badges");
    showBadges->setCheckable(true);
    showBadges->setChecked(getSettings()->teamsStyleShowBadges);
    QObject::connect(showBadges, &QAction::toggled, this, [](bool checked) {
        getSettings()->teamsStyleShowBadges.setValue(checked);
    });

    menu->addSeparator();
    menu->addAction("Settings", this, [this] {
        getApp()->getWindows()->showSettingsDialog(this);
    });
    menu->addSeparator();
    menu->addAction("Close", this, [this] {
        this->close();
    });
    menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void Window::updateTeamsIdentity()
{
    QString identity = "Chat";
    QString initials = "C";
    std::optional<int> participantCount;
    this->teamsChannelSignalHolder_.clear();

    if (this->teamsSplit_ != nullptr)
    {
        const auto channel = this->teamsSplit_->getSelectedChannel();
        if (channel && !channel->isEmpty())
        {
            const auto provider = channel->isTwitchChannel()
                                      ? QStringLiteral("twitch")
                                  : channel->isKickChannel()
                                      ? QStringLiteral("kick")
                                      : QStringLiteral("channel");
            const auto key = teamsChannelKey(provider, channel->getName());
            const auto aliases = getSettings()->teamsStyleAliases.getValue();
            identity = teamsResolvedIdentity(aliases, key,
                                             channel->getDisplayName(),
                                             channel->getName());

            this->teamsChannelSignalHolder_.managedConnect(
                channel->displayNameChanged, [this] {
                    this->updateTeamsIdentity();
                });

            if (auto twitch =
                    std::dynamic_pointer_cast<TwitchChannel>(channel))
            {
                participantCount = twitch->knownChatterCount();
                this->teamsChannelSignalHolder_.managedConnect(
                    twitch->chatterCountChanged, [this] {
                        this->updateTeamsIdentity();
                    });
            }
        }
    }

    initials = teamsIdentityInitials(identity);

    this->setWindowTitle(identity);
    if (this->teamsHeader_ != nullptr)
    {
        this->teamsHeader_->setIdentity(identity, initials);
        this->teamsHeader_->setParticipantCount(participantCount);
    }
}

void Window::addDebugStuff(HotkeyController::HotkeyMap &actions)
{
#ifndef NDEBUG
    actions.emplace("addMiscMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleMiscMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addCheerMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleCheerMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addLinkMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleLinkMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addRewardMessage", [=](std::vector<QString>) -> QString {
        rapidjson::Document doc;
        static bool alt = true;
        if (alt)
        {
            auto oMessage =
                parsePubSubBaseMessage(getSampleChannelRewardMessage());
            auto oInnerMessage =
                oMessage->toInner<PubSubMessageMessage>()
                    ->toInner<PubSubCommunityPointsChannelV1Message>();

            getApp()->getTwitch()->addFakeMessage(
                getSampleChannelRewardIRCMessage());
            getApp()->getTwitchPubSub()->pointReward.redeemed.invoke(
                oInnerMessage->data.value("redemption").toObject());
            alt = !alt;
        }
        else
        {
            auto oMessage =
                parsePubSubBaseMessage(getSampleChannelRewardMessage2());
            auto oInnerMessage =
                oMessage->toInner<PubSubMessageMessage>()
                    ->toInner<PubSubCommunityPointsChannelV1Message>();
            getApp()->getTwitchPubSub()->pointReward.redeemed.invoke(
                oInnerMessage->data.value("redemption").toObject());
            alt = !alt;
        }
        return "";
    });

    actions.emplace("addEmoteMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleEmoteTestMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addSubMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleSubMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });
#endif
}

void Window::addShortcuts()
{
    HotkeyController::HotkeyMap actions{
        {"openSettings",  // Open settings
         [this](std::vector<QString>) -> QString {
             SettingsDialog::showDialog(this);
             return "";
         }},
        {"openAccountSelector",  // Open account selector
         [](const std::vector<QString> &) -> QString {
             getApp()->getWindows()->showAccountSelectPopup({0, 0});
             return "";
         }},
        {"newSplit",  // Create a new split
         [this](std::vector<QString>) -> QString {
             if (this->teamsStyle_)
             {
                 this->openTeamsNewChat();
             }
             else
             {
                 this->notebook_->getOrAddSelectedPage()->appendNewSplit(true);
             }
             return "";
         }},
        {"openTab",  // CTRL + 1-8 to open corresponding tab.
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "openTab shortcut called without arguments. "
                        "Takes only "
                        "one argument: tab specifier";
                 return "openTab shortcut called without arguments. "
                        "Takes only "
                        "one argument: tab specifier";
             }
             auto target = arguments.at(0);
             if (target == "last")
             {
                 this->notebook_->selectLastTab();
             }
             else if (target == "next")
             {
                 this->notebook_->selectNextTab();
             }
             else if (target == "previous")
             {
                 this->notebook_->selectPreviousTab();
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (ok)
                 {
                     this->notebook_->selectVisibleIndex(result);
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for openTab shortcut";
                     return QString("Invalid argument for openTab "
                                    "shortcut: \"%1\". Use \"last\", "
                                    "\"next\", \"previous\" or an integer.")
                         .arg(target);
                 }
             }
             return "";
         }},
        {"selectTabHistoryBack",
         [this](const std::vector<QString> &) -> QString {
             this->notebook_->selectHistoryBack(true);
             return "";
         }},
        {"selectTabHistoryForward",
         [this](const std::vector<QString> &) -> QString {
             this->notebook_->selectHistoryForward(true);
             return "";
         }},
        {"popup",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 return "popup action called without arguments. Takes only "
                        "one: \"split\" or \"window\".";
             }
             if (arguments.at(0) == "split")
             {
                 if (auto *page = dynamic_cast<SplitContainer *>(
                         this->notebook_->getSelectedPage()))
                 {
                     if (auto *split = page->getSelectedSplit())
                     {
                         split->popup();
                     }
                 }
             }
             else if (arguments.at(0) == "window")
             {
                 if (auto *page = dynamic_cast<SplitContainer *>(
                         this->notebook_->getSelectedPage()))
                 {
                     page->popup();
                 }
             }
             else
             {
                 return R"(Invalid popup target. Use "split" or "window".)";
             }
             return "";
         }},
        {"zoom",
         [](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "zoom shortcut called without arguments. Takes "
                        "only "
                        "one argument: \"in\", \"out\", or \"reset\"";
                 return "zoom shortcut called without arguments. Takes "
                        "only "
                        "one argument: \"in\", \"out\", or \"reset\"";
             }
             auto change = 0.0f;
             auto direction = arguments.at(0);
             if (direction == "reset")
             {
                 getSettings()->uiScale.setValue(1);
                 return "";
             }

             if (direction == "in")
             {
                 change = 0.1f;
             }
             else if (direction == "out")
             {
                 change = -0.1f;
             }
             else
             {
                 qCWarning(chatterinoHotkeys)
                     << "Invalid zoom direction, use \"in\", \"out\", or "
                        "\"reset\"";
                 return "Invalid zoom direction, use \"in\", \"out\", or "
                        "\"reset\"";
             }
             getSettings()->setClampedUiScale(
                 getSettings()->getClampedUiScale() + change);
             return "";
         }},
        {"newTab",
         [this](std::vector<QString>) -> QString {
             if (this->teamsStyle_)
             {
                 this->openTeamsNewChat();
             }
             else
             {
                 this->notebook_->addPage(true);
             }
             return "";
         }},
        {"removeTab",
         [this](std::vector<QString>) -> QString {
             this->notebook_->removeCurrentPage();
             return "";
         }},
        {"reopenSplit",
         [this](std::vector<QString>) -> QString {
             if (ClosedSplits::empty())
             {
                 return "";
             }
             ClosedSplits::SplitInfo si = ClosedSplits::pop();
             SplitContainer *splitContainer{nullptr};
             if (this->teamsStyle_)
             {
                 splitContainer = this->notebook_->addPage(true);
             }
             if (si.tab)
             {
                 if (!this->teamsStyle_)
                 {
                     splitContainer =
                         dynamic_cast<SplitContainer *>(si.tab->page);
                 }
             }
             if (!splitContainer)
             {
                 splitContainer = this->notebook_->getOrAddSelectedPage();
             }
             Split *split = new Split(splitContainer);
             split->setChannel(
                 getApp()->getTwitch()->getOrAddChannel(si.channelName));
             split->setFilters(si.filters);
             splitContainer->insertSplit(split);
             splitContainer->setSelected(split);
             this->notebook_->select(splitContainer);
             return "";
         }},
        {"toggleLocalR9K",
         [](std::vector<QString>) -> QString {
             getSettings()->hideSimilar.setValue(!getSettings()->hideSimilar);
             getApp()->getWindows()->forceLayoutChannelViews();
             return "";
         }},
        {"openQuickSwitcher",
         [this](std::vector<QString>) -> QString {
             auto *quickSwitcher = new QuickSwitcherPopup(this);
             quickSwitcher->show();
             return "";
         }},
        {"quit",
         [](std::vector<QString>) -> QString {
             QApplication::exit();
             return "";
         }},
        {"moveTab",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "moveTab shortcut called without arguments. "
                        "Takes only one argument: new index (number, "
                        "\"next\" "
                        "or \"previous\")";
                 return "moveTab shortcut called without arguments. "
                        "Takes only one argument: new index (number, "
                        "\"next\" "
                        "or \"previous\")";
             }
             int newIndex = -1;
             bool indexIsGenerated =
                 false;  // indicates if `newIndex` was generated using target="next" or target="previous"

             auto target = arguments.at(0);
             qCDebug(chatterinoHotkeys) << target;
             if (target == "next")
             {
                 newIndex = this->notebook_->getSelectedIndex() + 1;
                 indexIsGenerated = true;
             }
             else if (target == "previous")
             {
                 newIndex = this->notebook_->getSelectedIndex() - 1;
                 indexIsGenerated = true;
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (!ok)
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for moveTab shortcut";
                     return QString("Invalid argument for moveTab shortcut: "
                                    "%1. Use \"next\" or \"previous\" or an "
                                    "integer.")
                         .arg(target);
                 }
                 newIndex = result;
             }
             if (newIndex >= this->notebook_->getPageCount() || 0 > newIndex)
             {
                 if (indexIsGenerated)
                 {
                     return "";  // don't error out on generated indexes, ie move tab right
                 }
                 qCWarning(chatterinoHotkeys)
                     << "Invalid index for moveTab shortcut:" << newIndex;
                 return QString("Invalid index for moveTab shortcut: %1.")
                     .arg(newIndex);
             }
             this->notebook_->rearrangePage(this->notebook_->getSelectedPage(),
                                            newIndex);
             return "";
         }},
        {"setStreamerMode",
         [](std::vector<QString> arguments) -> QString {
             auto mode = 2;
             if (arguments.size() != 0)
             {
                 auto arg = arguments.at(0);
                 if (arg == "off")
                 {
                     mode = 0;
                 }
                 else if (arg == "on")
                 {
                     mode = 1;
                 }
                 else if (arg == "toggle")
                 {
                     mode = 2;
                 }
                 else if (arg == "auto")
                 {
                     mode = 3;
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for setStreamerMode hotkey: "
                         << arg;
                     return QString("Invalid argument for setStreamerMode "
                                    "hotkey: %1. Use \"on\", \"off\", "
                                    "\"toggle\" or \"auto\".")
                         .arg(arg);
                 }
             }

             if (mode == 0)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::Disabled);
             }
             else if (mode == 1)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::Enabled);
             }
             else if (mode == 2)
             {
                 if (getApp()->getStreamerMode()->isEnabled())
                 {
                     getSettings()->enableStreamerMode.setValue(
                         StreamerModeSetting::Disabled);
                 }
                 else
                 {
                     getSettings()->enableStreamerMode.setValue(
                         StreamerModeSetting::Enabled);
                 }
             }
             else if (mode == 3)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::DetectStreamingSoftware);
             }
             return "";
         }},
        {"setTabVisibility",
         [this](std::vector<QString> arguments) -> QString {
             QString arg = arguments.empty() ? "toggle" : arguments.front();

             if (arg == "off")
             {
                 this->notebook_->hideAllTabsAction->trigger();
             }
             else if (arg == "on")
             {
                 this->notebook_->showAllTabsAction->trigger();
             }
             else if (arg == "toggle")
             {
                 this->notebook_->toggleTabVisibility();
             }
             else if (arg == "liveOnly")
             {
                 this->notebook_->onlyShowLiveTabsAction->trigger();
             }
             else if (arg == "toggleLiveOnly")
             {
                 // NOOP: Removed 2024-08-04 https://github.com/Chatterino/chatterino2/pull/5530
                 return "toggleLiveOnly is no longer a valid argument for "
                        "setTabVisibility";
             }
             else
             {
                 qCWarning(chatterinoHotkeys)
                     << "Invalid argument for setTabVisibility hotkey: " << arg;
                 return QString("Invalid argument for setTabVisibility hotkey: "
                                "%1. Use \"on\", \"off\", \"toggle\", or "
                                "\"liveOnly\".")
                     .arg(arg);
             }

             return "";
         }},
    };

    this->addDebugStuff(actions);

    this->shortcuts_ = getApp()->getHotkeys()->shortcutsForCategory(
        HotkeyCategory::Window, actions, this);
}

void Window::addMenuBar()
{
    QMenuBar *mainMenu = new QMenuBar();
    mainMenu->setNativeMenuBar(true);

    // First menu.
    QMenu *menu = mainMenu->addMenu(QString());

    // About button that shows the About tab in the Settings Dialog.
    QAction *about = menu->addAction(QString());
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, [this] {
        SettingsDialog::showDialog(this, SettingsDialogPreference::About);
    });

    QAction *prefs = menu->addAction(QString());
    prefs->setMenuRole(QAction::PreferencesRole);
    connect(prefs, &QAction::triggered, this, [this] {
        SettingsDialog::showDialog(this);
    });

    // Window menu.
    QMenu *windowMenu = mainMenu->addMenu(QString("Window"));

    // Window->Minimize item
    QAction *minimizeWindow = windowMenu->addAction(QString("Minimize"));
    minimizeWindow->setShortcuts({QKeySequence("Meta+M")});
    connect(minimizeWindow, &QAction::triggered, this, [this] {
        this->setWindowState(Qt::WindowMinimized);
    });

    QAction *nextTab = windowMenu->addAction(QString("Select next tab"));
    nextTab->setShortcuts({QKeySequence("Meta+Tab")});
    connect(nextTab, &QAction::triggered, this, [this] {
        this->notebook_->selectNextTab();
    });

    QAction *prevTab = windowMenu->addAction(QString("Select previous tab"));
    prevTab->setShortcuts({QKeySequence("Meta+Shift+Tab")});
    connect(prevTab, &QAction::triggered, this, [this] {
        this->notebook_->selectPreviousTab();
    });

    // Help menu.
    QMenu *helpMenu = mainMenu->addMenu(QString("Help"));

    // Help->Chatterino Wiki item
    QAction *helpWiki = helpMenu->addAction(QString("Chatterino Wiki"));
    connect(helpWiki, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(LINK_CHATTERINO_WIKI.toString()));
    });

    // Help->Chatterino Github
    QAction *helpGithub = helpMenu->addAction(QString("Chatterino GitHub"));
    connect(helpGithub, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(LINK_CHATTERINO_SOURCE.toString()));
    });

    // Help->Chatterino Discord
    QAction *helpDiscord = helpMenu->addAction(QString("Chatterino Discord"));
    connect(helpDiscord, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(LINK_CHATTERINO_DISCORD.toString()));
    });
}

void Window::onAccountSelected()
{
    auto user = getApp()->getAccounts()->twitch.getCurrent();

    // update title (also append username on Linux and MacOS)
    QString windowTitle = Version::instance().fullVersion();

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if (user->isAnon())
    {
        windowTitle += " - not logged in";
    }
    else
    {
        windowTitle += " - " + user->getUserName();
    }
#endif

    if (getApp()->getArgs().safeMode)
    {
        windowTitle += " (safe mode)";
    }

    this->setWindowTitle(windowTitle);

    if (this->teamsStyle_)
    {
        this->updateTeamsIdentity();
    }

    // update user
    if (this->userLabel_)
    {
        if (user->isAnon())
        {
            this->userLabel_->setText("anonymous");
        }
        else
        {
            this->userLabel_->setText(user->getUserName());
        }
    }
}

}  // namespace chatterino
