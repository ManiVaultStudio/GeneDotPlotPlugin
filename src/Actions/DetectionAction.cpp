#include "DetectionAction.h"

using namespace mv::gui;

DetectionAction::DetectionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _expressionCutoffAction(this, "Expression cutoff", -1e9f, 1e9f, 0.0f, 6),
    _minimumDetectionPercentageAction(this, "Hide dots below (%)", 0.0f, 100.0f, 0.0f, 1)
{
    setIconByName("filter");
    setToolTip("Detection: expression cutoff and minimum detection percentage");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);
    _expressionCutoffAction.setToolTip("Detection requires INPUT expression strictly above this value. Mean expression includes valid observations below the cutoff, including zeros.");
    _minimumDetectionPercentageAction.setToolTip("Hide dots below this detection percentage without changing summaries or the fixed 0-100% area scale");
    addAction(&_expressionCutoffAction, DecimalAction::SpinBox);
    addAction(&_minimumDetectionPercentageAction, DecimalAction::SpinBox);
    connect(&_expressionCutoffAction, &DecimalAction::valueChanged, this, &DetectionAction::aggregationSettingsChanged);
    connect(&_minimumDetectionPercentageAction, &DecimalAction::valueChanged, this, &DetectionAction::displaySettingsChanged);
}

void DetectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _expressionCutoffAction.fromParentVariantMap(variantMap);
    _minimumDetectionPercentageAction.fromParentVariantMap(variantMap);
}

QVariantMap DetectionAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();
    _expressionCutoffAction.insertIntoVariantMap(variantMap);
    _minimumDetectionPercentageAction.insertIntoVariantMap(variantMap);
    return variantMap;
}
