#pragma once

#include "ColoringAction.h"

#include <actions/DecimalAction.h>
#include <actions/GroupAction.h>
#include <actions/OptionAction.h>

/** Plot layout, dot size and coloring settings. */
class PlotAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    PlotAction(QObject* parent, const QString& title);
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

    mv::gui::OptionAction& getPlotOrientationOptionAction() { return _plotOrientationOptionAction; }
    mv::gui::DecimalAction& getMaximumDotDiameterAction() { return _maximumDotDiameterAction; }
    ColoringAction& getColoringAction() { return _coloringAction; }
    const ColoringAction& getColoringAction() const { return _coloringAction; }

signals:
    void displaySettingsChanged();

private:
    mv::gui::GroupAction _layoutGroupAction;
    mv::gui::GroupAction _dotSizeGroupAction;
    mv::gui::OptionAction _plotOrientationOptionAction;
    mv::gui::DecimalAction _maximumDotDiameterAction;
    ColoringAction _coloringAction;
};
