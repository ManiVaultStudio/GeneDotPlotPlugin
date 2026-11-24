#include "SettingsAction.h"

#include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>

#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace mv;
using namespace mv::gui;

namespace {
const QString defaultColorMapName = QStringLiteral("Reds");
}

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title, true),
    _pointDatasetPickerAction(this, "Expression dataset"),
    _clusterDatasetPickerAction(this, "Cell groups"),
    _inputScaleOptionAction(this, "Input scale", {"As stored: mean", "Linear normalized: log1p(mean)", "Log1p normalized: back-transform, mean, log1p"}),
    _detectionThresholdAction(this, "Detection threshold", -1e9f, 1e9f, 0.0f, 6),
    _standardizeExpressionToggleAction(this, "Per-gene z-score", false),
    _standardDeviationClipAction(this, "Z-score clipping", 0.5f, 5.0f, 2.5f, 1),
    _minimumDetectionPercentageAction(this, "Minimum detection (%)", 0.0f, 100.0f, 0.0f, 1),
    _maximumDotDiameterAction(this, "Maximum diameter (px)", 8.0f, 32.0f, 22.0f, 0),
    _colorMap1DAction(this, "1D Color map", defaultColorMapName),
    _allCellsTriggerAction(this, "All cells"),
    _helpTriggerAction(this, "Help"),
    _markerSelectionAction(this, "Marker selection"),
    _exportAction(this, "Export"),
    _statusMessageAction(this, "Status")
{
    setSerializationName("GeneDotPlot:Settings");
    _pointDatasetPickerAction.setSerializationName("Expression dataset");
    _clusterDatasetPickerAction.setSerializationName("Cell groups");
    setConnectionPermissionsToForceNone();
    _pointDatasetPickerAction.setFilterFunction([](const Dataset<>& dataset) { return dataset.isValid() && dataset->getDataType() == PointType; });
    _clusterDatasetPickerAction.setFilterFunction([](const Dataset<>& dataset) { return dataset.isValid() && dataset->getDataType() == ClusterType; });
    _pointDatasetPickerAction.setDefaultWidgetFlag(OptionAction::Clearable);
    _clusterDatasetPickerAction.setDefaultWidgetFlag(OptionAction::Clearable);
    _inputScaleOptionAction.setCurrentIndex(0);
    _standardDeviationClipAction.setEnabled(false);
    _colorMap1DAction.setDefaultWidgetFlags(ColorMapAction::Default);
    _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X).setEnabled(false);

    _pointDatasetPickerAction.setToolTip("Expression Points: cells/spots as rows, genes as dimensions");
    _clusterDatasetPickerAction.setToolTip("Clusters whose Points parent shares the expression dataset's source-cell index space");
    _inputScaleOptionAction.setToolTip("Choose the supplied expression scale. No library-size normalization is performed. The third mode averages natural-log1p-normalized input following Seurat.");
    _detectionThresholdAction.setToolTip("Detection requires INPUT expression strictly above this value. Mean expression includes valid observations below the threshold, including zeros.");
    _standardizeExpressionToggleAction.setToolTip("Standardize color across displayed groups for each gene. Constant/single-group genes map to zero. Scaled colors cannot compare absolute expression between genes.");
    _standardDeviationClipAction.setToolTip("Symmetric clipping of per-gene z-scores, in standard deviations");
    _minimumDetectionPercentageAction.setToolTip("Hide dots below this percentage without changing summaries or the fixed 0-100% area scale");
    _maximumDotDiameterAction.setToolTip("Diameter in pixels of a dot with 100% detection");
    _colorMap1DAction.setToolTip("Choose the palette for dots and the color legend. Open the settings button to adjust its range, reverse the palette, or use discrete colors.");
    _allCellsTriggerAction.setToolTip("Remove cluster grouping and summarize all loaded cells");

    addAction(&_pointDatasetPickerAction);
    addAction(&_clusterDatasetPickerAction);
    addAction(&_inputScaleOptionAction);
    addAction(&_detectionThresholdAction, DecimalAction::SpinBox);
    addAction(&_standardizeExpressionToggleAction, ToggleAction::CheckBox);
    addAction(&_standardDeviationClipAction, DecimalAction::SpinBox);
    addAction(&_minimumDetectionPercentageAction, DecimalAction::SpinBox);
    addAction(&_maximumDotDiameterAction, DecimalAction::SpinBox);
    addAction(&_colorMap1DAction, ColorMapAction::Default);
    addAction(&_allCellsTriggerAction, TriggerAction::Text);
    addAction(&_helpTriggerAction, TriggerAction::Text);
    addAction(&_markerSelectionAction);
    addAction(&_exportAction);
    addAction(&_statusMessageAction, StringAction::Label);
    _statusMessageAction.setWidgetConfigurationFunction([](WidgetAction*, QWidget* widget) {
        if (auto* statusLabel = qobject_cast<QLabel*>(widget))
        {
            statusLabel->setWordWrap(true);
            statusLabel->setTextFormat(Qt::PlainText);
        }
    });

    connect(&_inputScaleOptionAction, &OptionAction::currentIndexChanged, this, &SettingsAction::aggregationSettingsChanged);
    connect(&_detectionThresholdAction, &DecimalAction::valueChanged, this, &SettingsAction::aggregationSettingsChanged);
    connect(&_standardizeExpressionToggleAction, &ToggleAction::toggled, this, [this](bool checked) {
        _standardDeviationClipAction.setEnabled(checked);
        emit displaySettingsChanged();
    });
    connect(&_standardDeviationClipAction, &DecimalAction::valueChanged, this, &SettingsAction::displaySettingsChanged);
    connect(&_minimumDetectionPercentageAction, &DecimalAction::valueChanged, this, &SettingsAction::displaySettingsChanged);
    connect(&_maximumDotDiameterAction, &DecimalAction::valueChanged, this, &SettingsAction::displaySettingsChanged);
    connect(&_colorMap1DAction.getRangeAction(ColorMapAction::Axis::X), &DecimalRangeAction::rangeChanged, this, [this](const util::NumericalRange<float>& range) {
        emit colorMapRangeChanged(range.getMinimum(), range.getMaximum());
    });
    connect(&_allCellsTriggerAction, &TriggerAction::triggered, this, &SettingsAction::allCellsRequested);
    connect(&_helpTriggerAction, &TriggerAction::triggered, this, &SettingsAction::showHelp);
}

QWidget* SettingsAction::getWidget(QWidget* parent, const std::int32_t& widgetFlags)
{
    Q_UNUSED(widgetFlags)
    _dialogParentWidget = parent;

    auto* settingsWidget = new QWidget(parent);
    auto* settingsLayout = new QGridLayout(settingsWidget);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->addWidget(new QLabel("Expression:", settingsWidget), 0, 0);
    settingsLayout->addWidget(_pointDatasetPickerAction.createWidget(settingsWidget), 0, 1, 1, 2);
    settingsLayout->addWidget(new QLabel("Groups:", settingsWidget), 0, 3);
    settingsLayout->addWidget(_clusterDatasetPickerAction.createWidget(settingsWidget), 0, 4, 1, 2);
    settingsLayout->addWidget(_allCellsTriggerAction.createWidget(settingsWidget, TriggerAction::Text), 0, 6);
    settingsLayout->addWidget(_helpTriggerAction.createWidget(settingsWidget, TriggerAction::Text), 0, 7);

    settingsLayout->addWidget(new QLabel("Input / color:", settingsWidget), 1, 0);
    settingsLayout->addWidget(_inputScaleOptionAction.createWidget(settingsWidget), 1, 1, 1, 3);
    settingsLayout->addWidget(new QLabel("Detection >", settingsWidget), 1, 4);
    settingsLayout->addWidget(_detectionThresholdAction.createWidget(settingsWidget, DecimalAction::SpinBox), 1, 5, 1, 3);
    settingsLayout->addWidget(_standardizeExpressionToggleAction.createWidget(settingsWidget, ToggleAction::CheckBox), 2, 0, 1, 2);
    settingsLayout->addWidget(new QLabel("Z-score clip (+/-):", settingsWidget), 2, 2);
    settingsLayout->addWidget(_standardDeviationClipAction.createWidget(settingsWidget, DecimalAction::SpinBox), 2, 3);
    settingsLayout->addWidget(new QLabel("Minimum detection (%):", settingsWidget), 2, 4);
    settingsLayout->addWidget(_minimumDetectionPercentageAction.createWidget(settingsWidget, DecimalAction::SpinBox), 2, 5, 1, 3);
    settingsLayout->addWidget(new QLabel("Maximum diameter (px):", settingsWidget), 3, 0, 1, 2);
    settingsLayout->addWidget(_maximumDotDiameterAction.createWidget(settingsWidget, DecimalAction::SpinBox), 3, 2, 1, 2);
    settingsLayout->addWidget(_exportAction.createWidget(settingsWidget, GroupAction::Horizontal | GroupAction::NoMargins), 3, 4, 1, 4);
    settingsLayout->addWidget(new QLabel("1D Color map:", settingsWidget), 4, 0, 1, 2);
    settingsLayout->addWidget(_colorMap1DAction.createWidget(settingsWidget, ColorMapAction::Default), 4, 2, 1, 6);
    settingsLayout->setColumnStretch(1, 1);
    settingsLayout->setColumnStretch(5, 1);

    return settingsWidget;
}

void SettingsAction::updateColorMapDataRange(double minimumColorDataValue, double maximumColorDataValue)
{
    if (!std::isfinite(minimumColorDataValue) || !std::isfinite(maximumColorDataValue) || minimumColorDataValue >= maximumColorDataValue)
    {
        return;
    }

    const double maximumRangeValue = std::numeric_limits<float>::max();
    float minimumDataValue = float(std::clamp(minimumColorDataValue, -maximumRangeValue, maximumRangeValue));
    float maximumDataValue = float(std::clamp(maximumColorDataValue, -maximumRangeValue, maximumRangeValue));
    if (minimumDataValue == maximumDataValue)
    {
        if (maximumDataValue < std::numeric_limits<float>::max())
        {
            maximumDataValue = std::nextafter(maximumDataValue, std::numeric_limits<float>::max());
        }
        else
        {
            minimumDataValue = std::nextafter(minimumDataValue, std::numeric_limits<float>::lowest());
        }
    }

    const util::NumericalRange<float> dataRange(minimumDataValue, maximumDataValue);
    auto& colorMapRangeAction = _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X);
    auto& colorMapDataRangeAction = _colorMap1DAction.getDataRangeAction(ColorMapAction::Axis::X);
    if (!_hasColorMapDataRange || colorMapDataRangeAction.getRange() != dataRange)
    {
        auto colorMapRange = dataRange;
        if (_preserveRestoredColorMapRange)
        {
            const auto restoredMinimum = std::clamp(colorMapRangeAction.getMinimum(), minimumDataValue, maximumDataValue);
            const auto restoredMaximum = std::clamp(colorMapRangeAction.getMaximum(), minimumDataValue, maximumDataValue);
            if (std::isfinite(restoredMinimum) && std::isfinite(restoredMaximum) && restoredMinimum < restoredMaximum)
            {
                colorMapRange = { restoredMinimum, restoredMaximum };
            }
        }

        // Use the same range/data-range actions as Scatterplot. Block intermediate
        // endpoint changes so the plot receives the completed range only.
        const QSignalBlocker rangeSignalBlocker(&colorMapRangeAction);
        colorMapRangeAction.initialize(dataRange, colorMapRange);
        colorMapRangeAction.getRangeMinAction().setNumberOfDecimals(6);
        colorMapRangeAction.getRangeMaxAction().setNumberOfDecimals(6);
        colorMapDataRangeAction.initialize(dataRange, dataRange);
        colorMapRangeAction.setEnabled(true);
        _hasColorMapDataRange = true;
        _preserveRestoredColorMapRange = false;
    }

    emit colorMapRangeChanged(colorMapRangeAction.getMinimum(), colorMapRangeAction.getMaximum());
}

void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);
    _pointDatasetPickerAction.fromParentVariantMap(variantMap);
    _clusterDatasetPickerAction.fromParentVariantMap(variantMap);
    _inputScaleOptionAction.fromParentVariantMap(variantMap);
    _detectionThresholdAction.fromParentVariantMap(variantMap);
    _standardizeExpressionToggleAction.fromParentVariantMap(variantMap);
    _standardDeviationClipAction.fromParentVariantMap(variantMap);
    _minimumDetectionPercentageAction.fromParentVariantMap(variantMap);
    _maximumDotDiameterAction.fromParentVariantMap(variantMap);
    if (variantMap.contains(_colorMap1DAction.getSerializationName()))
    {
        _colorMap1DAction.fromParentVariantMap(variantMap);
    }
    else
    {
        _colorMap1DAction.setColorMap(defaultColorMapName);
    }

    _hasColorMapDataRange = false;
    // Earlier versions serialized an unused default range; only restore ranges
    // saved after range editing was enabled for this plugin.
    _preserveRestoredColorMapRange = variantMap.value("Color map range initialized", false).toBool();

    _markerSelectionAction.fromParentVariantMap(variantMap);
    _standardDeviationClipAction.setEnabled(_standardizeExpressionToggleAction.isChecked());
}

QVariantMap SettingsAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();
    _pointDatasetPickerAction.insertIntoVariantMap(variantMap);
    _clusterDatasetPickerAction.insertIntoVariantMap(variantMap);
    _inputScaleOptionAction.insertIntoVariantMap(variantMap);
    _detectionThresholdAction.insertIntoVariantMap(variantMap);
    _standardizeExpressionToggleAction.insertIntoVariantMap(variantMap);
    _standardDeviationClipAction.insertIntoVariantMap(variantMap);
    _minimumDetectionPercentageAction.insertIntoVariantMap(variantMap);
    _maximumDotDiameterAction.insertIntoVariantMap(variantMap);
    _colorMap1DAction.insertIntoVariantMap(variantMap);
    variantMap["Color map range initialized"] = _hasColorMapDataRange || _preserveRestoredColorMapRange;
    _markerSelectionAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

void SettingsAction::restoreLegacySettings(const QVariantMap& pluginVariantMap)
{
    // Keep projects from the original QWidget-based implementation readable.
    _colorMap1DAction.setColorMap(defaultColorMapName);
    _hasColorMapDataRange = false;
    _preserveRestoredColorMapRange = false;
    if (pluginVariantMap.contains("GeneDotPlot:Expression"))
    {
        _pointDatasetPickerAction.fromVariantMap(pluginVariantMap.value("GeneDotPlot:Expression").toMap());
    }

    if (pluginVariantMap.contains("GeneDotPlot:Groups"))
    {
        _clusterDatasetPickerAction.fromVariantMap(pluginVariantMap.value("GeneDotPlot:Groups").toMap());
    }

    _pointDatasetPickerAction.setSerializationName("Expression dataset");
    _clusterDatasetPickerAction.setSerializationName("Cell groups");

    const auto settingsVariantMap = pluginVariantMap.value("GeneDotPlot:Settings").toMap();
    _inputScaleOptionAction.setCurrentIndex(std::clamp(settingsVariantMap.value("inputScale", 0).toInt(), 0, 2));
    _detectionThresholdAction.setValue(settingsVariantMap.value("threshold", 0).toFloat());
    _standardizeExpressionToggleAction.setChecked(settingsVariantMap.value("scaled", false).toBool());
    _standardDeviationClipAction.setValue(settingsVariantMap.value("clip", 2.5).toFloat());
    _minimumDetectionPercentageAction.setValue(settingsVariantMap.value("minimum", 0).toFloat());
    _maximumDotDiameterAction.setValue(settingsVariantMap.value("diameter", 22).toFloat());
    _standardDeviationClipAction.setEnabled(_standardizeExpressionToggleAction.isChecked());
    _markerSelectionAction.queueSelectionState(settingsVariantMap.value("genes").toList(), settingsVariantMap.value("groups").toList(), settingsVariantMap.value("search").toString());
}

void SettingsAction::showHelp()
{
    const auto helpText = QString::fromUtf8(R"HELP(Load expression Points data (rows = cells/spots, dimensions = genes), then optionally compatible Clusters. Dropping Clusters alone uses its Points parent. All cells removes grouping.

Ctrl/Shift-select genes or paste a marker list. Drag genes and groups to reorder. Check groups to show them. Searching hides names without changing selection.

Dot AREA is proportional to the fraction of valid cells whose INPUT value is strictly above the detection threshold. The size scale always spans 0-100%. Mean expression includes zeros and all other valid observations. A blank is zero detection or a hidden dot; x means no valid values. Hover for exact counts and summaries.

Input/color modes:
As stored: average supplied values without transforming. For signed/centered/residual data, an above-threshold fraction is not biological detection.
Linear normalized: log1p(mean(input)).
Log1p normalized: mean(expm1(input)), then log1p for color, following Seurat-style averaging. Use ONLY when the input is already log1p-normalized with natural logarithms.
The last two modes reject negative input. Nonfinite input and back-transform overflow are excluded and reported. No library-size normalization is performed.

Per-gene z-score standardizes color across DISPLAYED groups, using sample standard deviation and symmetric clipping. Constant and single-group genes map to zero. Scaling with few groups can be misleading; scaled colors cannot compare absolute expression between genes.

1D Color map selects the palette for both dots and the color legend. Its settings button opens the same popup as Scatterplot: adjust the minimum/maximum, mirror the palette, use discrete steps, or edit a custom palette. Values outside the chosen range use the endpoint colors. The selected palette applies to unstandardized expression and per-gene z-scores. Changing palette settings does not recalculate expression summaries. Palette settings and the chosen range are saved in the project. The range resets to the full scale when the expression color-data range changes; dot-size and detection-display changes preserve it.

Click a plot row or dot to select its group and loaded member cells in linked ManiVault views. Ctrl-click toggles a group. Up/Down moves keyboard focus; Enter/Space selects. Selection does not filter cells or recalculate summaries. Group checkboxes change which groups are summarized. Overlapping groups are summarized independently; duplicate indices within a group are removed. Cells outside loaded subsets are omitted.

Image exports remain disabled in this build. Save CSV includes counts, means, unscaled/displayed color values and method notes. CSV writes are atomic and disabled during aggregation.

Project files retain datasets, gene/group order, visibility, search and plotting options. This view is descriptive and performs no differential-expression testing. Use violins to inspect distributions and cell embeddings to inspect individual cells.)HELP");

    QMessageBox::information(_dialogParentWidget, "Gene expression dot plot", helpText);
}
