#include "GeneDotPlotWidget.h"

#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSaveFile>

#include <QTextStream>
#include <QToolTip>
#include <algorithm>
#include <cmath>

namespace {
QString formatNumber(double value)
{
    return std::isfinite(value) ? QString::number(value, 'g', 6) : "NA";
}

QString quoteCsvField(QString text)
{
    text.replace('"', "\"\"");
    return '"' + text + '"';
}

QColor blend(const QColor& startColor, const QColor& endColor, double interpolation)
{
    return QColor::fromRgbF(startColor.redF() + interpolation * (endColor.redF() - startColor.redF()), startColor.greenF() + interpolation * (endColor.greenF() - startColor.greenF()), startColor.blueF() + interpolation * (endColor.blueF() - startColor.blueF()));
}
} // namespace

GeneDotPlotWidget::GeneDotPlotWidget(QWidget* parent) :
    QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    _emptyPlotMessage = "Drop expression Points and compatible Clusters data here.";
    refreshLayout();
}

void GeneDotPlotWidget::setData(std::vector<DotPlotGroup> cellGroups, std::vector<DotPlotGene> geneSummaries, const QString& plotTitle, const QString& methodDescription)
{
    for (const auto& geneSummary : geneSummaries)
    {
        if (geneSummary.groupSummaries.size() != cellGroups.size())
        {
            return;
        }
    }

    _cellGroups        = std::move(cellGroups);
    _geneSummaries     = std::move(geneSummaries);
    _plotTitle         = plotTitle;
    _methodDescription = methodDescription;
    _emptyPlotMessage.clear();
    _focusedGroupIndex = std::clamp(_focusedGroupIndex, 0, std::max(0, int(_cellGroups.size()) - 1));
    refreshColors();
    refreshLayout();
    update();
}

void GeneDotPlotWidget::setOptions(bool standardizeGeneExpression, double standardDeviationClip, double minimumDetectionFraction, double maximumDotDiameter)
{
    _standardizeGeneExpression = standardizeGeneExpression;
    _standardDeviationClip     = std::clamp(standardDeviationClip, 0.5, 5.0);
    _minimumDetectionFraction  = std::clamp(minimumDetectionFraction, 0.0, 1.0);
    _maximumDotDiameter        = std::clamp(maximumDotDiameter, 8.0, 32.0);
    refreshColors();
    refreshLayout();
    update();
}

void GeneDotPlotWidget::setMessage(const QString& message)
{
    _emptyPlotMessage = message;
    _cellGroups.clear();
    _geneSummaries.clear();
    _geneColorValues.clear();
    refreshLayout();
    update();
}

void GeneDotPlotWidget::setColorMap(const QImage& colorMapImage)
{
    _colorMapImage = colorMapImage;
    update();
}

void GeneDotPlotWidget::setGenesAsRows(bool genesAsRows)
{
    if (_genesAsRows == genesAsRows)
    {
        return;
    }

    _genesAsRows = genesAsRows;
    refreshLayout();
    update();
}

void GeneDotPlotWidget::setSelectedClusters(const QSet<int>& selectedClusterIndices)
{
    _selectedClusterIndices = selectedClusterIndices;
    update();
}

void GeneDotPlotWidget::setColorMapRange(double minimumColorValue, double maximumColorValue)
{
    // Keep the last valid scale while an endpoint is temporarily equal to or
    // crosses the other endpoint during editing.
    if (!std::isfinite(minimumColorValue) || !std::isfinite(maximumColorValue) || minimumColorValue >= maximumColorValue)
    {
        return;
    }

    _minimumColorValue = minimumColorValue;
    _maximumColorValue = maximumColorValue;
    update();
}

void GeneDotPlotWidget::refreshColors()
{
    _geneColorValues.clear();
    _minimumColorValue = _standardizeGeneExpression ? -_standardDeviationClip : 0;
    _maximumColorValue = _standardizeGeneExpression ? _standardDeviationClip : 0;
    for (const auto& geneSummary : _geneSummaries)
    {
        _geneColorValues.push_back(genedot::colorValues(geneSummary.groupSummaries, _standardizeGeneExpression, _standardDeviationClip));
        if (!_standardizeGeneExpression)
        {
            for (const auto value : _geneColorValues.back())
            {
                if (std::isfinite(value))
                {
                    _minimumColorValue = std::min(_minimumColorValue, value);
                    _maximumColorValue = std::max(_maximumColorValue, value);
                }
            }
        }
    }
    if (_minimumColorValue == _maximumColorValue)
    {
        _maximumColorValue = _minimumColorValue + 1;
    }
    _minimumColorDataValue = _minimumColorValue;
    _maximumColorDataValue = _maximumColorValue;
}

QString GeneDotPlotWidget::colorLabel() const
{
    return _standardizeGeneExpression ? "Per-gene z-score (displayed groups)" : "Mean expression";
}

void GeneDotPlotWidget::refreshLayout()
{
    const QFontMetrics fontMetrics(font());
    _pointCountColumnWidth = std::max(56, fontMetrics.horizontalAdvance(QStringLiteral("numPoints")) + 8);
    for (const auto& group : _cellGroups)
    {
        _pointCountColumnWidth = std::max(_pointCountColumnWidth, fontMetrics.horizontalAdvance(QString::number(qulonglong(group.cellCount))) + 8);
    }

    _plotColumnWidth = std::max(42, int(_maximumDotDiameter) + 12);
    _plotRowHeight   = std::max(34, int(_maximumDotDiameter) + 8);
    if (_genesAsRows)
    {
        _leftMargin = std::max(145, fontMetrics.horizontalAdvance(QStringLiteral("numPoints")) + 53);
        _topMargin  = 140;
        for (const auto& geneSummary : _geneSummaries)
        {
            _leftMargin = std::max(_leftMargin, fontMetrics.horizontalAdvance(geneSummary.name) + 53);
        }
        for (const auto& group : _cellGroups)
        {
            _topMargin = std::max(_topMargin, fontMetrics.horizontalAdvance(group.name) + 115);
            _plotColumnWidth = std::max(_plotColumnWidth, fontMetrics.horizontalAdvance(QString::number(qulonglong(group.cellCount))) + 12);
        }
    }
    else
    {
        _leftMargin = std::max(145, fontMetrics.horizontalAdvance(QStringLiteral("Group")) + _pointCountColumnWidth + 53);
        _topMargin  = 90;
        for (const auto& group : _cellGroups)
        {
            _leftMargin = std::max(_leftMargin, fontMetrics.horizontalAdvance(group.name) + _pointCountColumnWidth + 53);
        }
        for (const auto& geneSummary : _geneSummaries)
        {
            _topMargin = std::max(_topMargin, fontMetrics.horizontalAdvance(geneSummary.name) + 85);
        }
    }

    _leftMargin      = std::min(_leftMargin, 420);
    _topMargin       = std::min(_topMargin, 350);
    _methodDescriptionLines.clear();
    const int wrapWidth = sizeHint().width() - 40;
    QString line;
    for (const auto& word : _methodDescription.split(' ', Qt::SkipEmptyParts))
    {
        if (!line.isEmpty() && fontMetrics.horizontalAdvance(line + ' ' + word) > wrapWidth)
        {
            _methodDescriptionLines << line;
            line.clear();
        }
        if (!line.isEmpty())
        {
            line += ' ';
        }
        line += word;
    }
    if (!line.isEmpty())
    {
        _methodDescriptionLines << line;
    }
    setMinimumSize(sizeHint());
    updateGeometry();
}

QSize GeneDotPlotWidget::sizeHint() const
{
    return QSize(std::max(760, _leftMargin + plotColumnCount() * _plotColumnWidth + 35), std::max(380, _topMargin + plotRowCount() * _plotRowHeight + 180 + int(_methodDescriptionLines.size()) * 18));
}

int GeneDotPlotWidget::plotRowCount() const
{
    return int(_genesAsRows ? _geneSummaries.size() : _cellGroups.size());
}

int GeneDotPlotWidget::plotColumnCount() const
{
    return int(_genesAsRows ? _cellGroups.size() : _geneSummaries.size());
}

QPointF GeneDotPlotWidget::dotCenter(int geneIndex, int groupIndex) const
{
    const int columnIndex = _genesAsRows ? groupIndex : geneIndex;
    const int rowIndex = _genesAsRows ? geneIndex : groupIndex;
    return QPointF(_leftMargin + (columnIndex + 0.5) * _plotColumnWidth, _topMargin + (rowIndex + 0.5) * _plotRowHeight);
}

QRectF GeneDotPlotWidget::groupBand(int groupIndex) const
{
    if (_genesAsRows)
    {
        return QRectF(_leftMargin + groupIndex * _plotColumnWidth, 55, _plotColumnWidth, _topMargin + plotRowCount() * _plotRowHeight - 55);
    }
    return QRectF(12, _topMargin + groupIndex * _plotRowHeight, sizeHint().width() - 24, _plotRowHeight);
}

QColor GeneDotPlotWidget::valueColor(double value) const
{
    const double interpolation = std::clamp((value - _minimumColorValue) / (_maximumColorValue - _minimumColorValue), 0.0, 1.0);
    if (!_colorMapImage.isNull())
    {
        const auto pixelColumn = static_cast<int>(std::lround(interpolation * (_colorMapImage.width() - 1)));
        return _colorMapImage.pixelColor(pixelColumn, _colorMapImage.height() / 2);
    }

    if (_standardizeGeneExpression)
    {
        return interpolation < .5 ? blend(QColor("#2166ac"), QColor("#f7f7f7"), interpolation * 2)
                                  : blend(QColor("#f7f7f7"), QColor("#b2182b"), (interpolation - .5) * 2);
    }
    return blend(QColor("#edf2f7"), QColor("#075985"), interpolation);
}

void GeneDotPlotWidget::draw(QPainter& painter, bool exporting) const
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(QRect(QPoint(), sizeHint()), Qt::white);
    painter.setPen(QColor("#172b3a"));
    QFont heading = font();
    heading.setBold(true);
    heading.setPointSizeF(font().pointSizeF() + 2);
    painter.setFont(heading);
    painter.drawText(QRect(20, 14, sizeHint().width() - 40, 28), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(heading).elidedText(_plotTitle.isEmpty() ? "Gene expression dot plot" : _plotTitle, Qt::ElideMiddle, sizeHint().width() - 40));
    painter.setFont(font());
    if (!hasData())
    {
        painter.drawText(QRect(30, 75, 690, 160), Qt::TextWordWrap, _emptyPlotMessage);
        return;
    }

    const QFontMetrics fontMetrics(font());
    for (int rowIndex = 0; rowIndex < plotRowCount(); ++rowIndex)
    {
        if (rowIndex % 2 == 0)
        {
            painter.fillRect(QRectF(12, _topMargin + rowIndex * _plotRowHeight, sizeHint().width() - 24, _plotRowHeight), QColor("#f6f8fa"));
        }
    }
    for (int groupIndex = 0; groupIndex < int(_cellGroups.size()); ++groupIndex)
    {
        if (_selectedClusterIndices.contains(_cellGroups[groupIndex].clusterIndex))
        {
            painter.fillRect(groupBand(groupIndex), QColor("#e0eef9"));
        }
    }

    painter.setPen(QColor("#526372"));
    const double headerBaseline = _topMargin - 10;
    if (_genesAsRows)
    {
        painter.drawText(QPointF(33, _topMargin - 58), QStringLiteral("Group"));
        painter.drawText(QPointF(33, _topMargin - 28), QStringLiteral("numPoints"));
        painter.drawText(QPointF(33, headerBaseline), QStringLiteral("Gene"));
        for (int groupIndex = 0; groupIndex < int(_cellGroups.size()); ++groupIndex)
        {
            const auto& group = _cellGroups[groupIndex];
            const double x = dotCenter(0, groupIndex).x();
            painter.save();
            painter.translate(x + 4, _topMargin - 58);
            painter.rotate(-90);
            painter.drawText(0, 0, fontMetrics.elidedText(group.name, Qt::ElideRight, _topMargin - 115));
            painter.restore();
            painter.setPen(Qt::NoPen);
            painter.setBrush(group.color);
            painter.drawRoundedRect(QRectF(x - 7, _topMargin - 47, 14, 5), 2, 2);
            painter.setPen(QColor("#64748b"));
            painter.drawText(QRectF(x - _plotColumnWidth / 2.0, _topMargin - 44, _plotColumnWidth, 24), Qt::AlignHCenter | Qt::AlignVCenter, QString::number(qulonglong(group.cellCount)));
            painter.setPen(QColor("#526372"));
        }

        painter.setPen(QColor("#172b3a"));
        for (int geneIndex = 0; geneIndex < int(_geneSummaries.size()); ++geneIndex)
        {
            const double y = dotCenter(geneIndex, 0).y();
            painter.drawText(QRectF(33, y - 12, _leftMargin - 53, 24), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics.elidedText(_geneSummaries[geneIndex].name, Qt::ElideRight, _leftMargin - 53));
        }
    }
    else
    {
        const double groupNameColumnWidth = _leftMargin - _pointCountColumnWidth - 53;
        const double pointCountColumnLeft = _leftMargin - _pointCountColumnWidth - 10;
        const auto pointCountColumnTitle = QStringLiteral("numPoints");
        painter.drawText(QPointF(33, headerBaseline), QStringLiteral("Group"));
        painter.drawText(QPointF(_leftMargin - 10 - fontMetrics.horizontalAdvance(pointCountColumnTitle), headerBaseline), pointCountColumnTitle);
        for (int geneIndex = 0; geneIndex < int(_geneSummaries.size()); ++geneIndex)
        {
            painter.save();
            painter.translate(dotCenter(geneIndex, 0).x() + 4, headerBaseline);
            painter.rotate(-90);
            painter.drawText(0, 0, fontMetrics.elidedText(_geneSummaries[geneIndex].name, Qt::ElideRight, _topMargin - 75));
            painter.restore();
        }
        for (int groupIndex = 0; groupIndex < int(_cellGroups.size()); ++groupIndex)
        {
            const auto& group = _cellGroups[groupIndex];
            const double y = dotCenter(0, groupIndex).y();
            painter.setPen(Qt::NoPen);
            painter.setBrush(group.color);
            painter.drawRoundedRect(QRectF(20, y - 7, 5, 14), 2, 2);
            painter.setPen(QColor("#172b3a"));
            painter.drawText(QRectF(33, y - 12, groupNameColumnWidth, 24), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics.elidedText(group.name, Qt::ElideRight, int(groupNameColumnWidth)));
            painter.setPen(QColor("#64748b"));
            painter.drawText(QRectF(pointCountColumnLeft, y - 12, _pointCountColumnWidth, 24), Qt::AlignRight | Qt::AlignVCenter, QString::number(qulonglong(group.cellCount)));
        }
    }

    if (!exporting && hasFocus())
    {
        painter.setPen(QPen(QColor("#2563eb"), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(groupBand(_focusedGroupIndex).adjusted(1, 1, -1, -1));
    }
    for (int geneIndex = 0; geneIndex < int(_geneSummaries.size()); ++geneIndex)
    {
        for (int groupIndex = 0; groupIndex < int(_cellGroups.size()); ++groupIndex)
        {
            const auto& summary = _geneSummaries[geneIndex].groupSummaries[groupIndex];
            const auto center   = dotCenter(geneIndex, groupIndex);
            if (!summary.validCellCount)
            {
                painter.setPen(QPen(QColor("#9ca3af"), 1));
                painter.drawLine(center + QPointF(-3, -3), center + QPointF(3, 3));
                painter.drawLine(center + QPointF(-3, 3), center + QPointF(3, -3));
            }
            else if (summary.detectionFraction() > 0 && summary.detectionFraction() >= _minimumDetectionFraction)
            {
                const double radius = genedot::dotRadius(summary.detectionFraction(), _maximumDotDiameter / 2);
                painter.setBrush(valueColor(_geneColorValues[geneIndex][groupIndex]));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(center, radius, radius);
            }
        }
    }

    const int legendY = _topMargin + plotRowCount() * _plotRowHeight + 26;
    painter.setPen(QColor("#172b3a"));
    painter.drawText(20, legendY, "Cells above threshold (%) - area proportional");
    for (int i = 1; i <= 4; ++i)
    {
        const double x = 35 + (i - 1) * 70, radius = genedot::dotRadius(i / 4.0, _maximumDotDiameter / 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#64748b"));
        painter.drawEllipse(QPointF(x, legendY + 25), radius, radius);
        painter.setPen(QColor("#172b3a"));
        painter.drawText(QRectF(x - 25, legendY + 43, 50, 20), Qt::AlignCenter, QString::number(i * 25));
    }
    painter.drawText(370, legendY, colorLabel());

    // Sample every legend column through the same lookup as the dots so all
    // palette stops, discrete colors and alpha values are represented.
    constexpr int colorLegendWidth = 280;
    QImage colorLegendImage(colorLegendWidth, 1, QImage::Format_ARGB32);
    for (int pixelColumn = 0; pixelColumn < colorLegendWidth; ++pixelColumn)
    {
        const auto expressionValue = _minimumColorValue + (_maximumColorValue - _minimumColorValue) * pixelColumn / double(colorLegendWidth - 1);
        colorLegendImage.setPixelColor(pixelColumn, 0, valueColor(expressionValue));
    }

    painter.drawImage(QRectF(370, legendY + 14, colorLegendWidth, 16), colorLegendImage);
    // Prefer five reference values, reducing the count when long labels would
    // overlap. Keep endpoint labels within the color bar's horizontal extent.
    int colorLegendIntervalCount = 4;
    QStringList colorLegendLabels;
    std::vector<double> colorLegendLabelPositions;
    while (true)
    {
        colorLegendLabels.clear();
        colorLegendLabelPositions.clear();
        const double tickSpacing = (_maximumColorValue - _minimumColorValue) / colorLegendIntervalCount;
        const double maximumAbsoluteValue = std::max(std::abs(_minimumColorValue), std::abs(_maximumColorValue));
        const int numberOfSignificantDigits = tickSpacing > 0 && maximumAbsoluteValue > 0 ? std::clamp(int(std::ceil(std::log10(maximumAbsoluteValue) - std::log10(tickSpacing))) + 2, 4, 17) : 4;
        double previousLabelRight = 0;
        bool labelsOverlap = false;
        for (int tickIndex = 0; tickIndex <= colorLegendIntervalCount; ++tickIndex)
        {
            const double fraction = tickIndex / double(colorLegendIntervalCount);
            const double value = (1 - fraction) * _minimumColorValue + fraction * _maximumColorValue;
            const QString label = QString::number(value == 0 ? 0 : value, 'g', numberOfSignificantDigits);
            const double labelWidth = fontMetrics.horizontalAdvance(label);
            const double tickX = 370 + fraction * colorLegendWidth;
            const double labelLeft = std::clamp(tickX - labelWidth / 2, 370.0, 370.0 + colorLegendWidth - labelWidth);
            labelsOverlap |= tickIndex > 0 && labelLeft < previousLabelRight + 8;
            previousLabelRight = labelLeft + labelWidth;
            colorLegendLabels << label;
            colorLegendLabelPositions.push_back(labelLeft);
        }
        if (!labelsOverlap || colorLegendIntervalCount == 1)
        {
            break;
        }
        colorLegendIntervalCount /= 2;
    }

    for (int tickIndex = 0; tickIndex <= colorLegendIntervalCount; ++tickIndex)
    {
        const double tickX = 370 + colorLegendWidth * tickIndex / double(colorLegendIntervalCount);
        painter.drawLine(QPointF(tickX, legendY + 30), QPointF(tickX, legendY + 35));
        painter.drawText(QPointF(colorLegendLabelPositions[tickIndex], legendY + 52), colorLegendLabels[tickIndex]);
    }
    painter.setPen(QColor("#526372"));
    painter.drawText(20, legendY + 88, QString("Blank: zero detection or below %1%.   x: no valid values.   Group counts: loaded cells/spots.").arg(_minimumDetectionFraction * 100, 0, 'g', 4));
    int textY = legendY + 113;
    for (const auto& line : _methodDescriptionLines)
    {
        painter.drawText(20, textY, line);
        textY += 18;
    }
}

void GeneDotPlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    draw(painter, false);
}
int GeneDotPlotWidget::groupAt(const QPointF& point) const
{
    if (!hasData())
    {
        return -1;
    }
    const double plotRight = _leftMargin + plotColumnCount() * _plotColumnWidth;
    const double plotBottom = _topMargin + plotRowCount() * _plotRowHeight;
    if (_genesAsRows)
    {
        if (point.x() < _leftMargin || point.x() >= plotRight || point.y() < 55 || point.y() >= plotBottom)
        {
            return -1;
        }
        return int((point.x() - _leftMargin) / _plotColumnWidth);
    }
    if (point.y() < _topMargin || point.y() >= plotBottom || point.x() < 12 || point.x() >= plotRight)
    {
        return -1;
    }
    return int((point.y() - _topMargin) / _plotRowHeight);
}
int GeneDotPlotWidget::geneAt(const QPointF& point) const
{
    if (!hasData())
    {
        return -1;
    }
    const double plotRight = _leftMargin + plotColumnCount() * _plotColumnWidth;
    const double plotBottom = _topMargin + plotRowCount() * _plotRowHeight;
    if (_genesAsRows)
    {
        if (point.y() < _topMargin || point.y() >= plotBottom || point.x() < 12 || point.x() >= plotRight)
        {
            return -1;
        }
        return int((point.y() - _topMargin) / _plotRowHeight);
    }
    if (point.x() < _leftMargin || point.x() >= plotRight || point.y() < 55 || point.y() >= plotBottom)
    {
        return -1;
    }
    return int((point.x() - _leftMargin) / _plotColumnWidth);
}

QString GeneDotPlotWidget::cellDescription(int geneIndex, int group) const
{
    if (geneIndex < 0 || group < 0 || geneIndex >= int(_geneSummaries.size()) || group >= int(_cellGroups.size()))
    {
        return {};
    }

    const auto& summary = _geneSummaries[geneIndex].groupSummaries[group];
    return QString("%1 / %2\n%3 / %4 valid cells above threshold (%5%)\nMean (aggregation scale): %6\nColor value: %7\nMissing: %8; rejected for input scale: %9\nClick selects the group; Ctrl-click toggles.")
        .arg(_geneSummaries[geneIndex].name, _cellGroups[group].name)
        .arg(qulonglong(summary.detectedCellCount))
        .arg(qulonglong(summary.validCellCount))
        .arg(summary.validCellCount ? formatNumber(100 * summary.detectionFraction()) : "NA", summary.validCellCount ? formatNumber(summary.meanExpression) : "NA", formatNumber(_geneColorValues[geneIndex][group]))
        .arg(qulonglong(summary.missingValueCount))
        .arg(qulonglong(summary.rejectedValueCount));
}

bool GeneDotPlotWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        auto* help         = static_cast<QHelpEvent*>(event);
        const int groupIndex = groupAt(help->pos()), geneIndex = geneAt(help->pos());
        if (groupIndex >= 0)
        {
            const QString text = geneIndex >= 0 ? cellDescription(geneIndex, groupIndex) : _cellGroups[groupIndex].name + "\nClick selects this group.";
            QToolTip::showText(help->globalPos(), text.toHtmlEscaped().replace('\n', "<br>"), this);
        }
        else if (geneIndex >= 0)
        {
            QToolTip::showText(help->globalPos(), _geneSummaries[geneIndex].name.toHtmlEscaped(), this);
        }
        else
        {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

void GeneDotPlotWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        const int groupIndex = groupAt(event->position());
        if (groupIndex >= 0)
        {
            _focusedGroupIndex = groupIndex;
            setFocus();
            update();
            emit groupActivated(_cellGroups[groupIndex].clusterIndex, event->modifiers().testFlag(Qt::ControlModifier));
        }
    }
    QWidget::mousePressEvent(event);
}

void GeneDotPlotWidget::keyPressEvent(QKeyEvent* event)
{
    if (!hasData())
    {
        return;
    }
    const int previousGroupKey = _genesAsRows ? Qt::Key_Left : Qt::Key_Up;
    const int nextGroupKey = _genesAsRows ? Qt::Key_Right : Qt::Key_Down;
    if (event->key() == previousGroupKey || event->key() == nextGroupKey)
    {
        _focusedGroupIndex = std::clamp(_focusedGroupIndex + (event->key() == previousGroupKey ? -1 : 1), 0, int(_cellGroups.size()) - 1);
        update();
        event->accept();
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space)
    {
        emit groupActivated(_cellGroups[_focusedGroupIndex].clusterIndex, event->modifiers().testFlag(Qt::ControlModifier));
        event->accept();
    }
    else
    {
        QWidget::keyPressEvent(event);
    }
}

bool GeneDotPlotWidget::exportPlot(const QString& outputFilePath, bool saveAsSvg, QString& errorMessage) const
{
    Q_UNUSED(outputFilePath)
    Q_UNUSED(saveAsSvg)
    errorMessage = "PNG/SVG export is disabled in this build.";
    return false;
}

bool GeneDotPlotWidget::exportCsv(const QString& outputFilePath, QString& errorMessage) const
{
    if (!hasData())
    {
        errorMessage = "No completed plot to export.";
        return false;
    }
    QSaveFile file(outputFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMessage = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << "gene,dimension,group,cluster,total,valid,detected,percent,mean,unscaled_color,display_color,missing,rejected,method\n";
    for (int geneIndex = 0; geneIndex < int(_geneSummaries.size()); ++geneIndex)
    {
        for (int groupIndex = 0; groupIndex < int(_cellGroups.size()); ++groupIndex)
        {
            const auto& summary = _geneSummaries[geneIndex].groupSummaries[groupIndex];
            out << quoteCsvField(_geneSummaries[geneIndex].name) << ',' << _geneSummaries[geneIndex].dimensionIndex << ',' << quoteCsvField(_cellGroups[groupIndex].name) << ',' << _cellGroups[groupIndex].clusterIndex << ','
                << qulonglong(summary.totalCellCount) << ',' << qulonglong(summary.validCellCount) << ',' << qulonglong(summary.detectedCellCount) << ','
                << (summary.validCellCount ? formatNumber(100 * summary.detectionFraction()) : "NA") << ',' << (summary.validCellCount ? formatNumber(summary.meanExpression) : "NA") << ','
                << formatNumber(summary.colorValue) << ',' << formatNumber(_geneColorValues[geneIndex][groupIndex]) << ',' << qulonglong(summary.missingValueCount) << ',' << qulonglong(summary.rejectedValueCount) << ',' << quoteCsvField(_methodDescription) << '\n';
        }
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit())
    {
        errorMessage = file.errorString();
        return false;
    }
    return true;
}
