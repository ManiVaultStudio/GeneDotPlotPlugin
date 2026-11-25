#include "MarkerSelectionAction.h"

#include "../DotPlotListState.h"

#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

using namespace mv::gui;

MarkerSelectionAction::MarkerSelectionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title, true),
    _geneSearchStringAction(this, "Gene search"),
    _pasteMarkerGenesTriggerAction(this, "Paste genes..."),
    _showAllGroupsTriggerAction(this, "Show all"),
    _hideAllGroupsTriggerAction(this, "Hide all")
{
    setConnectionPermissionsToForceNone();
    _geneSearchStringAction.setPlaceHolderString("Find genes / features...");
    _geneSearchStringAction.setClearable(true);
    _geneSearchStringAction.setToolTip("Filter the feature list without changing the selected genes");
    _pasteMarkerGenesTriggerAction.setToolTip("Select an ordered list of marker genes by name");
    _showAllGroupsTriggerAction.setToolTip("Include every group in the dot plot");
    _hideAllGroupsTriggerAction.setToolTip("Exclude every group from the dot plot");

    addAction(&_geneSearchStringAction, StringAction::LineEdit);
    addAction(&_pasteMarkerGenesTriggerAction, TriggerAction::Text);
    addAction(&_showAllGroupsTriggerAction, TriggerAction::Text);
    addAction(&_hideAllGroupsTriggerAction, TriggerAction::Text);

    connect(&_geneSearchStringAction, &StringAction::stringChanged, this, &MarkerSelectionAction::filterGenes);
    connect(&_pasteMarkerGenesTriggerAction, &TriggerAction::triggered, this, &MarkerSelectionAction::selectMarkerGenes);
    connect(&_showAllGroupsTriggerAction, &TriggerAction::triggered, this, [this] { setAllGroupsVisible(true); });
    connect(&_hideAllGroupsTriggerAction, &TriggerAction::triggered, this, [this] { setAllGroupsVisible(false); });
}

QWidget* MarkerSelectionAction::getWidget(QWidget* parent, const std::int32_t& widgetFlags)
{
    Q_UNUSED(widgetFlags)

    // This plugin has one marker panel. Its lists are selection views; the
    // search, paste and visibility controls are ManiVault actions.
    auto* markerSelectionWidget = new QWidget(parent);
    auto* markerSelectionLayout = new QVBoxLayout(markerSelectionWidget);
    markerSelectionLayout->setContentsMargins(0, 0, 0, 0);
    markerSelectionLayout->addWidget(_geneSearchStringAction.createWidget(markerSelectionWidget, StringAction::LineEdit));

    _geneSelectionListWidget = new QListWidget(markerSelectionWidget);
    _geneSelectionListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _geneSelectionListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    _geneSelectionListWidget->setDefaultDropAction(Qt::MoveAction);
    _geneSelectionListWidget->setToolTip("Ctrl/Shift selects markers. Drag rows to change gene order. Searching does not clear selection.");
    markerSelectionLayout->addWidget(_geneSelectionListWidget, 3);
    markerSelectionLayout->addWidget(_pasteMarkerGenesTriggerAction.createWidget(markerSelectionWidget, TriggerAction::Text));

    auto* selectionHintLabel = new QLabel("Ctrl/Shift selects genes.", markerSelectionWidget);
    selectionHintLabel->setWordWrap(true);
    markerSelectionLayout->addWidget(selectionHintLabel);

    markerSelectionLayout->addWidget(new QLabel("Groups: check to show; drag to reorder", markerSelectionWidget));

    _groupSelectionListWidget = new QListWidget(markerSelectionWidget);
    _groupSelectionListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    _groupSelectionListWidget->setDefaultDropAction(Qt::MoveAction);
    markerSelectionLayout->addWidget(_groupSelectionListWidget, 2);

    auto* groupVisibilityLayout = new QHBoxLayout;
    groupVisibilityLayout->addWidget(_showAllGroupsTriggerAction.createWidget(markerSelectionWidget, TriggerAction::Text));
    groupVisibilityLayout->addWidget(_hideAllGroupsTriggerAction.createWidget(markerSelectionWidget, TriggerAction::Text));
    markerSelectionLayout->addLayout(groupVisibilityLayout);



    connect(_geneSelectionListWidget, &QListWidget::itemSelectionChanged, this, &MarkerSelectionAction::selectionChanged);
    connect(_geneSelectionListWidget->model(), &QAbstractItemModel::rowsMoved, this, &MarkerSelectionAction::selectionChanged);
    connect(_groupSelectionListWidget, &QListWidget::itemChanged, this, &MarkerSelectionAction::selectionChanged);
    connect(_groupSelectionListWidget->model(), &QAbstractItemModel::rowsMoved, this, &MarkerSelectionAction::selectionChanged);

    return markerSelectionWidget;
}

void MarkerSelectionAction::filterGenes(const QString& searchString)
{
    if (!_geneSelectionListWidget)
    {
        return;
    }

    for (int geneRow = 0; geneRow < _geneSelectionListWidget->count(); ++geneRow)
    {
        auto* geneItem = _geneSelectionListWidget->item(geneRow);
        geneItem->setHidden(!geneItem->text().contains(searchString, Qt::CaseInsensitive));
    }
}

void MarkerSelectionAction::setAllGroupsVisible(bool visible)
{
    if (!_groupSelectionListWidget)
    {
        return;
    }

    {
        const QSignalBlocker signalBlocker(_groupSelectionListWidget);

        for (int groupRow = 0; groupRow < _groupSelectionListWidget->count(); ++groupRow)
        {
            _groupSelectionListWidget->item(groupRow)->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        }
    }

    emit selectionChanged();
}

void MarkerSelectionAction::selectMarkerGenes()
{
    if (!_geneSelectionListWidget)
    {
        return;
    }

    bool accepted         = false;
    const auto markerText = QInputDialog::getMultiLineText(_geneSelectionListWidget, "Marker genes", "Gene names separated by commas, spaces or newlines. Input order becomes plot order.", {}, &accepted);

    if (!accepted)
    {
        return;
    }

    const auto markerNames = markerText.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    QStringList missingMarkerNames;
    QStringList ambiguousMarkerNames;
    std::vector<QListWidgetItem*> selectedGeneItems;

    for (const auto& markerName : markerNames)
    {
        QListWidgetItem* matchingGeneItem = nullptr;
        int matchingGeneCount             = 0;

        for (int geneRow = 0; geneRow < _geneSelectionListWidget->count(); ++geneRow)
        {
            auto* geneItem = _geneSelectionListWidget->item(geneRow);

            if (geneItem->text() == markerName)
            {
                matchingGeneItem = geneItem;
                ++matchingGeneCount;
            }
        }

        if (matchingGeneCount == 0)
        {
            missingMarkerNames << markerName;
        }
        else if (matchingGeneCount > 1)
        {
            ambiguousMarkerNames << markerName;
        }
        else if (std::find(selectedGeneItems.begin(), selectedGeneItems.end(), matchingGeneItem) == selectedGeneItems.end())
        {
            selectedGeneItems.push_back(matchingGeneItem);
        }
    }

    if (!missingMarkerNames.isEmpty() || !ambiguousMarkerNames.isEmpty())
    {
        QMessageBox::warning(_geneSelectionListWidget, "Check marker names", "Selection was not changed.\nUnknown: " + missingMarkerNames.join(", ") + "\nDuplicate feature names (select in list): " + ambiguousMarkerNames.join(", "));
        return;
    }

    {
        const QSignalBlocker signalBlocker(_geneSelectionListWidget);
        _geneSelectionListWidget->clearSelection();

        for (int markerIndex = 0; markerIndex < int(selectedGeneItems.size()); ++markerIndex)
        {
            auto* geneItem = selectedGeneItems[markerIndex];
            _geneSelectionListWidget->insertItem(markerIndex, _geneSelectionListWidget->takeItem(_geneSelectionListWidget->row(geneItem)));
            geneItem->setSelected(true);
        }
    }

    _geneSearchStringAction.setString({});
    emit selectionChanged();
}

void MarkerSelectionAction::queueSelectionState(const QVariantList& geneSelectionState, const QVariantList& groupSelectionState, const QString& geneSearchString)
{
    _pendingGeneSelectionState  = geneSelectionState;
    _pendingGroupSelectionState = groupSelectionState;
    _pendingGeneSearchString    = geneSearchString;
    _hasPendingSelectionState   = true;
}

void MarkerSelectionAction::applyPendingSelectionState()
{
    if (!_hasPendingSelectionState || !_geneSelectionListWidget || !_groupSelectionListWidget)
    {
        return;
    }

    genedot::restoreList(_geneSelectionListWidget, _pendingGeneSelectionState, false);
    genedot::restoreList(_groupSelectionListWidget, _pendingGroupSelectionState, true);
    _geneSearchStringAction.setString(_pendingGeneSearchString);
    filterGenes(_pendingGeneSearchString);
    _hasPendingSelectionState = false;
}

void MarkerSelectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);
    _geneSearchStringAction.fromParentVariantMap(variantMap);
    queueSelectionState(variantMap.value("Genes").toList(), variantMap.value("Groups").toList(), _geneSearchStringAction.getString());
}

QVariantMap MarkerSelectionAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();
    _geneSearchStringAction.insertIntoVariantMap(variantMap);
    variantMap["Genes"]  = _hasPendingSelectionState ? _pendingGeneSelectionState : genedot::listState(_geneSelectionListWidget, false);
    variantMap["Groups"] = _hasPendingSelectionState ? _pendingGroupSelectionState : genedot::listState(_groupSelectionListWidget, true);
    return variantMap;
}
