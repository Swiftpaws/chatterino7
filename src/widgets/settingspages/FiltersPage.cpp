// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/FiltersPage.hpp"

#include "Application.hpp"
#include "controllers/filters/FilterModel.hpp"
#include "controllers/filters/FilterRecord.hpp"
#include "controllers/filters/OutgoingMessageFilter.hpp"
#include "controllers/filters/OutgoingMessageFilterModel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/dialogs/ChannelFilterEditorDialog.hpp"
#include "widgets/helper/EditableModelView.hpp"

#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableView>
#include <QTabWidget>

namespace chatterino {

constexpr QStringView FILTERS_DOCUMENTATION =
    u"https://wiki.chatterino.com/Filters";

FiltersPage::FiltersPage()
{
    LayoutCreator<FiltersPage> layoutCreator(this);
    auto layout = layoutCreator.setLayoutType<QVBoxLayout>();
    auto tabs = layout.emplace<QTabWidget>();

    auto channel = tabs.appendTab(new QVBoxLayout, "Channel");
    channel.emplace<QLabel>(
        "Selectively display messages in Splits using channel filters. Set "
        "filters under a Split menu.");
    EditableModelView *view =
        channel
            .emplace<EditableModelView>(
                (new FilterModel(nullptr))
                    ->initialized(&getSettings()->filterRecords))
            .getElement();
    this->channelView_ = view;

    view->setTitles({"Name", "Filter", "Valid"});
    view->getTableView()->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Interactive);
    view->getTableView()->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);

    QTimer::singleShot(1, [view] {
        view->getTableView()->resizeColumnsToContents();
        view->getTableView()->setColumnWidth(0, 150);
        view->getTableView()->setColumnWidth(2, 125);
    });

    // We can safely ignore this signal connection since we own the view
    std::ignore = view->addButtonPressed.connect([this] {
        ChannelFilterEditorDialog d(this->window());
        if (d.exec() == QDialog::Accepted)
        {
            getSettings()->filterRecords.append(
                std::make_shared<FilterRecord>(d.getTitle(), d.getFilter()));
        }
    });

    auto *quickAddButton = new QPushButton("Quick Add");
    QObject::connect(quickAddButton, &QPushButton::pressed, [] {
        getSettings()->filterRecords.append(std::make_shared<FilterRecord>(
            "My filter", "message.content contains \"hello\""));
    });
    view->addCustomButton(quickAddButton);

    QObject::connect(view->getTableView(), &QTableView::clicked,
                     [this, view](const QModelIndex &clicked) {
                         this->channelTableCellClicked(clicked, view);
                     });

    auto *filterHelpLabel =
        new QLabel(QStringView(u"<a href='%1'><span "
                               "style='color:#99f'>filter info</span></a>")
                       .arg(FILTERS_DOCUMENTATION));
    filterHelpLabel->setOpenExternalLinks(true);
    view->addCustomButton(filterHelpLabel);

    channel.append(
        this->createCheckBox("Do not filter my own messages",
                             getSettings()->excludeUserMessagesFromFilter));

    auto outgoing = tabs.appendTab(new QVBoxLayout, "Outgoing");
    outgoing.emplace<QLabel>(
        "Prevent messages matching a regular expression from being sent. "
        "Invalid enabled expressions also prevent sending.");
    auto *outgoingView =
        outgoing
            .emplace<EditableModelView>(
                (new OutgoingMessageFilterModel(nullptr))
                    ->initialized(&getSettings()->outgoingMessageFilters))
            .getElement();
    this->outgoingView_ = outgoingView;
    outgoingView->setTitles({"Enabled", "Regex", "Valid"});
    outgoingView->getTableView()
        ->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Interactive);
    outgoingView->getTableView()
        ->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::Stretch);
    QTimer::singleShot(1, [outgoingView] {
        outgoingView->getTableView()->resizeColumnsToContents();
        outgoingView->getTableView()->setColumnWidth(2, 125);
    });
    std::ignore = outgoingView->addButtonPressed.connect([] {
        getSettings()->outgoingMessageFilters.append({
            .enabled = true,
            .pattern = "my regex",
        });
    });
    outgoingView->addRegexHelpLink();
    QObject::connect(outgoingView->getTableView(), &QTableView::clicked,
                     [this, outgoingView](const QModelIndex &clicked) {
                         this->outgoingTableCellClicked(clicked, outgoingView);
                     });

    auto tester = outgoing.emplace<QHBoxLayout>().withoutMargin();
    tester.emplace<QLabel>("Test message:");
    auto *testInput = tester.emplace<QLineEdit>().getElement();
    auto *testResult = tester.emplace<QLabel>("OK").getElement();
    auto updateTestResult = [testInput, testResult] {
        const auto result = filterOutgoingMessage(
            testInput->text(), getSettings()->outgoingMessageFilters.raw());
        if (!result)
        {
            testResult->setText("OK");
        }
        else if (result->reason == OutgoingMessageFilterBlockReason::Invalid)
        {
            testResult->setText(
                QString("Filtered: rule %1 is invalid")
                    .arg(result->ruleNumber));
        }
        else
        {
            testResult->setText(QString("Filtered: matches rule %1")
                                    .arg(result->ruleNumber));
        }
    };
    QObject::connect(testInput, &QLineEdit::textChanged, this,
                     [updateTestResult](const QString &) {
                         updateTestResult();
                     });
    auto *outgoingModel = outgoingView->getModel();
    QObject::connect(outgoingModel, &QAbstractItemModel::dataChanged, this,
                     [updateTestResult] { updateTestResult(); });
    QObject::connect(outgoingModel, &QAbstractItemModel::rowsInserted, this,
                     [updateTestResult] { updateTestResult(); });
    QObject::connect(outgoingModel, &QAbstractItemModel::rowsRemoved, this,
                     [updateTestResult] { updateTestResult(); });
    QObject::connect(outgoingModel, &QAbstractItemModel::rowsMoved, this,
                     [updateTestResult] { updateTestResult(); });
    updateTestResult();
}

void FiltersPage::onShow()
{
    return;
}

void FiltersPage::channelTableCellClicked(const QModelIndex &clicked,
                                          EditableModelView *view)
{
    // valid column
    if (clicked.column() == 2)
    {
        QMessageBox popup(this->window());

        auto filterText =
            view->getModel()->data(clicked.siblingAtColumn(1)).toString();
        auto filterResult = filters::Filter::fromString(filterText);

        if (std::holds_alternative<filters::Filter>(filterResult))
        {
            auto f = std::move(std::get<filters::Filter>(filterResult));
            if (f.returnType() == filters::Type::Bool)
            {
                popup.setIcon(QMessageBox::Icon::Information);
                popup.setWindowTitle("Valid filter");
                popup.setText("Filter is valid");
                popup.setInformativeText(
                    QString("Parsed as:\n%1").arg(f.filterString()));
            }
            else
            {
                popup.setIcon(QMessageBox::Icon::Warning);
                popup.setWindowTitle("Invalid filter");
                popup.setText(QString("Unexpected filter return type"));
                popup.setInformativeText(
                    QString("Expected %1 but got %2")
                        .arg(filters::typeToString(filters::Type::Bool))
                        .arg(filters::typeToString(f.returnType())));
            }
        }
        else
        {
            auto err = std::move(std::get<filters::FilterError>(filterResult));
            popup.setIcon(QMessageBox::Icon::Warning);
            popup.setWindowTitle("Invalid filter");
            popup.setText(QString("Parsing errors occurred:"));
            popup.setInformativeText(err.message);
        }

        popup.exec();
    }
}

void FiltersPage::outgoingTableCellClicked(const QModelIndex &clicked,
                                           EditableModelView *view)
{
    if (clicked.column() != 2)
    {
        return;
    }

    const auto pattern =
        view->getModel()->data(clicked.siblingAtColumn(1)).toString();
    const QRegularExpression regex(
        pattern,
        QRegularExpression::PatternOption::UseUnicodePropertiesOption);
    QMessageBox popup(this->window());
    if (!pattern.isEmpty() && regex.isValid())
    {
        popup.setIcon(QMessageBox::Icon::Information);
        popup.setWindowTitle("Valid regular expression");
        popup.setText("Regular expression is valid");
    }
    else
    {
        popup.setIcon(QMessageBox::Icon::Warning);
        popup.setWindowTitle("Invalid regular expression");
        popup.setText(pattern.isEmpty() ? "Regular expression is empty"
                                        : regex.errorString());
        if (!pattern.isEmpty())
        {
            popup.setInformativeText(
                QString("Error at offset %1").arg(regex.patternErrorOffset()));
        }
    }
    popup.exec();
}

bool FiltersPage::filterElements(const QString &query)
{
    std::array channelFields{0, 1};
    std::array outgoingFields{1};

    const bool channelMatches =
        this->channelView_->filterSearchResults(query, channelFields);
    const bool outgoingMatches =
        this->outgoingView_->filterSearchResults(query, outgoingFields);
    return channelMatches || outgoingMatches;
}

}  // namespace chatterino
