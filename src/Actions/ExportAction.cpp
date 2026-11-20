#include "ExportAction.h"

#include "../GeneDotPlotWidget.h"

#include <QFileDialog>
#include <QMessageBox>

using namespace mv::gui;

ExportAction::ExportAction(QObject* parent, const QString& title) :
    GroupAction(parent, title, true),
    _pngSaveTriggerAction(this, "Save PNG"),
    _svgSaveTriggerAction(this, "Save SVG"),
    _csvSaveTriggerAction(this, "Save CSV")
{
    setConnectionPermissionsToForceNone();
    setShowLabels(false);
    addStretch();
    addAction(&_pngSaveTriggerAction, TriggerAction::Text);
    addAction(&_svgSaveTriggerAction, TriggerAction::Text);
    addAction(&_csvSaveTriggerAction, TriggerAction::Text);

    _pngSaveTriggerAction.setToolTip("PNG export is disabled in this build");
    _svgSaveTriggerAction.setToolTip("SVG export is disabled in this build");
    _csvSaveTriggerAction.setToolTip("Save all displayed gene/group summaries, including hidden dots, as CSV");

    connect(&_pngSaveTriggerAction, &TriggerAction::triggered, this, [this] { savePlot(Format::Png); });
    connect(&_svgSaveTriggerAction, &TriggerAction::triggered, this, [this] { savePlot(Format::Svg); });
    connect(&_csvSaveTriggerAction, &TriggerAction::triggered, this, [this] { savePlot(Format::Csv); });

    setExportEnabled(false);
}

void ExportAction::initialize(GeneDotPlotWidget* geneDotPlotWidget, QWidget* dialogParentWidget)
{
    _geneDotPlotWidget  = geneDotPlotWidget;
    _dialogParentWidget = dialogParentWidget;
}

void ExportAction::setExportEnabled(bool enabled)
{
    // Preserve the disabled image-export implementation in the user's source.
    _pngSaveTriggerAction.setEnabled(false);
    _svgSaveTriggerAction.setEnabled(false);
    _csvSaveTriggerAction.setEnabled(enabled);
}

void ExportAction::savePlot(Format format)
{
    if (!_geneDotPlotWidget || !_geneDotPlotWidget->hasData())
    {
        return;
    }

    QString fileExtension;
    QString fileFilter;

    switch (format)
    {
        case Format::Png:
            fileExtension = ".png";
            fileFilter    = "PNG image (*.png)";
            break;

        case Format::Svg:
            fileExtension = ".svg";
            fileFilter    = "SVG image (*.svg)";
            break;

        case Format::Csv:
            fileExtension = ".csv";
            fileFilter    = "CSV table (*.csv)";
            break;
    }

    auto outputFilePath = QFileDialog::getSaveFileName(_dialogParentWidget, "Export gene dot plot", "gene-dot-plot" + fileExtension, fileFilter);

    if (outputFilePath.isEmpty())
    {
        return;
    }

    if (!outputFilePath.endsWith(fileExtension, Qt::CaseInsensitive))
    {
        outputFilePath += fileExtension;
    }

    QString errorMessage;
    const auto exportSucceeded = format == Format::Csv ? _geneDotPlotWidget->exportCsv(outputFilePath, errorMessage) : _geneDotPlotWidget->exportPlot(outputFilePath, format == Format::Svg, errorMessage);

    if (!exportSucceeded)
    {
        QMessageBox::warning(_dialogParentWidget, "Export failed", errorMessage);
    }
}
