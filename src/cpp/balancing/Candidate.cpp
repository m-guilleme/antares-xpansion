#include "include/antares-xpansion/balancing/Area.h"

void CandidateBase::setOneWeekBoundsData(Antares::Solver::WeeklyProblemId pbId,
                                         std::vector<BoundData> oneWeekBoundsData)
{
    boundsData[pbId] = oneWeekBoundsData;
}

// Sequential: create all map nodes up front so the parallel phase never modifies the map structure
void CandidateBase::prepareBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds)
{
    for (const auto& pbId: pbIds)
    {
        boundsData.try_emplace(pbId);
    }
}

// Thread-safe as long as each pbId is handled by exactly one thread
void CandidateBase::computeAndSetOneWeekCapacityData(Antares::Solver::WeeklyProblemId pbId,
                                                     const std::shared_ptr<Problem>& problem)
{
    std::vector<BoundData> oneWeekBoundsData(NUMBER_OF_HOURS_PER_WEEK);
    for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
    {
        double upperBound;
        double lowerBound;
        problem->get_ub(&upperBound, dispProdVarIndices[hour], dispProdVarIndices[hour]);
        problem->get_lb(&lowerBound, dispProdVarIndices[hour], dispProdVarIndices[hour]);
        oneWeekBoundsData[hour].lowBoundRatioToUpBound = upperBound > 0.0 ? lowerBound / upperBound
                                                                          : 0.0;
        oneWeekBoundsData[hour].upBoundRatioToInstalledCap = upperBound;
    }
    boundsData.at(pbId) = std::move(oneWeekBoundsData); // at(): lookup only, no insertion
}

// Sequential: same result as the old in-loop updates
void CandidateBase::updateCapacitiesFromBounds()
{
    double maxUpperBound = installedCapacity;
    for (const auto& [pbId, oneWeekBoundsData]: boundsData)
    {
        for (const auto& hourlyBoundsData: oneWeekBoundsData)
        {
            maxUpperBound = std::max(maxUpperBound, hourlyBoundsData.upBoundRatioToInstalledCap);
        }
    }

    installedCapacity = maxUpperBound;
    previousInstalledCapacity = maxUpperBound;
    initInstalledCapacity = maxUpperBound;
    for (auto& [pbId, oneWeekBoundData]: boundsData)
    {
        for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
        {
            oneWeekBoundData[hour].upBoundRatioToInstalledCap = oneWeekBoundData[hour]
                                                                      .upBoundRatioToInstalledCap
                                                                    == installedCapacity
                                                                  ? 1.
                                                                : installedCapacity > 0.0
                                                                  ? oneWeekBoundData[hour]
                                                                        .upBoundRatioToInstalledCap
                                                                      / installedCapacity
                                                                  : 0.0;
        }
    }
}

void CandidateBase::updateOscillationStatus(const CapacityAction& action)
{
    if (lastAction.has_value() && action != lastAction)
    {
        oscillationCounter += 1;
    }
    lastAction = action;
}
