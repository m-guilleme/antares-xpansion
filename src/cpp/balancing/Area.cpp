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
        const double limit = reachingPotential ? candidate.type->potential()
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
}

bool Area::isInvestmentPossible() const
{
    return std::ranges::any_of(investmentCandidates,
                               [](const auto& entry)
                               {
                                   return entry.second.installedCapacity
                                            < entry.second.type->expansionPotential
                                          && entry.second.installedCapacity
                                               >= entry.second.initInstalledCapacity;
                               });
}

bool Area::isDecommissioningPossible() const
{
    return std::ranges::any_of(decommissioningCandidates,
                               [this](const auto& entry)
                               {
                                   return entry.second.installedCapacity
                                            > entry.second.type->decommissioningPotential
                                          && entry.second.installedCapacity
                                               <= entry.second.initInstalledCapacity;
                               });
}

bool Area::isDisinvestmentPossible() const
{
    return std::ranges::any_of(investmentCandidates,
                               [](const auto& entry) {
                                   return entry.second.installedCapacity
                                          > entry.second.initInstalledCapacity;
                               });
}

bool Area::isRecommissioningPossible() const
{
    return std::ranges::any_of(decommissioningCandidates,
                               [](const auto& entry) {
                                   return entry.second.installedCapacity
                                          < entry.second.initInstalledCapacity;
                               });
}

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

void Area::updateCandidatesRentabilityPerProblem(const CapacityAction& action,
                                                 const Antares::Solver::WeeklyProblemId& pbId,
                                                 const PbOutput& pbOutput,
                                                 const std::vector<double>& solution)
{
    const bool isInvestmentAction = action == CapacityAction::INVESTMENT
                                    || action == CapacityAction::DISINVESTMENT;
    if (isInvestmentAction)
    {
        updateActionCandidatesRentabilityPerProblem(investmentCandidates, pbId, pbOutput, solution);
    }
    else
    {
        updateActionCandidatesRentabilityPerProblem(decommissioningCandidates,
                                                    pbId,
                                                    pbOutput,
                                                    solution);
    }
}

template<typename T>
void Area::updateActionCandidatesRentabilityPerProblem(
  std::map<std::string, Candidate<T>>& candidates,
  const Antares::Solver::WeeklyProblemId& pbId,
  const PbOutput& pbOutput,
  const std::vector<double>& solution)
{
    const std::array<double, NUMBER_OF_HOURS_PER_WEEK>& areaPrices = pbOutput.areaPrices.at(name);
    for (auto& [candidateName, rentability]: candidatesRentability)
    {
        Candidate<T>& candidate = candidates.at(candidateName);
        candidate.updateRentabilityWithProblem(pbId, areaPrices, solution);
    }
}

void Area::computeCandidatesRentability(const CapacityAction& action)
{
    const bool isInvestmentAction = action == CapacityAction::INVESTMENT
                                    || action == CapacityAction::DISINVESTMENT;
    if (isInvestmentAction)
    {
        computeActionCandidatesRentability(investmentCandidates);
    }
    else
    {
        computeActionCandidatesRentability(decommissioningCandidates);
    }
}

template<typename T>
void Area::computeActionCandidatesRentability(std::map<std::string, Candidate<T>>& candidates)
{
    for (auto& [candidateName, rentability]: candidatesRentability)
    {
        Candidate<T>& candidate = candidates.at(candidateName);
        rentability += candidate.computeRentability();
    }
}

static bool shouldSelectMaxRentability(const CapacityAction& action)
{
    return action == CapacityAction::INVESTMENT || action == CapacityAction::RECOMMISSIONING;
}

std::string Area::selectCandidateWithBestRentability(const CapacityAction& action)
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

double Area::lowerThreshold() const
{
    return reliabilityStandard - reliabilityStandardDeadBandDown;
}

double Area::higherThreshold() const
{
    return reliabilityStandard + reliabilityStandardDeadBandUp;
}

void Area::checkDecomCandidatesTypeAgainstDecomCandidateCapacities() const
{
    for (const auto& [candidateName, candidate]: decommissioningCandidates)
    {
        if (candidate.type->decommissioningPotential < 0)
        {
            throw std::runtime_error("Decommissioning potential is negative");
        }
    }
}
