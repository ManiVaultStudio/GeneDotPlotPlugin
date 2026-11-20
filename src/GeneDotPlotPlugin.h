#pragma once

#include "Actions/SettingsAction.h"
#include "GeneDotPlotWidget.h"

#include <Dataset.h>
#include <ViewPlugin.h>

#include <QTimer>

#include <cstdint>
#include <vector>

class Points;
class Clusters;
class QListWidget;
class QScrollArea;

/** Dataset integration and chunked expression aggregation for the dot plot. */
class GeneDotPlotPlugin : public mv::plugin::ViewPlugin
{
    Q_OBJECT

public:
    explicit GeneDotPlotPlugin(const mv::plugin::PluginFactory* factory);
    void init() override;
    void loadData(const mv::Datasets& datasets) override;
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;
    SettingsAction& getSettingsAction() { return _settingsAction; }

private:
    void initializeDropHandling(QScrollArea* plotScrollArea);
    void initializeDatasetConnections();
    bool isCompatibleGrouping(const mv::Dataset<Clusters>& clusterDataset) const;
    void setPointDataset(const mv::Dataset<Points>& pointDataset);
    void setClusterDataset(const mv::Dataset<Clusters>& clusterDataset);
    void rebuildGeneSelection(bool preserveSelection = true);
    void rebuildGroupSelection(bool preserveSelection = true);
    void scheduleAggregation();
    void beginAggregation();
    void calculateAggregationChunk();
    void finishAggregation();
    void updatePlotDisplay();
    void synchronizeGroupSelection();
    void selectGroupCells(int clusterIndex, bool toggleSelection);
    QString getMethodDescription() const;
    genedot::InputScale getInputScale() const;

    mv::Dataset<Points> _pointDataset;
    mv::Dataset<Clusters> _clusterDataset;
    SettingsAction _settingsAction;
    GeneDotPlotWidget* _geneDotPlotWidget  = nullptr;
    QListWidget* _geneSelectionListWidget  = nullptr;
    QListWidget* _groupSelectionListWidget = nullptr;
    QTimer _aggregationRefreshTimer;
    QTimer _aggregationWorkTimer;

    std::vector<std::uint32_t> _loadedGlobalCellIndices;
    std::vector<std::vector<std::uint32_t>> _groupMemberLocalIndices;
    std::vector<DotPlotGroup> _displayedGroups;
    std::vector<DotPlotGene> _geneSummaries;
    std::vector<int> _selectedGeneDimensionIndices;
    std::vector<float> _currentGeneExpressionValues;
    genedot::Accumulator _groupExpressionAccumulator;
    std::size_t _currentGeneIndex             = 0;
    std::size_t _currentGroupIndex            = 0;
    std::size_t _currentMemberOffset          = 0;
    std::size_t _outsideSubsetMembershipCount = 0;
    std::size_t _duplicateMembershipCount     = 0;
    bool _isCurrentGeneLoaded                 = false;
    bool _isRestoringState                    = false;
    bool _hasDeferredProjectState             = false;
    QString _groupingWarningMessage;
    mv::Datasets _deferredDatasets;
    QVariantMap _deferredProjectState;
};

class GeneDotPlotPluginFactory : public mv::plugin::ViewPluginFactory
{
    Q_OBJECT
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_PLUGIN_METADATA(IID "studio.manivault.GeneDotPlotPlugin" FILE "PluginInfo.json")

public:
    GeneDotPlotPluginFactory();
    mv::plugin::ViewPlugin* produce() override;
    mv::DataTypes supportedDataTypes() const override;
    mv::gui::PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
