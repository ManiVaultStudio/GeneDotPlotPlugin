#pragma once

#include <actions/ColorMap1DAction.h>
#include <actions/DecimalAction.h>
#include <actions/GroupAction.h>
#include <actions/ToggleAction.h>

/** Expression-color scaling, palette and adjustable palette range. */
class ColoringAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    ColoringAction(QObject* parent, const QString& title);
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;
    void updateColorMapDataRange(double minimumColorDataValue, double maximumColorDataValue);

    mv::gui::ColorMap1DAction& getColorMap1DAction() { return _colorMap1DAction; }
    mv::gui::ToggleAction& getStandardizeExpressionToggleAction() { return _standardizeExpressionToggleAction; }
    const mv::gui::ToggleAction& getStandardizeExpressionToggleAction() const { return _standardizeExpressionToggleAction; }
    mv::gui::DecimalAction& getStandardDeviationClipAction() { return _standardDeviationClipAction; }
    const mv::gui::DecimalAction& getStandardDeviationClipAction() const { return _standardDeviationClipAction; }

signals:
    void displaySettingsChanged();
    void colorMapRangeChanged(double minimumColorValue, double maximumColorValue);

private:
    mv::gui::ColorMap1DAction _colorMap1DAction;
    mv::gui::ToggleAction _standardizeExpressionToggleAction;
    mv::gui::DecimalAction _standardDeviationClipAction;
    bool _hasColorMapDataRange = false;
    bool _preserveRestoredColorMapRange = false;
    float _restoredMinimumColorValue = 0.0f;
    float _restoredMaximumColorValue = 1.0f;
};
