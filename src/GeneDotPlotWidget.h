#pragma once

#include "DotPlotStatistics.h"
#include <QColor>
#include <QImage>
#include <QSet>
#include <QStringList>
#include <QWidget>
#include <vector>

class QPainter;

struct DotPlotGroup
{
    QString name;
    QColor color;
    int clusterIndex      = -1;
    std::size_t cellCount = 0;
};

struct DotPlotGene
{
    QString name;
    int dimensionIndex = -1;
    std::vector<genedot::Summary> groupSummaries;
};

class GeneDotPlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GeneDotPlotWidget(QWidget* parent = nullptr);
    void setData(std::vector<DotPlotGroup> cellGroups, std::vector<DotPlotGene> geneSummaries, const QString& plotTitle, const QString& methodDescription);
    void setOptions(bool standardizeGeneExpression, double standardDeviationClip, double minimumDetectionFraction, double maximumDotDiameter);
    void setColorMap(const QImage& colorMapImage);
    void setSelectedClusters(const QSet<int>& selectedClusterIndices);
    void setMessage(const QString& message);
    bool exportPlot(const QString& outputFilePath, bool saveAsSvg, QString& errorMessage) const;
    bool exportCsv(const QString& outputFilePath, QString& errorMessage) const;
    QSize sizeHint() const override;
    QPointF dotCenter(int geneIndex, int groupIndex) const;
    QString cellDescription(int geneIndex, int groupIndex) const;
    bool hasData() const { return !_geneSummaries.empty() && !_cellGroups.empty(); }

signals:
    void groupActivated(int clusterIndex, bool toggleSelection);

protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void draw(QPainter& painter, bool exporting) const;
    void refreshLayout();
    void refreshColors();
    int groupAt(const QPointF& point) const;
    int geneAt(const QPointF& point) const;
    QColor valueColor(double value) const;
    QString colorLabel() const;
    std::vector<DotPlotGroup> _cellGroups;
    std::vector<DotPlotGene> _geneSummaries;
    std::vector<std::vector<double>> _geneColorValues;
    QImage _colorMapImage;
    QSet<int> _selectedClusterIndices;
    QString _plotTitle;
    QString _methodDescription;
    QString _emptyPlotMessage;
    QStringList _methodDescriptionLines;
    bool _standardizeGeneExpression  = false;
    double _standardDeviationClip    = 2.5;
    double _minimumDetectionFraction = 0.0;
    double _maximumDotDiameter       = 22.0;
    double _minimumColorValue        = 0.0;
    double _maximumColorValue        = 1.0;
    int _leftMargin                  = 180;
    int _topMargin                   = 220;
    int _geneColumnWidth             = 42;
    int _groupRowHeight              = 34;
    int _focusedGroupRow             = 0;
};
