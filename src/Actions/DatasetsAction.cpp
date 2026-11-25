#include "DatasetsAction.h"

#include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>

using namespace mv;
using namespace mv::gui;

DatasetsAction::DatasetsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _pointDatasetPickerAction(this, "Expression"),
    _clusterDatasetPickerAction(this, "Groups"),
    _inputScaleOptionAction(this, "Input scale", {"Mean", "Linear normalized: log1p(mean)", "Log1p normalized: back-transform, mean, log1p"})
{
    setIconByName("database");
    setToolTip("Datasets: expression data, cell groups and input scale");
    setDefaultWidgetFlags(GroupAction::Horizontal);
    setLabelSizingType(LabelSizingType::Auto);

    _pointDatasetPickerAction.setFilterFunction([](const Dataset<>& dataset) { return dataset.isValid() && dataset->getDataType() == PointType; });
    _clusterDatasetPickerAction.setFilterFunction([](const Dataset<>& dataset) { return dataset.isValid() && dataset->getDataType() == ClusterType; });
    _clusterDatasetPickerAction.setDefaultWidgetFlag(OptionAction::Clearable);

    _inputScaleOptionAction.setCurrentIndex(0);
    _pointDatasetPickerAction.setToolTip("Expression Points: cells/spots as rows, genes as dimensions");
    _clusterDatasetPickerAction.setToolTip("Clusters whose Points parent shares the expression dataset's source-cell index space. Clear this selection to summarize all loaded cells.");
    _inputScaleOptionAction.setToolTip("Choose the supplied expression scale. No library-size normalization is performed. The third mode averages natural-log1p-normalized input following Seurat.");
    
    addAction(&_pointDatasetPickerAction);
    addAction(&_clusterDatasetPickerAction);
    addAction(&_inputScaleOptionAction);

    connect(&_inputScaleOptionAction, &OptionAction::currentIndexChanged, this, &DatasetsAction::aggregationSettingsChanged);
}

void DatasetsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _pointDatasetPickerAction.fromParentVariantMap(variantMap);
    _clusterDatasetPickerAction.fromParentVariantMap(variantMap);
    _inputScaleOptionAction.fromParentVariantMap(variantMap);
}

QVariantMap DatasetsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();
    _pointDatasetPickerAction.insertIntoVariantMap(variantMap);
    _clusterDatasetPickerAction.insertIntoVariantMap(variantMap);
    _inputScaleOptionAction.insertIntoVariantMap(variantMap);
    return variantMap;
}
