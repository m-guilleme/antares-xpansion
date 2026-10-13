#include "include/antares-xpansion/balancing/Area.h"

template<typename T>
void Area::initializeRentabilityImpl(const std::map<std::string, Candidate<T>>& candidates,
                                     const CapacityAction& action)
{
    candidatesRentability.clear();
    for (const auto& [candidateName, candidate]: candidates)
    {
        const bool reachingPotential = action == CapacityAction::INVESTMENT
                                       || action == CapacityAction::DECOMMISSIONING;
        const double limit = reachingPotential ? candidate.type->maxCapacity()
                                               : candidate.initInstalledCapacity;
        if (candidate.installedCapacity == limit)
        {
            continue;
        }
        candidatesRentability[candidateName] -= extraCost(candidate);
    }
}

void Area::initializeInvestmentRentability(const CapacityAction& action)
{
    initializeRentabilityImpl(investmentCandidates, action);
}

void Area::initializeDecommissioningRentability(const CapacityAction& action)
{
    initializeRentabilityImpl(decommissioningCandidates, action);
}

void Area::initializeRentability(const CapacityAction& action)
{
    if (action == CapacityAction::INVESTMENT || action == CapacityAction::DISINVESTMENT)
    {
        initializeInvestmentRentability(action);
    }
    else
    {
        initializeDecommissioningRentability(action);
    }
}

Candidate<InvestmentCandidateType>& Area::getInvestmentCandidate(const std::string& candidateName)
{
    return investmentCandidates.at(candidateName);
}

Candidate<DecommissioningCandidateType>& Area::getDecommissioningCandidate(
  const std::string& candidateName)
{
    return decommissioningCandidates.at(candidateName);
} /// @brief Check if investment is possible for the area

/// @return true if investment is possible, false otherwise
bool Area::isInvestmentPossible() const
{
    return std::ranges::any_of(investmentCandidates,
                               [](const auto& entry) {
                                   return entry.second.installedCapacity
                                          < entry.second.type->expansionPotential;
                               });
}

/// @brief Check if decommissioning is possible for the area
/// @return true if decommissioning is possible, false otherwise
bool Area::isDecommissioningPossible() const
{
    return std::ranges::any_of(decommissioningCandidates,
                               [](const auto& entry) {
                                   return entry.second.installedCapacity
                                          > entry.second.type->decommissioningPotential;
                               });
}

/// @brief Check if disinvestment is possible for the area
/// @return true if disinvestment is possible, false otherwise
bool Area::isDisinvestmentPossible() const
{
    return std::ranges::any_of(investmentCandidates,
                               [](const auto& entry) {
                                   return entry.second.installedCapacity
                                          > entry.second.initInstalledCapacity;
                               });
}

/// @brief Check if recommissioning is possible for the area
/// @return true if recommissioning is possible, false otherwise
bool Area::isRecommissioningPossible() const
{
    return std::ranges::any_of(decommissioningCandidates,
                               [](const auto& entry) {
                                   return entry.second.installedCapacity
                                          < entry.second.initInstalledCapacity;
                               });
}

/// @brief Check if an area reachs max oscillation through one of their candidate
/// @return true if the area has reached mas oscillation, false otherwise
bool Area::maxOscillationReached() const
{
    bool maxOscillationReached = false;
    for (const auto& [candidateName, candidate]: investmentCandidates)
    {
        if (candidate.oscillationCounter >= maxOscillation)
        {
            maxOscillationReached = true;
            continue;
        }
    }
    for (const auto& [candidateName, candidate]: decommissioningCandidates)
    {
        if (candidate.oscillationCounter >= maxOscillation)
        {
            maxOscillationReached = true;
            continue;
        }
    }
    return maxOscillationReached;
}

template<typename T>
double Area::extraCost(const Candidate<T>& candidate)
{
    return candidate.installedCapacity * candidate.type->unitCost();
}

/// @brief Find the best candidate for a given area
/// @param action The action to apply for which the best candidate is looked for
/// @param pbOutput AvgCriteria and rice of the problem
/// @param solution Solution of the problem
void Area::updateRentabilityWithProblem(const CapacityAction action,
                                        const PbOutput pbOutput,
                                        const std::vector<double> solution)
{
    const bool isInvestmentAction = action == CapacityAction::INVESTMENT
                                    || action == CapacityAction::DISINVESTMENT;
    if (isInvestmentAction)
    {
        computeRentabilityForCandidates(investmentCandidates, action, pbOutput, solution);
    }
    else
    {
        computeRentabilityForCandidates(decommissioningCandidates, action, pbOutput, solution);
    }
}

template<typename T>
void Area::computeRentabilityForCandidates(const std::map<std::string, Candidate<T>>& candidates,
                                           const CapacityAction action,
                                           const PbOutput pbOutput,
                                           const std::vector<double> solution)
{
    for (auto& [candidateName, rentability]: candidatesRentability)
    {
        double production(0.0);
        Candidate<T> candidate = candidates.at(candidateName);
        double marginalCost = candidate.marginalCost;
        const auto& dispProdVarIndices = candidate.dispProdVarIndices;

        for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
        {
            production = solution.at(dispProdVarIndices.at(hour));
            rentability += (pbOutput.areaPrices.at(name).at(hour) - marginalCost) * production;
        }
    }
}

static bool shouldSelectMaxRentability(CapacityAction action)
{
    return action == CapacityAction::INVESTMENT || action == CapacityAction::RECOMMISSIONING;
}

std::string Area::selectCandidateWithBestRentability(CapacityAction action)
{
    const auto valueOf = [](const auto& entry) { return entry.second; };
    const auto best = shouldSelectMaxRentability(action)
                        ? std::ranges::max_element(candidatesRentability, {}, valueOf)
                        : std::ranges::min_element(candidatesRentability, {}, valueOf);
    return best->first;
}

bool Area::isCandidatesRentabilityEmpty() const
{
    return candidatesRentability.empty();
}

std::map<std::string, double> Area::getCandidatesRentability() const
{
    return candidatesRentability;
}
