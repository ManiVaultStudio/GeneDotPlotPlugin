#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace genedot {

// The user supplies the expression scale. Detection always uses input values;
// this view never performs library-size normalization.
enum class InputScale { AsStored,
                        LinearLogColor,
                        Log1pNormalized };

struct Summary
{
    std::size_t totalCellCount     = 0;
    std::size_t validCellCount     = 0;
    std::size_t detectedCellCount  = 0;
    std::size_t missingValueCount  = 0;
    std::size_t rejectedValueCount = 0;
    double meanExpression          = 0.0;
    double colorValue              = std::numeric_limits<double>::quiet_NaN();

    double detectionFraction() const { return validCellCount ? double(detectedCellCount) / double(validCellCount) : 0.0; }
};

class Accumulator
{
public:
    void addObservation(double inputValue, InputScale inputScale, double detectionThreshold)
    {
        ++_summary.totalCellCount;

        if (!std::isfinite(inputValue))
        {
            ++_summary.missingValueCount;
            return;
        }

        if (inputScale != InputScale::AsStored && inputValue < 0.0)
        {
            ++_summary.rejectedValueCount;
            return;
        }

        const auto expressionValue = inputScale == InputScale::Log1pNormalized ? std::expm1(inputValue) : inputValue;

        if (!std::isfinite(expressionValue))
        {
            ++_summary.rejectedValueCount;
            return;
        }

        ++_summary.validCellCount;

        if (inputValue > detectionThreshold)
        {
            ++_summary.detectedCellCount;
        }

        // A weighted mean avoids overflowing a sum or a large difference.
        const auto observationWeight = 1.0 / double(_summary.validCellCount);
        _summary.meanExpression      = _summary.meanExpression * (1.0 - observationWeight) + expressionValue * observationWeight;
    }

    Summary getSummary(InputScale inputScale) const
    {
        auto summary = _summary;

        if (summary.validCellCount)
        {
            summary.colorValue = inputScale == InputScale::AsStored ? summary.meanExpression : std::log1p(summary.meanExpression);
        }

        return summary;
    }

private:
    Summary _summary;
};

// Sample z-scores across displayed groups, separately for each gene. Missing
// groups remain missing; constant and single-valid-group genes map to zero.
inline std::vector<double> colorValues(const std::vector<Summary>& groupSummaries, bool standardizeExpression, double standardDeviationClip)
{
    std::vector<double> colorValues;
    double meanColorValue        = 0.0;
    double sumSquaredDifferences = 0.0;
    std::size_t validGroupCount  = 0;

    for (const auto& groupSummary : groupSummaries)
    {
        colorValues.push_back(groupSummary.colorValue);

        if (!std::isfinite(groupSummary.colorValue))
        {
            continue;
        }

        const auto differenceFromMean = groupSummary.colorValue - meanColorValue;
        meanColorValue += differenceFromMean / double(++validGroupCount);
        sumSquaredDifferences += differenceFromMean * (groupSummary.colorValue - meanColorValue);
    }

    if (!standardizeExpression)
    {
        return colorValues;
    }

    const auto standardDeviation = validGroupCount > 1 ? std::sqrt(std::max(0.0, sumSquaredDifferences / double(validGroupCount - 1))) : 0.0;

    for (auto& colorValue : colorValues)
    {
        if (std::isfinite(colorValue))
        {
            colorValue = standardDeviation > 0.0 ? std::clamp((colorValue - meanColorValue) / standardDeviation, -standardDeviationClip, standardDeviationClip) : 0.0;
        }
    }

    return colorValues;
}

inline double dotRadius(double detectionFraction, double maximumRadius)
{
    return maximumRadius * std::sqrt(std::clamp(detectionFraction, 0.0, 1.0));
}

} // namespace genedot
