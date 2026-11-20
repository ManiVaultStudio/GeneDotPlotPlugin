#pragma once

#include <actions/GroupAction.h>
#include <actions/TriggerAction.h>

class GeneDotPlotWidget;

/** Actions for exporting the completed chart or its aggregated values. */
class ExportAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    enum class Format { Png,
                        Svg,
                        Csv };

    ExportAction(QObject* parent, const QString& title);
    void initialize(GeneDotPlotWidget* geneDotPlotWidget, QWidget* dialogParentWidget);
    void setExportEnabled(bool enabled);

    mv::gui::TriggerAction& getPngSaveTriggerAction() { return _pngSaveTriggerAction; }
    mv::gui::TriggerAction& getSvgSaveTriggerAction() { return _svgSaveTriggerAction; }
    mv::gui::TriggerAction& getCsvSaveTriggerAction() { return _csvSaveTriggerAction; }

private:
    void savePlot(Format format);

    GeneDotPlotWidget* _geneDotPlotWidget = nullptr;
    QWidget* _dialogParentWidget          = nullptr;
    mv::gui::TriggerAction _pngSaveTriggerAction;
    mv::gui::TriggerAction _svgSaveTriggerAction;
    mv::gui::TriggerAction _csvSaveTriggerAction;
};
