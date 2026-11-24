#include "GeneDotPlotPlugin.h"

#include "DotPlotListState.h"
#include "DotPlotMembership.h"

#include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>
#include <DatasetsMimeData.h>
#include <actions/PluginTriggerAction.h>
#include <widgets/DropWidget.h>

#include <QListWidget>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

using namespace mv;
using namespace mv::gui;
using namespace mv::plugin;
using genedot::listState;
using genedot::restoreList;

namespace {
constexpr int maximumGeneCount             = 250;
constexpr int maximumGroupCount            = 2000;
constexpr int maximumDotCount              = 25000;
constexpr std::size_t aggregationChunkSize = 32768;
} // namespace

GeneDotPlotPlugin::GeneDotPlotPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _settingsAction(this, "Settings")
{
    _aggregationRefreshTimer.setSingleShot(true);
    _aggregationRefreshTimer.setInterval(80);
    _aggregationWorkTimer.setInterval(0);

    connect(&_aggregationRefreshTimer, &QTimer::timeout, this, &GeneDotPlotPlugin::beginAggregation);
    connect(&_aggregationWorkTimer, &QTimer::timeout, this, &GeneDotPlotPlugin::calculateAggregationChunk);
}

void GeneDotPlotPlugin::init()
{
    auto* pluginLayout = new QVBoxLayout(&getWidget());
    pluginLayout->setContentsMargins(8, 8, 8, 8);
    pluginLayout->addWidget(_settingsAction.createWidget(&getWidget()));

    auto* plotSplitter          = new QSplitter(&getWidget());
    auto& markerSelectionAction = _settingsAction.getMarkerSelectionAction();
    plotSplitter->addWidget(markerSelectionAction.createWidget(plotSplitter));
    _geneSelectionListWidget  = markerSelectionAction.getGeneSelectionListWidget();
    _groupSelectionListWidget = markerSelectionAction.getGroupSelectionListWidget();

    auto* plotScrollArea = new QScrollArea(plotSplitter);
    plotScrollArea->setWidgetResizable(true);
    _geneDotPlotWidget = new GeneDotPlotWidget;
    plotScrollArea->setWidget(_geneDotPlotWidget);
    plotSplitter->addWidget(plotScrollArea);
    plotSplitter->setStretchFactor(1, 1);
    plotSplitter->setSizes({220, 900});
    pluginLayout->addWidget(plotSplitter, 1);
    pluginLayout->addWidget(_settingsAction.getStatusMessageAction().createWidget(&getWidget(), StringAction::Label));

    _settingsAction.getExportAction().initialize(_geneDotPlotWidget, &getWidget());
    initializeDropHandling(plotScrollArea);
    initializeDatasetConnections();

    connect(&_settingsAction.getPointDatasetPickerAction(), &DatasetPickerAction::currentDatasetChanged, this, [this](const Dataset<>& dataset) {
        if (!_isRestoringState)
        {
            setPointDataset(Dataset<Points>(dataset));
        }
    });
    connect(&_settingsAction.getClusterDatasetPickerAction(), &DatasetPickerAction::currentDatasetChanged, this, [this](const Dataset<>& dataset) {
        if (!_isRestoringState)
        {
            setClusterDataset(Dataset<Clusters>(dataset));
        }
    });
    connect(&_settingsAction, &SettingsAction::allCellsRequested, this, [this] { setClusterDataset({}); });
    connect(&_settingsAction, &SettingsAction::aggregationSettingsChanged, this, &GeneDotPlotPlugin::scheduleAggregation);
    connect(&_settingsAction, &SettingsAction::displaySettingsChanged, this, &GeneDotPlotPlugin::updatePlotDisplay);
    connect(&markerSelectionAction, &MarkerSelectionAction::selectionChanged, this, &GeneDotPlotPlugin::scheduleAggregation);
    connect(_geneDotPlotWidget, &GeneDotPlotWidget::groupActivated, this, &GeneDotPlotPlugin::selectGroupCells);
    connect(&_settingsAction.getColorMap1DAction(), &ColorMapAction::imageChanged, _geneDotPlotWidget, &GeneDotPlotWidget::setColorMap);
    _geneDotPlotWidget->setColorMap(_settingsAction.getColorMap1DAction().getColorMapImage());

    if (_hasDeferredProjectState)
    {
        _hasDeferredProjectState = false;
        fromVariantMap(_deferredProjectState);
        _deferredProjectState.clear();
    }

    if (!_deferredDatasets.isEmpty())
    {
        const auto datasets = _deferredDatasets;
        _deferredDatasets.clear();
        loadData(datasets);
    }

    scheduleAggregation();
}

void GeneDotPlotPlugin::initializeDropHandling(QScrollArea* plotScrollArea)
{
    plotScrollArea->viewport()->setAcceptDrops(true);
    const auto createDropRegions = [this](const QMimeData* mimeData) -> DropWidget::DropRegions {
        const auto* datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);

        if (!datasetsMimeData || datasetsMimeData->getDatasets().isEmpty() || datasetsMimeData->getDatasets().size() > 2)
        {
            return {};
        }

        bool containsPoints   = false;
        bool containsClusters = false;

        for (const auto& dataset : datasetsMimeData->getDatasets())
        {
            if (!dataset.isValid())
            {
                return {};
            }

            if (dataset->getDataType() == PointType && !containsPoints)
            {
                containsPoints = true;
            }
            else if (dataset->getDataType() == ClusterType && !containsClusters)
            {
                containsClusters = true;
            }
            else
            {
                return {};
            }
        }

        const auto datasets = datasetsMimeData->getDatasets();
        return {new DropWidget::DropRegion(this, "Gene dot plot", "Load expression data and/or cell groups", "braille", true, [this, datasets] { loadData(datasets); })};
    };

    auto* viewportDropWidget = new DropWidget(plotScrollArea->viewport());
    viewportDropWidget->initialize(createDropRegions);

    auto* chartDropWidget = new DropWidget(_geneDotPlotWidget);
    chartDropWidget->initialize(createDropRegions);
}

void GeneDotPlotPlugin::initializeDatasetConnections()
{
    connect(&_pointDataset, &Dataset<Points>::dataChanged, this, &GeneDotPlotPlugin::scheduleAggregation);
    connect(&_pointDataset, &Dataset<Points>::dataDimensionsChanged, this, [this] { rebuildGeneSelection(); });
    connect(&_pointDataset, &Dataset<Points>::guiNameChanged, this, &GeneDotPlotPlugin::scheduleAggregation);
    connect(&_pointDataset, &Dataset<Points>::removed, this, [this] { setPointDataset({}); });
    connect(&_clusterDataset, &Dataset<Clusters>::dataChanged, this, [this] { rebuildGroupSelection(); });
    connect(&_clusterDataset, &Dataset<Clusters>::guiNameChanged, this, &GeneDotPlotPlugin::scheduleAggregation);
    connect(&_clusterDataset, &Dataset<Clusters>::removed, this, [this] { setClusterDataset({}); });
    connect(&_clusterDataset, &Dataset<Clusters>::dataSelectionChanged, this, &GeneDotPlotPlugin::synchronizeGroupSelection);
}

bool GeneDotPlotPlugin::isCompatibleGrouping(const Dataset<Clusters>& clusterDataset) const
{
    if (!_pointDataset.isValid() || !clusterDataset.isValid())
    {
        return false;
    }

    const auto parentDataset = clusterDataset->getParent();
    if (!parentDataset.isValid() || parentDataset->getDataType() != PointType)
    {
        return false;
    }

    const Dataset<Points> groupPointDataset(parentDataset);
    // Proxy global indices are local concatenation indices. Accept only a
    // grouping attached to that exact proxy, never another source by accident.
    if (_pointDataset->isProxy() || groupPointDataset->isProxy())
    {
        return _pointDataset->getId() == groupPointDataset->getId();
    }

    const auto pointSourceDataset = _pointDataset->getSourceDataset<Points>(), groupSourceDataset = groupPointDataset->getSourceDataset<Points>();
    return pointSourceDataset.isValid() && groupSourceDataset.isValid() && pointSourceDataset->getId() == groupSourceDataset->getId();
}

void GeneDotPlotPlugin::loadData(const Datasets& datasets)
{
    if (!_geneDotPlotWidget)
    {
        _deferredDatasets = datasets;
        return;
    }

    Dataset<Points> pointDataset;
    Dataset<Clusters> clusterDataset;
    for (const auto& dataset : datasets)
    {
        if (dataset.isValid())
        {
            if (dataset->getDataType() == PointType && !pointDataset.isValid())
            {
                pointDataset = Dataset<Points>(dataset);
            }
            if (dataset->getDataType() == ClusterType && !clusterDataset.isValid())
            {
                clusterDataset = Dataset<Clusters>(dataset);
            }
        }
    }
    if (!pointDataset.isValid() && !_pointDataset.isValid() && clusterDataset.isValid())
    {
        const auto parentDataset = clusterDataset->getParent();
        if (parentDataset.isValid() && parentDataset->getDataType() == PointType)
        {
            pointDataset = Dataset<Points>(parentDataset);
        }
    }
    if (pointDataset.isValid())
    {
        setPointDataset(pointDataset);
    }
    if (clusterDataset.isValid())
    {
        setClusterDataset(clusterDataset);
    }
}

void GeneDotPlotPlugin::setPointDataset(const Dataset<Points>& pointDataset)
{
    const bool isSameDataset = _pointDataset.isValid() && pointDataset.isValid() && _pointDataset->getId() == pointDataset->getId();
    _aggregationWorkTimer.stop();
    _pointDataset = pointDataset;
    {
        const QSignalBlocker blocker(&_settingsAction.getPointDatasetPickerAction());
        _settingsAction.getPointDatasetPickerAction().setCurrentDataset(_pointDataset);
    }
    if (_clusterDataset.isValid() && !isCompatibleGrouping(_clusterDataset))
    {
        setClusterDataset({});
    }
    rebuildGeneSelection(isSameDataset);
    rebuildGroupSelection();
    scheduleAggregation();
}

void GeneDotPlotPlugin::setClusterDataset(const Dataset<Clusters>& clusterDataset)
{
    if (clusterDataset.isValid() && !_pointDataset.isValid())
    {
        const auto parentDataset = clusterDataset->getParent();
        if (parentDataset.isValid() && parentDataset->getDataType() == PointType)
        {
            setPointDataset(Dataset<Points>(parentDataset));
        }
    }
    if (clusterDataset.isValid() && !isCompatibleGrouping(clusterDataset))
    {
        _groupingWarningMessage = "Groups rejected: choose Clusters attached to Points with the same source cell indices.";
        {
            const QSignalBlocker blocker(&_settingsAction.getClusterDatasetPickerAction());
            _settingsAction.getClusterDatasetPickerAction().setCurrentDataset(_clusterDataset);
        }
        scheduleAggregation();
        return;
    }

    const bool isSameDataset = clusterDataset.isValid() && _clusterDataset.isValid() && clusterDataset->getId() == _clusterDataset->getId();
    _clusterDataset          = clusterDataset;
    _groupingWarningMessage.clear();
    {
        const QSignalBlocker blocker(&_settingsAction.getClusterDatasetPickerAction());
        _settingsAction.getClusterDatasetPickerAction().setCurrentDataset(_clusterDataset);
    }
    rebuildGroupSelection(isSameDataset);
    synchronizeGroupSelection();
    scheduleAggregation();
}

void GeneDotPlotPlugin::rebuildGeneSelection(bool preserveSelection)
{
    if (!_geneSelectionListWidget)
    {
        return;
    }

    const auto selectionState = preserveSelection ? listState(_geneSelectionListWidget, false) : QVariantList{};
    const QSignalBlocker blocker(_geneSelectionListWidget);
    _geneSelectionListWidget->clear();
    if (_pointDataset.isValid())
    {
        const auto& dimensionNames = _pointDataset->getDimensionNames();
        for (int itemIndex = 0; itemIndex < _pointDataset->getNumDimensions(); ++itemIndex)
        {
            const QString featureName = itemIndex < int(dimensionNames.size()) && !dimensionNames[itemIndex].isEmpty() ? dimensionNames[itemIndex] : QString("Feature %1").arg(itemIndex + 1);
            auto* selectionItem       = new QListWidgetItem(featureName, _geneSelectionListWidget);
            selectionItem->setData(Qt::UserRole, itemIndex);
            selectionItem->setToolTip(featureName);
            selectionItem->setHidden(!featureName.contains(_settingsAction.getMarkerSelectionAction().getGeneSearchStringAction().getString(), Qt::CaseInsensitive));
        }
        if (!selectionState.isEmpty())
        {
            restoreList(_geneSelectionListWidget, selectionState, false);
        }
        else if (_geneSelectionListWidget->count())
        {
            _geneSelectionListWidget->item(0)->setSelected(true);
        }
    }
    scheduleAggregation();
}

void GeneDotPlotPlugin::rebuildGroupSelection(bool preserveSelection)
{
    if (!_groupSelectionListWidget)
    {
        return;
    }

    const auto selectionState = preserveSelection ? listState(_groupSelectionListWidget, true) : QVariantList{};
    const QSignalBlocker blocker(_groupSelectionListWidget);
    _groupSelectionListWidget->clear();
    if (_clusterDataset.isValid())
    {
        const auto& clusters = _clusterDataset->getClusters();
        for (int itemIndex = 0; itemIndex < clusters.size(); ++itemIndex)
        {
            auto* selectionItem = new QListWidgetItem(clusters[itemIndex].getName(), _groupSelectionListWidget);
            selectionItem->setData(Qt::UserRole, itemIndex);
            selectionItem->setData(Qt::DecorationRole, clusters[itemIndex].getColor());
            selectionItem->setFlags(selectionItem->flags() | Qt::ItemIsUserCheckable);
            selectionItem->setCheckState(Qt::Checked);
        }
        if (!selectionState.isEmpty())
        {
            restoreList(_groupSelectionListWidget, selectionState, true);
        }
    }
    else
    {
        auto* selectionItem = new QListWidgetItem("All cells", _groupSelectionListWidget);
        selectionItem->setData(Qt::UserRole, -1);
        selectionItem->setFlags(selectionItem->flags() | Qt::ItemIsUserCheckable);
        selectionItem->setCheckState(Qt::Checked);
    }
    scheduleAggregation();
}

genedot::InputScale GeneDotPlotPlugin::getInputScale() const
{
    return static_cast<genedot::InputScale>(_settingsAction.getInputScaleOptionAction().getCurrentIndex());
}

void GeneDotPlotPlugin::scheduleAggregation()
{
    if (_isRestoringState || !_geneDotPlotWidget)
    {
        return;
    }

    _aggregationWorkTimer.stop();
    _aggregationRefreshTimer.start();
    _settingsAction.getExportAction().setExportEnabled(false);
    _geneDotPlotWidget->setMessage("Updating gene/group summaries...");
}

void GeneDotPlotPlugin::beginAggregation()
{
    _aggregationWorkTimer.stop();
    _settingsAction.getExportAction().setExportEnabled(false);
    _displayedGroups.clear();
    _groupMemberLocalIndices.clear();
    _geneSummaries.clear();
    _selectedGeneDimensionIndices.clear();
    _currentGeneExpressionValues.clear();
    _loadedGlobalCellIndices.clear();
    _currentGeneIndex             = 0;
    _currentGroupIndex            = 0;
    _currentMemberOffset          = 0;
    _outsideSubsetMembershipCount = 0;
    _duplicateMembershipCount     = 0;

    _isCurrentGeneLoaded        = false;
    _groupExpressionAccumulator = {};

    const auto message = [this](const QString& text) {
        _geneDotPlotWidget->setMessage(text);
        _settingsAction.getStatusMessageAction().setString(text + (_groupingWarningMessage.isEmpty() ? "" : " " + _groupingWarningMessage));
    };
    if (!_pointDataset.isValid())
    {
        message("Choose expression Points data (rows = cells/spots; dimensions = genes).");
        return;
    }
    if (_clusterDataset.isValid() && !isCompatibleGrouping(_clusterDataset))
    {
        message("The grouping no longer matches the expression data. Choose compatible Clusters or All cells.");
        return;
    }
    for (int itemIndex = 0; itemIndex < _geneSelectionListWidget->count(); ++itemIndex)
    {
        if (_geneSelectionListWidget->item(itemIndex)->isSelected())
        {
            _selectedGeneDimensionIndices.push_back(_geneSelectionListWidget->item(itemIndex)->data(Qt::UserRole).toInt());
        }
    }
    int groupCount = 0;
    for (int itemIndex = 0; itemIndex < _groupSelectionListWidget->count(); ++itemIndex)
    {
        if (_groupSelectionListWidget->item(itemIndex)->checkState() == Qt::Checked)
        {
            ++groupCount;
        }
    }
    if (_selectedGeneDimensionIndices.empty() || groupCount == 0)
    {
        message("Select at least one gene and check at least one group.");
        return;
    }
    if (_selectedGeneDimensionIndices.size() > maximumGeneCount || groupCount > maximumGroupCount || _selectedGeneDimensionIndices.size() * groupCount > maximumDotCount)
    {
        message("Choose at most 250 genes, 2,000 groups, and 25,000 gene/group pairs per view. No selections have been truncated.");
        return;
    }
    if (_pointDataset->getNumPoints() > std::numeric_limits<std::uint32_t>::max())
    {
        message("This dataset exceeds the SDK's 32-bit cell-index range.");
        return;
    }
    try
    {
        const auto loadedCellCount = static_cast<std::size_t>(_pointDataset->getNumPoints());
        _pointDataset->getGlobalIndices(_loadedGlobalCellIndices);
        if (_loadedGlobalCellIndices.size() != loadedCellCount)
        {
            throw std::runtime_error("Global index count does not match the loaded cells.");
        }
        genedot::MembershipIndex membershipIndex;
        if (_clusterDataset.isValid())
        {
            membershipIndex.reset(_loadedGlobalCellIndices);
        }
        for (int groupRow = 0; groupRow < _groupSelectionListWidget->count(); ++groupRow)
        {
            const auto* selectionItem = _groupSelectionListWidget->item(groupRow);
            if (selectionItem->checkState() != Qt::Checked)
            {
                continue;
            }
            const int clusterIndex = selectionItem->data(Qt::UserRole).toInt();
            _groupMemberLocalIndices.emplace_back();
            auto& memberLocalIndices = _groupMemberLocalIndices.back();
            if (_clusterDataset.isValid())
            {
                const auto& cluster = _clusterDataset->getClusters().at(clusterIndex);
                auto membership     = membershipIndex.mapGlobalIndicesToLocalRows(cluster.getIndices());
                _duplicateMembershipCount += membership.duplicateIndexCount;
                _outsideSubsetMembershipCount += membership.outsideSubsetCount;
                memberLocalIndices = std::move(membership.localRowIndices);
                _displayedGroups.push_back({cluster.getName(), cluster.getColor(), clusterIndex, memberLocalIndices.size()});
            }
            else
            {
                memberLocalIndices.resize(loadedCellCount);
                std::iota(memberLocalIndices.begin(), memberLocalIndices.end(), 0u);
                _displayedGroups.push_back({"All cells", QColor("#64748b"), -1, loadedCellCount});
            }
        }
        _geneSummaries.reserve(_selectedGeneDimensionIndices.size());
        _settingsAction.getStatusMessageAction().setString("Calculating summaries from all loaded cells; yielding between chunks...");
        _aggregationWorkTimer.start();
    }
    catch (const std::exception& exception)
    {
        message(QString("Cannot prepare dot plot: %1").arg(QString::fromUtf8(exception.what())));
    }
}

void GeneDotPlotPlugin::calculateAggregationChunk()
{
    try
    {
        if (!_pointDataset.isValid() || _currentGeneIndex >= _selectedGeneDimensionIndices.size())
        {
            _aggregationWorkTimer.stop();
            return;
        }
        if (!_isCurrentGeneLoaded)
        {
            const int dimension = _selectedGeneDimensionIndices[_currentGeneIndex];
            if (dimension < 0 || dimension >= _pointDataset->getNumDimensions())
            {
                throw std::runtime_error("Gene dimensions changed while calculating.");
            }
            _pointDataset->extractDataForDimension(_currentGeneExpressionValues, dimension);
            if (_currentGeneExpressionValues.size() != static_cast<std::size_t>(_pointDataset->getNumPoints()))
            {
                throw std::runtime_error("Expression length differs from cell loadedCellCount.");
            }
            QString featureName;
            for (int itemIndex = 0; itemIndex < _geneSelectionListWidget->count(); ++itemIndex)
            {
                if (_geneSelectionListWidget->item(itemIndex)->data(Qt::UserRole).toInt() == dimension)
                {
                    featureName = _geneSelectionListWidget->item(itemIndex)->text();
                    break;
                }
            }
            _geneSummaries.push_back({featureName, dimension, {}});
            _geneSummaries.back().groupSummaries.reserve(_displayedGroups.size());
            _isCurrentGeneLoaded = true;
            _currentGroupIndex = _currentMemberOffset = 0;
            _groupExpressionAccumulator               = {};
        }
        std::size_t remainingMembershipBudget = aggregationChunkSize;
        while (_currentGroupIndex < _groupMemberLocalIndices.size() && remainingMembershipBudget)
        {
            const auto& memberLocalIndices = _groupMemberLocalIndices[_currentGroupIndex];
            const auto membershipEnd       = _currentMemberOffset + std::min(remainingMembershipBudget, memberLocalIndices.size() - _currentMemberOffset);
            for (; _currentMemberOffset < membershipEnd; ++_currentMemberOffset, --remainingMembershipBudget)
            {
                _groupExpressionAccumulator.addObservation(_currentGeneExpressionValues[memberLocalIndices[_currentMemberOffset]], getInputScale(), _settingsAction.getDetectionThresholdAction().getValue());
            }
            if (_currentMemberOffset == memberLocalIndices.size())
            {
                _geneSummaries.back().groupSummaries.push_back(_groupExpressionAccumulator.getSummary(getInputScale()));
                ++_currentGroupIndex;
                _currentMemberOffset        = 0;
                _groupExpressionAccumulator = {};
                // Even empty groups consume budget to keep callbacks bounded.
                if (remainingMembershipBudget)
                {
                    --remainingMembershipBudget;
                }
            }
        }
        if (_currentGroupIndex == _groupMemberLocalIndices.size())
        {
            ++_currentGeneIndex;
            _isCurrentGeneLoaded = false;
            _settingsAction.getStatusMessageAction().setString(QString("Calculated %1 / %2 genes.").arg(qulonglong(_currentGeneIndex)).arg(qulonglong(_selectedGeneDimensionIndices.size())));
            if (_currentGeneIndex == _selectedGeneDimensionIndices.size())
            {
                finishAggregation();
            }
        }
    }
    catch (const std::exception& exception)
    {
        _aggregationWorkTimer.stop();
        _geneDotPlotWidget->setMessage("Calculation failed.");
        _settingsAction.getStatusMessageAction().setString(QString("Calculation failed: %1").arg(QString::fromUtf8(exception.what())));
        _settingsAction.getExportAction().setExportEnabled(false);
    }
}

QString GeneDotPlotPlugin::getMethodDescription() const
{
    const QString scale = _settingsAction.getInputScaleOptionAction().getCurrentIndex() == 0   ? "Mean of stored values."
                          : _settingsAction.getInputScaleOptionAction().getCurrentIndex() == 1 ? "Color = log1p(mean of linear normalized values)."
                                                                                               : "Log1p normalized input: mean = mean(expm1(input)); color = log1p(mean).";
    return QString("%1 Detection: input > %2; denominator = valid cells including zeros. Missing/rejected values excluded. %3 No normalization or statistical testing.")
        .arg(scale)
        .arg(_settingsAction.getDetectionThresholdAction().getValue(), 0, 'g', 8)
        .arg(_settingsAction.getStandardizeExpressionToggleAction().isChecked() ? QString("Colors z-scored across displayed groups per gene, clipped to +/-%1.").arg(_settingsAction.getStandardDeviationClipAction().getValue()) : "Colors unstandardized.");
}

void GeneDotPlotPlugin::finishAggregation()
{
    _aggregationWorkTimer.stop();
    _currentGeneExpressionValues.clear();
    std::size_t missingValueCount  = 0;
    std::size_t rejectedValueCount = 0;
    for (const auto& gene : _geneSummaries)
    {
        for (const auto& group : gene.groupSummaries)
        {
            missingValueCount += group.missingValueCount;
            rejectedValueCount += group.rejectedValueCount;
        }
    }

    QString status = QString("%1 genes x %2 groups; %3 loaded cells/spots. All valid observations used; overlapping groups counted independently.")
                         .arg(qulonglong(_geneSummaries.size()))
                         .arg(qulonglong(_displayedGroups.size()))
                         .arg(qulonglong(_pointDataset->getNumPoints()));
    if (_outsideSubsetMembershipCount)
    {
        status += QString(" %1 memberships outside loaded cells.").arg(qulonglong(_outsideSubsetMembershipCount));
    }
    if (_duplicateMembershipCount)
    {
        status += QString(" %1 duplicate memberships removed.").arg(qulonglong(_duplicateMembershipCount));
    }
    if (missingValueCount || rejectedValueCount)
    {
        status += QString(" %1 missing; %2 rejected gene/group observations (see tooltips/CSV).").arg(qulonglong(missingValueCount)).arg(qulonglong(rejectedValueCount));
    }
    if (_settingsAction.getStandardizeExpressionToggleAction().isChecked() && _displayedGroups.size() < 5)
    {
        status += " Z-scores with few groups can be misleading.";
    }
    if (getInputScale() == genedot::InputScale::AsStored)
    {
        status += " For signed/centered data, above-threshold fraction is not biological detection.";
    }
    if (!_groupingWarningMessage.isEmpty())
    {
        status += ' ' + _groupingWarningMessage;
    }

    _settingsAction.getStatusMessageAction().setString(status);
    updatePlotDisplay();
    _settingsAction.getExportAction().setExportEnabled(true);
    synchronizeGroupSelection();
}

void GeneDotPlotPlugin::updatePlotDisplay()
{
    if (!_geneDotPlotWidget || _isRestoringState)
    {
        return;
    }

    _geneDotPlotWidget->setOptions(_settingsAction.getStandardizeExpressionToggleAction().isChecked(), _settingsAction.getStandardDeviationClipAction().getValue(), _settingsAction.getMinimumDetectionPercentageAction().getValue() / 100, _settingsAction.getMaximumDotDiameterAction().getValue());
    if (!_geneSummaries.empty() && !_aggregationWorkTimer.isActive() && !_aggregationRefreshTimer.isActive() && _currentGeneIndex == _selectedGeneDimensionIndices.size())
    {
        const QString title = (_pointDataset.isValid() ? _pointDataset->getGuiName() : "Expression") + " / " + (_clusterDataset.isValid() ? _clusterDataset->getGuiName() : "All cells");
        _geneDotPlotWidget->setData(_displayedGroups, _geneSummaries, title, getMethodDescription());
    }
}

void GeneDotPlotPlugin::synchronizeGroupSelection()
{
    QSet<int> selected;
    if (_clusterDataset.isValid())
    {
        for (const auto index : _clusterDataset->getSelectionIndices())
        {
            if (index < std::uint32_t(_clusterDataset->getClusters().size()))
            {
                selected.insert(int(index));
            }
        }
    }
    if (_geneDotPlotWidget)
    {
        _geneDotPlotWidget->setSelectedClusters(selected);
    }
}

void GeneDotPlotPlugin::selectGroupCells(int clusterIndex, bool toggleSelection)
{
    if (!_pointDataset.isValid())
    {
        return;
    }
    if (_clusterDataset.isValid())
    {
        if (clusterIndex < 0 || clusterIndex >= _clusterDataset->getClusters().size())
        {
            return;
        }
        auto selectedClusterIndices        = _clusterDataset->getSelectionIndices();
        const auto selectedClusterIterator = std::find(selectedClusterIndices.begin(), selectedClusterIndices.end(), std::uint32_t(clusterIndex));
        if (!toggleSelection)
        {
            selectedClusterIndices = {std::uint32_t(clusterIndex)};
        }
        else if (selectedClusterIterator == selectedClusterIndices.end())
        {
            selectedClusterIndices.push_back(std::uint32_t(clusterIndex));
        }
        else
        {
            selectedClusterIndices.erase(selectedClusterIterator);
        }
        _clusterDataset->setSelectionIndices(selectedClusterIndices);
        events().notifyDatasetDataSelectionChanged(_clusterDataset);
        std::unordered_set<std::uint32_t> selectedGlobalCellIndices;
        for (const auto index : selectedClusterIndices)
        {
            if (index < std::uint32_t(_clusterDataset->getClusters().size()))
            {
                for (const auto globalCellIndex : _clusterDataset->getClusters()[index].getIndices())
                {
                    selectedGlobalCellIndices.insert(globalCellIndex);
                }
            }
        }
        std::vector<std::uint32_t> selectedLoadedCellIndices;
        for (const auto index : _loadedGlobalCellIndices)
        {
            if (selectedGlobalCellIndices.count(index))
            {
                selectedLoadedCellIndices.push_back(index);
            }
        }
        std::sort(selectedLoadedCellIndices.begin(), selectedLoadedCellIndices.end());
        selectedLoadedCellIndices.erase(std::unique(selectedLoadedCellIndices.begin(), selectedLoadedCellIndices.end()), selectedLoadedCellIndices.end());
        _pointDataset->setSelectionIndices(selectedLoadedCellIndices);
    }
    else
    {
        _pointDataset->setSelectionIndices(_loadedGlobalCellIndices);
    }
    events().notifyDatasetDataSelectionChanged(_pointDataset);
    synchronizeGroupSelection();
}

void GeneDotPlotPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    if (!_geneDotPlotWidget)
    {
        _deferredProjectState    = variantMap;
        _hasDeferredProjectState = true;
        return;
    }

    ViewPlugin::fromVariantMap(variantMap);

    {
        QScopedValueRollback<bool> restoringStateGuard(_isRestoringState, true);

        if (variantMap.value("GeneDotPlot:ActionSettingsVersion", 0).toInt() >= 2)
        {
            _settingsAction.fromParentVariantMap(variantMap);
        }
        else
        {
            _settingsAction.restoreLegacySettings(variantMap);
        }

        // Capture both references before changing data: setPointDataset can
        // clear an incompatible grouping left over from the current project.
        const auto restoredPointDataset   = _settingsAction.getPointDatasetPickerAction().getCurrentDataset<Points>();
        const auto restoredClusterDataset = _settingsAction.getClusterDatasetPickerAction().getCurrentDataset<Clusters>();
        setPointDataset(restoredPointDataset);
        setClusterDataset(restoredClusterDataset);
        _settingsAction.getMarkerSelectionAction().applyPendingSelectionState();
    }

    scheduleAggregation();
}

QVariantMap GeneDotPlotPlugin::toVariantMap() const
{
    if (!_geneDotPlotWidget && _hasDeferredProjectState)
    {
        return _deferredProjectState;
    }

    auto variantMap                                 = ViewPlugin::toVariantMap();
    variantMap["GeneDotPlot:ActionSettingsVersion"] = 2;
    _settingsAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

GeneDotPlotPluginFactory::GeneDotPlotPluginFactory()
{
    setIconByName("braille");
}

ViewPlugin* GeneDotPlotPluginFactory::produce()
{
    return new GeneDotPlotPlugin(this);
}

DataTypes GeneDotPlotPluginFactory::supportedDataTypes() const
{
    return {PointType, ClusterType};
}

PluginTriggerActions GeneDotPlotPluginFactory::getPluginTriggerActions(const Datasets& datasets) const
{
    if (datasets.isEmpty() || datasets.size() > 2)
    {
        return {};
    }

    bool containsPoints   = false;
    bool containsClusters = false;

    for (const auto& dataset : datasets)
    {
        if (!dataset.isValid())
        {
            return {};
        }

        if (dataset->getDataType() == PointType && !containsPoints)
        {
            containsPoints = true;
        }
        else if (dataset->getDataType() == ClusterType && !containsClusters)
        {
            containsClusters = true;
        }
        else
        {
            return {};
        }
    }

    return {new PluginTriggerAction(const_cast<GeneDotPlotPluginFactory*>(this), this, "Gene dot plot", "Compare marker expression and detection across cell groups", icon(), [this, datasets](PluginTriggerAction&) {
        auto* geneDotPlotPlugin = dynamic_cast<GeneDotPlotPlugin*>(plugins().requestViewPlugin(getKind()));

        if (geneDotPlotPlugin)
        {
            geneDotPlotPlugin->loadData(datasets);
        }
    })};
}
