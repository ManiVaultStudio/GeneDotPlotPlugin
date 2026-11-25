#include "ColoringAction.h"

#include <QSignalBlocker>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace mv;
using namespace mv::gui;

ColoringAction::ColoringAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _colorMap1DAction(this, "1D Color map", "Reds"),
    _standardizeExpressionToggleAction(this, "Per-gene z-score", false),
    _standardDeviationClipAction(this, "Z-score clipping", 0.5f, 5.0f, 2.5f, 1)
{
    setLabelSizingType(LabelSizingType::Auto);
    _standardDeviationClipAction.setEnabled(false);
    _colorMap1DAction.setDefaultWidgetFlags(ColorMapAction::Default);
    _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X).setEnabled(false);
    _colorMap1DAction.setToolTip("Choose the palette for dots and the color legend. Open the settings button to adjust its range, reverse the palette, or use discrete colors.");
    _standardizeExpressionToggleAction.setToolTip("Standardize color across displayed groups for each gene. Constant/single-group genes map to zero. Scaled colors cannot compare absolute expression between genes.");
    _standardDeviationClipAction.setToolTip("Symmetric clipping of per-gene z-scores, in standard deviations");
    addAction(&_colorMap1DAction, ColorMapAction::Default);
    addAction(&_standardizeExpressionToggleAction, ToggleAction::CheckBox);
    addAction(&_standardDeviationClipAction, DecimalAction::SpinBox);
    connect(&_standardizeExpressionToggleAction, &ToggleAction::toggled, this, [this](bool checked) {
        _standardDeviationClipAction.setEnabled(checked);
        emit displaySettingsChanged();
    });
    connect(&_standardDeviationClipAction, &DecimalAction::valueChanged, this, &ColoringAction::displaySettingsChanged);
    connect(&_colorMap1DAction.getRangeAction(ColorMapAction::Axis::X), &DecimalRangeAction::rangeChanged, this, [this](const util::NumericalRange<float>& range) {
        emit colorMapRangeChanged(range.getMinimum(), range.getMaximum());
    });
}

void ColoringAction::updateColorMapDataRange(double minimumColorDataValue, double maximumColorDataValue)
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
            const auto restoredMinimum = std::clamp(_restoredMinimumColorValue, minimumDataValue, maximumDataValue);
            const auto restoredMaximum = std::clamp(_restoredMaximumColorValue, minimumDataValue, maximumDataValue);
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

void ColoringAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _colorMap1DAction.fromParentVariantMap(variantMap);
    _standardizeExpressionToggleAction.fromParentVariantMap(variantMap);
    _standardDeviationClipAction.fromParentVariantMap(variantMap);
    _hasColorMapDataRange = false;
    _preserveRestoredColorMapRange = variantMap.value("Color map range initialized", false).toBool();
    // Private decimal actions restore their value within the current limits.
    // Keep the endpoints until aggregation supplies the expression-data limits.
    _restoredMinimumColorValue = variantMap.value("Color map minimum", 0.0f).toFloat();
    _restoredMaximumColorValue = variantMap.value("Color map maximum", 1.0f).toFloat();
    _standardDeviationClipAction.setEnabled(_standardizeExpressionToggleAction.isChecked());
}

QVariantMap ColoringAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();
    _colorMap1DAction.insertIntoVariantMap(variantMap);
    _standardizeExpressionToggleAction.insertIntoVariantMap(variantMap);
    _standardDeviationClipAction.insertIntoVariantMap(variantMap);
    variantMap["Color map range initialized"] = _hasColorMapDataRange || _preserveRestoredColorMapRange;
    const auto& rangeAction = _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X);
    variantMap["Color map minimum"] = _preserveRestoredColorMapRange ? _restoredMinimumColorValue : rangeAction.getMinimum();
    variantMap["Color map maximum"] = _preserveRestoredColorMapRange ? _restoredMaximumColorValue : rangeAction.getMaximum();
    return variantMap;
}
