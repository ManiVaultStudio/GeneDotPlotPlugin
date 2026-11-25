#include "PlotAction.h"

using namespace mv::gui;

PlotAction::PlotAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _layoutGroupAction(this, "Layout"),
    _dotSizeGroupAction(this, "Dot size"),
    _plotOrientationOptionAction(this, "Plot orientation", {"Horizontal (cell types as rows)", "Vertical (genes as rows)"}),
    _maximumDotDiameterAction(this, "Maximum diameter (px)", 8.0f, 32.0f, 22.0f, 0),
    _coloringAction(this, "Coloring")
{
    setIconByName("sliders");
    setToolTip("Plot: layout, dot size and coloring");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);
    _layoutGroupAction.setShowLabels(false);
    _dotSizeGroupAction.setLabelSizingType(LabelSizingType::Auto);
    _plotOrientationOptionAction.setCurrentIndex(0);
    _plotOrientationOptionAction.setToolTip("Horizontal: cell types/groups are rows and genes are columns. Vertical: genes are rows and cell types/groups are columns.");
    _maximumDotDiameterAction.setToolTip("Diameter in pixels of a dot with 100% detection");
    _layoutGroupAction.addAction(&_plotOrientationOptionAction);
    _dotSizeGroupAction.addAction(&_maximumDotDiameterAction, DecimalAction::SpinBox);
    addAction(&_layoutGroupAction, GroupAction::Vertical | GroupAction::NoMargins);
    addAction(&_dotSizeGroupAction, GroupAction::Vertical | GroupAction::NoMargins);
    addAction(&_coloringAction, GroupAction::Vertical | GroupAction::NoMargins);
    connect(&_plotOrientationOptionAction, &OptionAction::currentIndexChanged, this, &PlotAction::displaySettingsChanged);
    connect(&_maximumDotDiameterAction, &DecimalAction::valueChanged, this, &PlotAction::displaySettingsChanged);
    connect(&_coloringAction, &ColoringAction::displaySettingsChanged, this, &PlotAction::displaySettingsChanged);
}

void PlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _plotOrientationOptionAction.fromParentVariantMap(variantMap);
    _maximumDotDiameterAction.fromParentVariantMap(variantMap);
    _coloringAction.fromParentVariantMap(variantMap);
}

QVariantMap PlotAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();
    _plotOrientationOptionAction.insertIntoVariantMap(variantMap);
    _maximumDotDiameterAction.insertIntoVariantMap(variantMap);
    _coloringAction.insertIntoVariantMap(variantMap);
    return variantMap;
}
