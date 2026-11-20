#pragma once

#include "ExportAction.h"
#include "MarkerSelectionAction.h"

#include <actions/DatasetPickerAction.h>
#include <actions/DecimalAction.h>
#include <actions/GroupAction.h>
#include <actions/OptionAction.h>
#include <actions/StringAction.h>
#include <actions/ToggleAction.h>
#include <actions/TriggerAction.h>

#include <QPointer>

/** Persistent ManiVault actions for dataset loading and dot-plot settings. */
class SettingsAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    SettingsAction(QObject* parent, const QString& title);
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;
    void restoreLegacySettings(const QVariantMap& pluginVariantMap);

    mv::gui::DatasetPickerAction& getPointDatasetPickerAction() { return _pointDatasetPickerAction; }
    mv::gui::DatasetPickerAction& getClusterDatasetPickerAction() { return _clusterDatasetPickerAction; }
    mv::gui::OptionAction& getInputScaleOptionAction() { return _inputScaleOptionAction; }
    mv::gui::DecimalAction& getDetectionThresholdAction() { return _detectionThresholdAction; }
    mv::gui::ToggleAction& getStandardizeExpressionToggleAction() { return _standardizeExpressionToggleAction; }
    mv::gui::DecimalAction& getStandardDeviationClipAction() { return _standardDeviationClipAction; }
    mv::gui::DecimalAction& getMinimumDetectionPercentageAction() { return _minimumDetectionPercentageAction; }
    mv::gui::DecimalAction& getMaximumDotDiameterAction() { return _maximumDotDiameterAction; }
    MarkerSelectionAction& getMarkerSelectionAction() { return _markerSelectionAction; }
    ExportAction& getExportAction() { return _exportAction; }
    mv::gui::StringAction& getStatusMessageAction() { return _statusMessageAction; }

    const mv::gui::OptionAction& getInputScaleOptionAction() const { return _inputScaleOptionAction; }
    const mv::gui::DecimalAction& getDetectionThresholdAction() const { return _detectionThresholdAction; }
    const mv::gui::ToggleAction& getStandardizeExpressionToggleAction() const { return _standardizeExpressionToggleAction; }
    const mv::gui::DecimalAction& getStandardDeviationClipAction() const { return _standardDeviationClipAction; }

signals:
    void aggregationSettingsChanged();
    void displaySettingsChanged();
    void allCellsRequested();

protected:
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override;

private:
    void showHelp();

    mv::gui::DatasetPickerAction _pointDatasetPickerAction;
    mv::gui::DatasetPickerAction _clusterDatasetPickerAction;
    mv::gui::OptionAction _inputScaleOptionAction;
    mv::gui::DecimalAction _detectionThresholdAction;
    mv::gui::ToggleAction _standardizeExpressionToggleAction;
    mv::gui::DecimalAction _standardDeviationClipAction;
    mv::gui::DecimalAction _minimumDetectionPercentageAction;
    mv::gui::DecimalAction _maximumDotDiameterAction;
    mv::gui::TriggerAction _allCellsTriggerAction;
    mv::gui::TriggerAction _helpTriggerAction;
    MarkerSelectionAction _markerSelectionAction;
    ExportAction _exportAction;
    mv::gui::StringAction _statusMessageAction;
    QPointer<QWidget> _dialogParentWidget;
};
