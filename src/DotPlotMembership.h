#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace genedot {

struct Membership
{
    std::vector<std::uint32_t> localRowIndices;
    std::size_t outsideSubsetCount  = 0;
    std::size_t duplicateIndexCount = 0;
};

// A flat chain handles repeated source rows without allocating a vector per
// cell. Full datasets and subsets both use source-global cluster indices.
class MembershipIndex
{
public:
    void reset(const std::vector<std::uint32_t>& globalCellIndices)
    {
        if (globalCellIndices.size() > invalidLocalIndex)
        {
            throw std::runtime_error("Too many cells for 32-bit indices.");
        }

        _firstLocalIndexByGlobalIndex.clear();
        _firstLocalIndexByGlobalIndex.reserve(globalCellIndices.size());
        _nextLocalIndex.assign(globalCellIndices.size(), invalidLocalIndex);

        for (std::size_t rowIndex = 0; rowIndex < globalCellIndices.size(); ++rowIndex)
        {
            const auto localRowIndex             = static_cast<std::uint32_t>(rowIndex);
            auto [globalIndexIterator, inserted] = _firstLocalIndexByGlobalIndex.emplace(globalCellIndices[rowIndex], localRowIndex);

            if (!inserted)
            {
                _nextLocalIndex[rowIndex]   = globalIndexIterator->second;
                globalIndexIterator->second = localRowIndex;
            }
        }
    }

    template <class Indices>
    Membership mapGlobalIndicesToLocalRows(const Indices& globalCellIndices) const
    {
        Membership membership;
        membership.localRowIndices.reserve(globalCellIndices.size());

        std::unordered_set<std::uint32_t> visitedGlobalIndices;
        visitedGlobalIndices.reserve(globalCellIndices.size());

        for (const auto globalCellIndex : globalCellIndices)
        {
            if (!visitedGlobalIndices.insert(globalCellIndex).second)
            {
                ++membership.duplicateIndexCount;
                continue;
            }

            const auto globalIndexIterator = _firstLocalIndexByGlobalIndex.find(globalCellIndex);

            if (globalIndexIterator == _firstLocalIndexByGlobalIndex.end())
            {
                ++membership.outsideSubsetCount;
                continue;
            }

            for (auto localRowIndex = globalIndexIterator->second; localRowIndex != invalidLocalIndex; localRowIndex = _nextLocalIndex[localRowIndex])
            {
                membership.localRowIndices.push_back(localRowIndex);
            }
        }

        return membership;
    }

private:
    static constexpr std::uint32_t invalidLocalIndex = std::numeric_limits<std::uint32_t>::max();
    std::unordered_map<std::uint32_t, std::uint32_t> _firstLocalIndexByGlobalIndex;
    std::vector<std::uint32_t> _nextLocalIndex;
};

} // namespace genedot
