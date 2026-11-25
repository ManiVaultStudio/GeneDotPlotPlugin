#pragma once

#include "DatasetsAction.h"
#include "DetectionAction.h"
#include "ExportAction.h"
#include "MarkerSelectionAction.h"
#include "PlotAction.h"

#include <actions/GroupAction.h>
#include <actions/HorizontalToolbarAction.h>
#include <actions/StringAction.h>
#include <actions/TriggerAction.h>

#include <QPointer>

/** Groups plugin actions into a compact toolbar, following Scatterplot. */
class SettingsAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    SettingsAction(QObject* parent, const QString& title);
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

    DatasetsAction& getDatasetsAction() { return _datasetsAction; }
    const DatasetsAction& getDatasetsAction() const { return _datasetsAction; }
    DetectionAction& getDetectionAction() { return _detectionAction; }
    const DetectionAction& getDetectionAction() const { return _detectionAction; }
    PlotAction& getPlotAction() { return _plotAction; }
    const PlotAction& getPlotAction() const { return _plotAction; }
    MarkerSelectionAction& getMarkerSelectionAction() { return _markerSelectionAction; }
    ExportAction& getExportAction() { return _exportAction; }
    mv::gui::StringAction& getStatusMessageAction() { return _statusMessageAction; }

signals:
    void aggregationSettingsChanged();
    void displaySettingsChanged();

protected:
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override;

private:
    void showHelp();

    DatasetsAction _datasetsAction;
    DetectionAction _detectionAction;
    PlotAction _plotAction;
    MarkerSelectionAction _markerSelectionAction;
    ExportAction _exportAction;
    mv::gui::TriggerAction _helpTriggerAction;
    mv::gui::StringAction _statusMessageAction;
    mv::gui::HorizontalToolbarAction _primaryToolbarAction;
    QPointer<QWidget> _dialogParentWidget;
};
