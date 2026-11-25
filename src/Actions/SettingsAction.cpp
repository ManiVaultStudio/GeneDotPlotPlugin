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
    const auto helpText = QString::fromUtf8(R"HELP(The toolbar groups settings into Datasets (expression data, cell groups and input scale), Detection (expression cutoff and Hide dots below), and Plot (layout, dot size and coloring). Export and Help remain directly accessible.

Load expression Points data (rows = cells/spots, dimensions = genes), then optionally compatible Clusters. Dropping Clusters alone uses its Points parent. Clear Cell groups to remove grouping and summarize all loaded cells.

Ctrl/Shift-select genes or paste a marker list. Drag genes and groups to reorder. Check groups to show them. Searching hides names without changing selection.

Plot orientation switches between Horizontal (cell types/groups as rows) and Vertical (genes as rows). Group names, colors, counts and selection follow their axis. Changing orientation does not change expression summaries, dot sizes or color scales.

Dot AREA is proportional to the fraction of valid cells whose INPUT value is strictly above the detection threshold. The size scale always spans 0-100%. Mean expression includes zeros and all other valid observations. A blank is zero detection or a hidden dot; x means no valid values. Hover for exact counts and summaries.

Input/color modes:
As stored: average supplied values without transforming. For signed/centered/residual data, an above-threshold fraction is not biological detection.
Linear normalized: log1p(mean(input)).
Log1p normalized: mean(expm1(input)), then log1p for color, following Seurat-style averaging. Use ONLY when the input is already log1p-normalized with natural logarithms.
The last two modes reject negative input. Nonfinite input and back-transform overflow are excluded and reported. No library-size normalization is performed.

Per-gene z-score standardizes color across DISPLAYED groups, using sample standard deviation and symmetric clipping. Constant and single-group genes map to zero. Scaling with few groups can be misleading; scaled colors cannot compare absolute expression between genes.

1D Color map selects the palette for both dots and the color legend. Its settings button opens the same popup as Scatterplot: adjust the minimum/maximum, mirror the palette, use discrete steps, or edit a custom palette. Values outside the chosen range use the endpoint colors. The selected palette applies to unstandardized expression and per-gene z-scores. Changing palette settings does not recalculate expression summaries. Palette settings and the chosen range are saved in the project. The range resets to the full scale when the expression color-data range changes; dot-size and detection-display changes preserve it.

Click a group row, group column or dot to select its group and loaded member cells in linked ManiVault views. Ctrl-click toggles a group. Up/Down moves group focus in Horizontal orientation; Left/Right moves group focus in Vertical orientation. Enter/Space selects. Selection does not filter cells or recalculate summaries. Group checkboxes change which groups are summarized. Overlapping groups are summarized independently; duplicate indices within a group are removed. Cells outside loaded subsets are omitted.

Image exports remain disabled in this build. Save CSV includes counts, means, unscaled/displayed color values and method notes. CSV writes are atomic and disabled during aggregation.

Project files retain datasets, gene/group order, visibility, search and plotting options. This view is descriptive and performs no differential-expression testing. Use violins to inspect distributions and cell embeddings to inspect individual cells.)HELP");

    QMessageBox::information(_dialogParentWidget, "Gene expression dot plot", helpText);
}

