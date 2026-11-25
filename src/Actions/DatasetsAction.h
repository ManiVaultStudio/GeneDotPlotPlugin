#pragma once

#include <actions/DatasetPickerAction.h>
#include <actions/GroupAction.h>
#include <actions/OptionAction.h>

/** Expression data, grouping and input-scale settings. */
class DatasetsAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    DatasetsAction(QObject* parent, const QString& title);
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

    mv::gui::DatasetPickerAction& getPointDatasetPickerAction() { return _pointDatasetPickerAction; }
    mv::gui::DatasetPickerAction& getClusterDatasetPickerAction() { return _clusterDatasetPickerAction; }
    mv::gui::OptionAction& getInputScaleOptionAction() { return _inputScaleOptionAction; }
    const mv::gui::OptionAction& getInputScaleOptionAction() const { return _inputScaleOptionAction; }

signals:
    void aggregationSettingsChanged();

private:
    mv::gui::DatasetPickerAction _pointDatasetPickerAction;
    mv::gui::DatasetPickerAction _clusterDatasetPickerAction;
    mv::gui::OptionAction _inputScaleOptionAction;
};
