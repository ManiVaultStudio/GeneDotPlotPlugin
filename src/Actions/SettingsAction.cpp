#include "SettingsAction.h"

#include <QLabel>
#include <QMessageBox>

using namespace mv::gui;

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _datasetsAction(this, "Datasets"),
    _detectionAction(this, "Detection"),
    _plotAction(this, "Plot"),
    _markerSelectionAction(this, "Marker selection"),
    _exportAction(this, "Export"),
    _helpTriggerAction(this, "Help"),
    _statusMessageAction(this, "Status"),
    _primaryToolbarAction(this, "Primary toolbar")
{
    setSerializationName("GeneDotPlot:Settings");
    setConnectionPermissionsToForceNone();
    _exportAction.setIconByName("download");
    _exportAction.setToolTip("Export: save the displayed gene/group summaries");
    _exportAction.setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    _helpTriggerAction.setIconByName("circle-question");
    _helpTriggerAction.setToolTip("Help: gene dot plot settings and interpretation");
    _primaryToolbarAction.addAction(&_datasetsAction);
    _primaryToolbarAction.addAction(&_detectionAction);
    _primaryToolbarAction.addAction(&_plotAction);
    _primaryToolbarAction.addAction(&_exportAction);
    _primaryToolbarAction.addAction(&_helpTriggerAction, 100, TriggerAction::Icon);

    addAction(&_datasetsAction);
    addAction(&_detectionAction);
    addAction(&_plotAction);
    addAction(&_markerSelectionAction);
    addAction(&_exportAction);
    addAction(&_helpTriggerAction, TriggerAction::Icon);
    addAction(&_statusMessageAction, StringAction::Label);
    _statusMessageAction.setWidgetConfigurationFunction([](WidgetAction*, QWidget* widget) {
        if (auto* statusLabel = qobject_cast<QLabel*>(widget))
        {
            statusLabel->setWordWrap(true);
            statusLabel->setTextFormat(Qt::PlainText);
        }
    });
    connect(&_datasetsAction, &DatasetsAction::aggregationSettingsChanged, this, &SettingsAction::aggregationSettingsChanged);
    connect(&_detectionAction, &DetectionAction::aggregationSettingsChanged, this, &SettingsAction::aggregationSettingsChanged);
    connect(&_detectionAction, &DetectionAction::displaySettingsChanged, this, &SettingsAction::displaySettingsChanged);
    connect(&_plotAction, &PlotAction::displaySettingsChanged, this, &SettingsAction::displaySettingsChanged);
    connect(&_helpTriggerAction, &TriggerAction::triggered, this, &SettingsAction::showHelp);
}

QWidget* SettingsAction::getWidget(QWidget* parent, const std::int32_t& widgetFlags)
{
    Q_UNUSED(widgetFlags)
    _dialogParentWidget = parent;
    return _primaryToolbarAction.createWidget(parent);
}

void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _datasetsAction.fromParentVariantMap(variantMap);
    _detectionAction.fromParentVariantMap(variantMap);
    _plotAction.fromParentVariantMap(variantMap);
    _markerSelectionAction.fromParentVariantMap(variantMap);
}

QVariantMap SettingsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();
    _datasetsAction.insertIntoVariantMap(variantMap);
    _detectionAction.insertIntoVariantMap(variantMap);
    _plotAction.insertIntoVariantMap(variantMap);
    _markerSelectionAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

void SettingsAction::showHelp()
{
    QMessageBox::information(_dialogParentWidget, "Gene expression dot plot",
        "Load expression data (rows = cells, dimensions = genes), then optionally Clusters. "
        "Clear Groups to remove grouping and summarize all loaded cells.\n\n"

        "Ctrl/Shift-select genes or paste a marker list. Drag genes and groups to reorder. "
        "Check groups to show them. Searching hides names without changing selection.\n\n"

        "Dot area is proportional to the fraction of valid cells whose input value is strictly above the detection threshold. "
        "The size scale always spans 0-100%. Mean expression includes zeros and all other valid observations. "
        "A blank is zero detection or a hidden dot; x means no valid values. Hover for exact counts and summaries.\n\n"

        "Input/color modes:\n"
        "Mean: average input values without transforming.\n"
        "Linear normalized: log1p(mean(input)).\n"
        "Log1p normalized: mean(expm1(input)), then log1p for color, following Seurat-style averaging. "
        "Use ONLY when the input is already log1p-normalized with natural logarithms.\n"
        "The last two modes reject negative input. Nonfinite input and back-transform overflow are excluded and reported. "
        "No library-size normalization is performed.\n\n"

        "Per-gene z-score standardizes color across displayed groups, using sample standard deviation and symmetric clipping. "
        "Constant and single-group genes map to zero.\n\n"

        "Click a group row, group column or dot to select its group and loaded member cells in linked ManiVault views. "
        "Ctrl-click toggles a group. Up/Down moves group focus in Horizontal orientation; "
        "Left/Right moves group focus in Vertical orientation. Enter/Space selects."
    );
}

