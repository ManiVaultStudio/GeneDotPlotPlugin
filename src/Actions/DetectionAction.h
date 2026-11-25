#pragma once

#include <actions/DecimalAction.h>
#include <actions/GroupAction.h>

/** Detection cutoff and dot-visibility settings. */
class DetectionAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    DetectionAction(QObject* parent, const QString& title);
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

    mv::gui::DecimalAction& getExpressionCutoffAction() { return _expressionCutoffAction; }
    const mv::gui::DecimalAction& getExpressionCutoffAction() const { return _expressionCutoffAction; }
    mv::gui::DecimalAction& getMinimumDetectionPercentageAction() { return _minimumDetectionPercentageAction; }

signals:
    void aggregationSettingsChanged();
    void displaySettingsChanged();

private:
    mv::gui::DecimalAction _expressionCutoffAction;
    mv::gui::DecimalAction _minimumDetectionPercentageAction;
};
