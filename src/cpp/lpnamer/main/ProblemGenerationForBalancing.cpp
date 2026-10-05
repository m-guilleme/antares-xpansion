
#include "antares-xpansion/lpnamer/main/ProblemGenerationForBalancing.h"

#include <iostream>
#include <numeric>
#include <tbb/parallel_for_each.h>
#include <utility>

#include <antares/api/solver.h>
#include <antares/solver/lps/LpsFromAntares.h>

/// @brief Launch the simulation and save the problems
/// @param directories The directories to use for the problems generation
/// @param areas The area investments to use for the problems modification
/// @param logger The logger to use
/// @param solverName The name of the solver to use
ProblemGenerationForBalancing::ProblemGenerationForBalancing(
  ConfigurationManager::ConfigDirectories directories,
  std::map<std::string, Area>& areas,
  Logger logger,
  std::shared_ptr<ProblemManager> problemManager,
  std::filesystem::path iterationsLogFileName):
    ProblemGenerationOptimSimu(directories, logger, problemManager),
    areas(areas),
    iterationsLogFileName(iterationsLogFileName)
{
    logger->display_message("Get candidates indices and marginal cost",
                            LogUtils::LOGLEVEL::INFO,
                            PROBLEM_GENERATION_LOGGER_CONTEXT);
    fillDispProdVarIndicesAndMarginalCosts();
    logger->display_message("Get candidate bounds data",
                            LogUtils::LOGLEVEL::INFO,
                            PROBLEM_GENERATION_LOGGER_CONTEXT);
    setCapacitiesDataForCandidates();
    logger->display_message("Initialize logs",
                            LogUtils::LOGLEVEL::INFO,
                            PROBLEM_GENERATION_LOGGER_CONTEXT);
    initializeOscillationRecords();
    initializeIterativeLogCSV();
}

/// @brief Fill the DispatchableProduction variable indices and marginal cost for a given area
/// @param areaName The name of the area to process
/// @param candidateName The name of the cluster candidate to process
/// @param varToIndex A map from variable names to their indices
/// @param objCoeffs The objective coefficients for each variable
template<typename T>
void ProblemGenerationForBalancing::fillDispProdVarIndicesAndMarginalCostsForArea(
  const std::string& areaName,
  const std::string& candidateName,
  Candidate<T>& candidate,
  const std::unordered_map<std::string, size_t>& varToIndex,
  const std::vector<double>& objCoeffs)
{
    const AreaCandidate key{areaName, candidateName};

    for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
    {
        const std::string varName = "dispatchableproduction::area<" + areaName
                                    + ">::thermalcluster<" + candidateName + ">::hour<"
                                    + std::to_string(hour) + ">";

        const auto it = varToIndex.find(varName);
        if (it != varToIndex.end())
        {
            candidate.dispProdVarIndices[hour] = it->second;

            if (hour == 0)
            {
                candidate.marginalCost = objCoeffs[it->second];
            }
        }
        else
        {
            throw std::runtime_error("Failed to find for candidate " + candidateName + " of area "
                                     + areaName + " the dispProdVarIndices of hour "
                                     + std::to_string(hour));
        }
    }
}

template<typename T>
void ProblemGenerationForBalancing::setCapacityDataForOneCandidate(const std::string& areaName,
                                                                   const std::string& candidateName,
                                                                   Candidate<T>& candidate)
{
    const auto& dispProdVarIndices = candidate.dispProdVarIndices;
    double installedCapacity = 0.0;
    for (const auto& pbId: problemManager->getProblemIds())
    {
        std::shared_ptr<Problem> problem = problemManager->getProblemFromId(pbId);
        std::vector<BoundData> oneWeekBoundsData(NUMBER_OF_HOURS_PER_WEEK);
        for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
        {
            double upperBound;
            double lowerBound;
            problem->get_ub(&upperBound, dispProdVarIndices[hour], dispProdVarIndices[hour]);
            problem->get_lb(&lowerBound, dispProdVarIndices[hour], dispProdVarIndices[hour]);
            oneWeekBoundsData[hour].lowBoundRatioToUpBound = upperBound > 0.0
                                                               ? lowerBound / upperBound
                                                               : 0.0;
            // if both bound are equal to 0.0 we set as upperonly
            oneWeekBoundsData[hour].upBoundRatioToInstalledCap = upperBound;
            if (upperBound > installedCapacity)
            {
                installedCapacity = upperBound;
            }
        }
        candidate.setOneWeekBoundsData(pbId, oneWeekBoundsData);
    };
    for (auto& [pbId, oneWeekBoundData]: candidate.boundsData)
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
    candidate.installedCapacity = installedCapacity;
    candidate.previousInstalledCapacity = installedCapacity;
    candidate.initInstalledCapacity = installedCapacity;
}

void ProblemGenerationForBalancing::setCapacitiesDataForCandidates()
{
    for (auto& [areaName, areaSetting]: areas)
    {
        for (auto& [candidateName, candidate]: areaSetting.investmentCandidates)
        {
            setCapacityDataForOneCandidate(areaName, candidateName, candidate);
        }

        for (auto& [candidateName, candidate]: areaSetting.decommissioningCandidates)
        {
            setCapacityDataForOneCandidate(areaName, candidateName, candidate);
        }
    }
}

static std::unordered_map<std::string, size_t> buildVarToIndex(const std::vector<std::string>& vars)
{
    std::unordered_map<std::string, size_t> index;
    index.reserve(vars.size());
    for (size_t i = 0; i < vars.size(); ++i)
    {
        // convert the source string to lower case
        std::string lowerString;
        lowerString.resize(vars[i].size());
        std::transform(vars[i].begin(),
                       vars[i].end(),
                       lowerString.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        // add pair
        index.emplace(lowerString, i);
    }
    return index;
}

/// @brief Update the problems for the balancing calculation
void ProblemGenerationForBalancing::fillDispProdVarIndicesAndMarginalCosts()
{
    const auto& firstProblem = problemManager->getFirstProblem();
    auto vars = firstProblem->get_col_names();
    for (auto& s: vars)
    {
        s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1); // remove whitespaces
    }

    size_t nbVars = vars.size();
    std::vector<double> objCoeffs(nbVars);
    firstProblem->get_obj(objCoeffs.data(), 0, nbVars - 1);

    const auto varToIndex = buildVarToIndex(vars);

    for (auto& [areaName, area]: areas)
    {
        for (auto& [candidateName, candidate]: area.investmentCandidates)
        {
            fillDispProdVarIndicesAndMarginalCostsForArea(areaName,
                                                          candidateName,
                                                          candidate,
                                                          varToIndex,
                                                          objCoeffs);
        }
        for (auto& [candidateName, candidate]: area.decommissioningCandidates)
        {
            fillDispProdVarIndicesAndMarginalCostsForArea(areaName,
                                                          candidateName,
                                                          candidate,
                                                          varToIndex,
                                                          objCoeffs);
        }
    }
}

void ProblemGenerationForBalancing::logCriterionAndAreaSettings(
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues) const
{
    // For each area, log the criterion state and the DispatchableProduction variable values for the
    // cluster candidates of the area
    for (const auto& [areaName, area]: areas)
    {
        std::stringstream ss;
        ss << "\n  Criterion state for area " << areaName << ": " << to_string(area.criterionState)
           << " | max oscillation reached : " << std::boolalpha << maxOscillationReached(areaName)
           << "\n";
        ss << "  Average criteria value: " << area.avgCriteria;
        if (area.criterionState != CriterionState::VALID)
        {
            double threshold;
            if (area.criterionState == CriterionState::HIGHER)
            {
                ss << " (which is above the threshold of " << higherThreshold(area) << ")";
            }
            else
            {
                ss << " (which is below the threshold of " << lowerThreshold(area) << ")";
            }
        }
        ss << "\n";

        for (const auto& [candidateName, investmentCandidate]: area.investmentCandidates)
        {
            ss << "  Invested capacity for cluster candidate " << candidateName << ": "
               << investmentCandidate.installedCapacity
               << " | oscillation : " << oscillationRecords.at({areaName, candidateName}).first
               << "\n";
        }
        for (const auto& [candidateName, decommissioningCandidate]: area.decommissioningCandidates)
        {
            ss << "  Decommissioned capacity for cluster candidate " << candidateName << ": "
               << decommissioningCandidate.installedCapacity
               << " | oscillation : " << oscillationRecords.at({areaName, candidateName}).first
               << "\n";
        }

        logger->display_message(ss.str(),
                                LogUtils::LOGLEVEL::INFO,
                                PROBLEM_GENERATION_LOGGER_CONTEXT);
    }
}

void ProblemGenerationForBalancing::initializeIterativeLogCSV() const
{
    std::ofstream file(iterationsLogFileName);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open iterative log file for writing: "
                                 + iterationsLogFileName.string());
    }
    // header
    file << "iteration,zone,criteria,action,cluster candidate,capacity change\n";
}

void ProblemGenerationForBalancing::saveCriterionAndAreaSettingsToIterativeLogCSV(
  int iteration) const
{
    std::ofstream file(iterationsLogFileName, std::ios_base::app);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open iterative log file for writing: "
                                 + iterationsLogFileName.string());
    }
    std::string action;
    auto writeLineToFile = [&]<typename T>(const std::string& areaName,
                                           const std::string& candidateName,
                                           const CriterionState& criterionState,
                                           const Candidate<T>& candidate)
    {
        // only writing the line if capacity has been modified, i.e. an action has been
        // performed
        if (candidate.installedCapacity != candidate.previousInstalledCapacity)
        {
            file << iteration << "," << areaName << "," << to_string(criterionState) << ","
                 << to_string(areas.at(areaName).lastAction) << "," << candidateName << ","
                 << candidate.installedCapacity - candidate.previousInstalledCapacity << "\n";
        }
    };

    for (const auto& [areaName, area]: areas)
    {
        for (const auto& [candidateName, investmentCandidate]: area.investmentCandidates)
        {
            writeLineToFile(areaName, candidateName, area.criterionState, investmentCandidate);
        }
        for (const auto& [candidateName, decommissioningCandidate]: area.decommissioningCandidates)
        {
            writeLineToFile(areaName, candidateName, area.criterionState, decommissioningCandidate);
        }
    }
}

void ProblemGenerationForBalancing::saveClusterResultsToCSV(
  const std::filesystem::path& outputPath) const
{
    std::ofstream file(outputPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file for writing: " + outputPath.string());
    }

    file << "area name,candidate name,capacity,capacity change\n";

    for (const auto& [areaName, area]: areas)
    {
        for (const auto& [candidateName, investmentCandidate]: area.investmentCandidates)
        {
            file << areaName << "," << candidateName << "," << investmentCandidate.installedCapacity
                 << ","
                 << investmentCandidate.installedCapacity
                      - investmentCandidate.initInstalledCapacity
                 << "\n";
        }
        for (const auto& [candidateName, decommissioningCandidate]: area.decommissioningCandidates)
        {
            file << areaName << "," << candidateName << ","
                 << decommissioningCandidate.installedCapacity << ","
                 << decommissioningCandidate.installedCapacity
                      - decommissioningCandidate.initInstalledCapacity
                 << "\n";
        }
    }
}

void ProblemGenerationForBalancing::saveCriterionAndAreaSettingsToCSV(
  const std::filesystem::path& outputPath) const
{
    std::ofstream file(outputPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file for writing: " + outputPath.string());
    }

    file << "area,criteria "
            "value,criteria target,criteria lower bound,criteria upper bound,status,total capacity "
            "change\n";

    for (const auto& [areaName, area]: areas)
    {
        const double criteriaValue = area.avgCriteria;
        std::string status = (criteriaValue < lowerThreshold(area))    ? "LOWER"
                             : (criteriaValue > higherThreshold(area)) ? "HIGHER"
                                                                       : "NO ACTION";
        double totalCapacityChange(0.0);
        for (const auto& investmentCandidate: area.investmentCandidates | std::views::values)
        {
            totalCapacityChange += investmentCandidate.installedCapacity
                                   - investmentCandidate.initInstalledCapacity;
        }
        for (const auto& decommissioningCandidate:
             area.decommissioningCandidates | std::views::values)
        {
            totalCapacityChange += decommissioningCandidate.installedCapacity
                                   - decommissioningCandidate.initInstalledCapacity;
        }
        file << areaName << "," << criteriaValue << "," << area.reliabilityStandard << ","
             << lowerThreshold(area) << "," << higherThreshold(area) << "," << status << ","
             << totalCapacityChange << "\n";
    }
}

/// @brief Find the action to apply for each area candidates
/// @param simuValues The simulation values to use for the problems modification
/// @return A map associating selected area candidate to their action
std::map<AreaCandidate, CapacityAction> ProblemGenerationForBalancing::findAreaCandidatesToModify(
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues)
{
    std::map<AreaCandidate, CapacityAction> areaCandidatesToModify;
    updateAreasIncrement();

    for (const auto& [areaName, area]: areas)
    {
        const CriterionState areaCriterionState = area.criterionState;

        if (areaCriterionState == CriterionState::VALID)
        {
            continue;
        }
        std::optional<CapacityAction> action = determineCapacityAction(areaName,
                                                                       areaCriterionState,
                                                                       area);
        // if no action possible, no candidate will be modified
        if (action.has_value())
        {
            const std::string candidateName = getBestCandidate(simuValues,
                                                               areaName,
                                                               area,
                                                               action.value());
            areaCandidatesToModify[{areaName, candidateName}] = action.value();
        }
    }

    updateOldCriterionState();
    return areaCandidatesToModify;
}

/// @brief Find the action to apply from the criterion states and area investment parameters
/// @param areaName The name of the area
/// @param currentState The current criterion state
/// @param area The area investment parameters
/// @return The action to apply
std::optional<CapacityAction> ProblemGenerationForBalancing::determineCapacityAction(
  const std::string& areaName,
  CriterionState currentState,
  const Area& area) const
{
    std::optional<CapacityAction> lastAction;
    lastAction = area.lastAction;

    const bool isHigher = currentState == CriterionState::HIGHER;
    // Investment cycle if the previous action was investment or disinvestment, or if it's the first
    // iteration and the criterion is higher than the target
    const bool isInvestmentCycle = lastAction == CapacityAction::INVESTMENT
                                   || lastAction == CapacityAction::DISINVESTMENT
                                   || (!lastAction.has_value() && isHigher);

    if (maxOscillationReached(areaName))
    {
        // if no action is possible: logging a warning and carrying on
        std::ostringstream oss;
        oss << "No action in area " << areaName << " because max oscillation has been reached";
        logger->display_message(oss.str(),
                                LogUtils::LOGLEVEL::INFO,
                                PROBLEM_GENERATION_LOGGER_CONTEXT);
        return std::nullopt;
    }

    if (isInvestmentCycle && isHigher)
    {
        if (area.isInvestmentPossible())
        {
            return CapacityAction::INVESTMENT;
        }
        if (area.isRecommissioningPossible())
        {
            return CapacityAction::RECOMMISSIONING;
        }
    }
    else if (isInvestmentCycle && !isHigher)
    {
        if (area.isDisinvestmentPossible())
        {
            return CapacityAction::DISINVESTMENT;
        }
        if (area.isDecommissioningPossible())
        {
            return CapacityAction::DECOMMISSIONING;
        }
    }
    else if (isHigher)
    {
        if (area.isRecommissioningPossible())
        {
            return CapacityAction::RECOMMISSIONING;
        }
        if (area.isInvestmentPossible())
        {
            return CapacityAction::INVESTMENT;
        }
    }
    else
    {
        if (area.isDecommissioningPossible())
        {
            return CapacityAction::DECOMMISSIONING;
        }
        if (area.isDisinvestmentPossible())
        {
            return CapacityAction::DISINVESTMENT;
        }
    }

    // if no action is possible: logging a warning and carrying on
    std::ostringstream oss;
    oss << "Area " << areaName << " is not balanced but no modification is possible\n"
        << " Current criterion state: " << to_string(currentState) << "\n"
        << " Previous action: " << (lastAction.has_value() ? to_string(lastAction.value()) : "None")
        << "\n";
    logger->display_message(oss.str(),
                            LogUtils::LOGLEVEL::WARNING,
                            PROBLEM_GENERATION_LOGGER_CONTEXT);
    return std::nullopt;
}

template<typename T>
static double extraCost(const Candidate<T>& candidate)
{
    if constexpr (std::is_same_v<T, InvestmentCandidateType>)
    {
        return candidate.installedCapacity
               * (candidate.type->investmentCost + candidate.type->fixedOmCosts);
    }
    else
    {
        return candidate.installedCapacity
               * (candidate.type->decommissioningCost + candidate.type->fixedOmCosts);
    }
}

template<typename T>
std::map<std::string, double> ProblemGenerationForBalancing::computeRentabilityForCandidates(
  const std::string& areaName,
  const std::map<std::string, Candidate<T>>& candidates,
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues,
  CapacityAction action) const
{
    std::map<std::string, double> rentability;
    for (const auto& [candidateName, candidate]: candidates)
    {
        double value = 0.0;
        if constexpr (std::is_same_v<T, InvestmentCandidateType>)
        {
            if (action == CapacityAction::INVESTMENT
                && candidate.installedCapacity == candidate.type->expansionPotential)
            {
                continue;
            }
            else if (action == CapacityAction::DISINVESTMENT
                     && candidate.installedCapacity == candidate.initInstalledCapacity)
            {
                continue;
            }
        }
        else
        {
            if (action == CapacityAction::DECOMMISSIONING
                && candidate.installedCapacity == candidate.type->decommissioningPotential)
            {
                continue;
            }
            else if (action == CapacityAction::RECOMMISSIONING
                     && candidate.installedCapacity == candidate.initInstalledCapacity)
            {
                continue;
            }
        }
        double production(0.0);
        double marginalCost = candidate.marginalCost;
        for (const auto& [pbId, pbOutput]: simuValues)
        {
            const auto& dispProdVarIndices = candidate.dispProdVarIndices;
            // production is fetched from values resulting of the optimization
            std::shared_ptr<Problem> problem = problemManager->getProblemFromId(pbId);
            auto solution = problemManager->getProblemSolution(pbId, problem);

            for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
            {
                production = solution.at(dispProdVarIndices.at(hour));
                value += (pbOutput.areaPrices.at(areaName).at(hour) - marginalCost) * production;
            }
        }
        value -= extraCost(candidate);
        rentability[candidateName] = value;
    }
    return rentability;
}

static bool shouldSelectMaxRentability(CapacityAction action)
{
    return action == CapacityAction::INVESTMENT || action == CapacityAction::RECOMMISSIONING;
}

static std::string selectBestClusterFromRentability(
  const std::map<std::string, double>& rentability,
  CapacityAction action)
{
    const auto valueOf = [](const auto& entry) { return entry.second; };
    const auto best = shouldSelectMaxRentability(action)
                        ? std::ranges::max_element(rentability, {}, valueOf)
                        : std::ranges::min_element(rentability, {}, valueOf);
    return best->first;
}

/// @brief Find the best candidate for a given area
/// @param simuValues The simulation values to look for the cluster candidate selection
/// @param areaName The name of the area to find the best candidate for
/// @param area The area investment parameters
/// @param action The action to apply for which the best candidate is looked for
/// @return The name of the best candidate for the given area and action
std::string ProblemGenerationForBalancing::getBestCandidate(
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues,
  const std::string& areaName,
  const Area& area,
  CapacityAction action) const
{
    const bool isInvestmentAction = action == CapacityAction::INVESTMENT
                                    || action == CapacityAction::DISINVESTMENT;

    std::map<std::string, double> rentability;
    if (isInvestmentAction)
    {
        rentability = computeRentabilityForCandidates(areaName,
                                                      area.investmentCandidates,
                                                      simuValues,
                                                      action);
    }
    else
    {
        rentability = computeRentabilityForCandidates(areaName,
                                                      area.decommissioningCandidates,
                                                      simuValues,
                                                      action);
    }

    return selectBestClusterFromRentability(rentability, action);
}

/// @brief Update the the area investment increments based on the criterion states
/// @param areaCritData The criterion data containing states to use for the update
void ProblemGenerationForBalancing::updateAreasIncrement()
{
    for (auto& [areaName, area]: areas)
    {
        if (area.oldCriterionState != area.criterionState
            && area.oldCriterionState != CriterionState::UNINITIALIZED
            && area.criterionState != CriterionState::VALID)
        {
            area.currentInvestmentIncrement = std::max(area.investmentIncrement * 0.1,
                                                       area.currentInvestmentIncrement
                                                         - 0.1 * area.investmentIncrement);
            area.currentDecommissioningIncrement = std::max(area.decommissioningIncrement * 0.1,
                                                            area.currentDecommissioningIncrement
                                                              - 0.1
                                                                  * area.decommissioningIncrement);
        }
    }
}

/// @brief Update the old criterion states with the current ones
/// @param areaCritState The current criterion states to set as old criterion states
void ProblemGenerationForBalancing::updateOldCriterionState()
{
    for (auto& [areaName, area]: areas)
    {
        area.oldCriterionState = area.criterionState;
    }
}

/// @brief Compute the criterion state from the area investment parameters and the criterion
/// value
/// @param area The area investment parameters to use for the computation
/// @param value The criterion value to use for the computation
/// @return The criterion state computed
CriterionState ProblemGenerationForBalancing::computeCriterionState(const Area& area,
                                                                    double value) const
{
    if (value < lowerThreshold(area))
    {
        return CriterionState::LOWER;
    }
    else if (value > higherThreshold(area))
    {
        return CriterionState::HIGHER;
    }
    else
    {
        return CriterionState::VALID;
    }
}

/// @brief Compute the average area criteria values from the simulation values
/// @param simuValues The simulation values to compute the average from
/// @return The average area criteria values
std::map<std::string, double> computeAverageAreaCriteriaValues(
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues)
{
    std::set<unsigned int> years;
    std::map<std::string, double> areasAvgCriteria;

    for (const auto& [id, output]: simuValues)
    {
        years.insert(id.year);
        for (const auto& [area, value]: output.areaCriterionValues)
        {
            areasAvgCriteria[area] += value;
        }
    }

    const double numYears = static_cast<double>(years.size());
    for (auto& sum: areasAvgCriteria | std::views::values)
    {
        sum /= numYears;
    }

    return areasAvgCriteria;
}

/// @brief Update current area criteria data
void ProblemGenerationForBalancing::updateAreaCriteriaData(
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues)
{
    const auto areasAvgCriteria = computeAverageAreaCriteriaValues(simuValues);
    for (auto& [areaName, area]: areas)
    {
        area.avgCriteria = areasAvgCriteria.at(areaName);
        area.criterionState = computeCriterionState(area, areasAvgCriteria.at(areaName));
    };
}

/// @brief Compute the new candidate's installed capacity
/// @param action The action to apply
/// @param area The area investment data to update
/// @param candidateName The name of the cluster candidate to update
void ProblemGenerationForBalancing::computeCandidateInstalledCapacity(
  CapacityAction action,
  Area& area,
  const std::string& candidateName)
{
    switch (action)
    {
    case CapacityAction::INVESTMENT:
    {
        auto& candidate = area.getInvestmentCandidate(candidateName);
        candidate.installedCapacity = std::min(candidate.installedCapacity
                                                 + area.currentInvestmentIncrement,
                                               candidate.type->expansionPotential);
        break;
    }
    case CapacityAction::DISINVESTMENT:
    {
        auto& candidate = area.getInvestmentCandidate(candidateName);
        candidate.installedCapacity = std::max(candidate.installedCapacity
                                                 - area.currentInvestmentIncrement,
                                               candidate.initInstalledCapacity);
        break;
    }
    case CapacityAction::DECOMMISSIONING:
    {
        auto& candidate = area.getDecommissioningCandidate(candidateName);
        candidate.installedCapacity = std::max(candidate.installedCapacity
                                                 - area.currentDecommissioningIncrement,
                                               candidate.type->decommissioningPotential);
        break;
    }
    case CapacityAction::RECOMMISSIONING:
    {
        auto& candidate = area.getDecommissioningCandidate(candidateName);
        candidate.installedCapacity = std::min(candidate.installedCapacity
                                                 + area.currentDecommissioningIncrement,
                                               candidate.initInstalledCapacity);
        break;
    }
    }
}

/// @brief Apply the action for each area candidate to the problems
/// @param areaCandidate The area candidate to apply the action to
/// @param action The action to apply
void ProblemGenerationForBalancing::applyActionToCluster(const AreaCandidate& areaCandidate,
                                                         CapacityAction action)
{
    std::array<size_t, NUMBER_OF_HOURS_PER_WEEK> varIndices;

    auto& area = areas.at(areaCandidate.first);
    area.lastAction = action;
    computeCandidateInstalledCapacity(action, area, areaCandidate.second);
    double installedCapacity;
    if (action == CapacityAction::INVESTMENT || action == CapacityAction::DISINVESTMENT)
    {
        installedCapacity = area.getInvestmentCandidate(areaCandidate.second).installedCapacity;
        varIndices = area.getInvestmentCandidate(areaCandidate.second).dispProdVarIndices;
    }
    else
    {
        installedCapacity = area.getDecommissioningCandidate(areaCandidate.second)
                              .installedCapacity;
        varIndices = area.getDecommissioningCandidate(areaCandidate.second).dispProdVarIndices;
    }
    std::vector<int> vecIndices(varIndices.begin(), varIndices.end());

    tbb::parallel_for_each(
      problemManager->getProblemIds(),
      [&](const auto& pbId)
      {
          std::vector<BoundData> oneWeekBoundsDataCandidate;
          switch (action)
          {
          case CapacityAction::INVESTMENT:
          case CapacityAction::DISINVESTMENT:
              oneWeekBoundsDataCandidate = area.getInvestmentCandidate(areaCandidate.second)
                                             .boundsData[pbId];
              break;
          case CapacityAction::DECOMMISSIONING:
          case CapacityAction::RECOMMISSIONING:
              oneWeekBoundsDataCandidate = area.getDecommissioningCandidate(areaCandidate.second)
                                             .boundsData[pbId];
              break;
          }
          std::shared_ptr<Problem> problem = problemManager->getProblemFromId(pbId);
          std::vector<char> vecUpperChar(NUMBER_OF_HOURS_PER_WEEK, 'U');
          std::vector<char> vecLowerChar(NUMBER_OF_HOURS_PER_WEEK, 'L');
          std::vector<double> upperBoundsValue(NUMBER_OF_HOURS_PER_WEEK);
          std::vector<double> lowerBoundsValue(NUMBER_OF_HOURS_PER_WEEK);
          for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
          {
              double upperValue = oneWeekBoundsDataCandidate[hour].upBoundRatioToInstalledCap
                                  * installedCapacity;
              upperBoundsValue[hour] = upperValue;
              lowerBoundsValue[hour] = oneWeekBoundsDataCandidate[hour].lowBoundRatioToUpBound
                                       * upperValue;
          }
          problem->chg_bounds(vecIndices, vecUpperChar, upperBoundsValue);
          problem->chg_bounds(vecIndices, vecLowerChar, lowerBoundsValue);
          problemManager->setProblem(pbId, problem);
      });
}

double getCandidateCurrentCapacity(std::map<std::string, Area>& areas,
                                   const CapacityAction& action,
                                   const AreaCandidate& areaCandidate)
{
    double candidateCurrentCapacity;
    switch (action)
    {
    case CapacityAction::INVESTMENT:
    case CapacityAction::DISINVESTMENT:
        candidateCurrentCapacity = areas.at(areaCandidate.first)
                                     .getInvestmentCandidate(areaCandidate.second)
                                     .installedCapacity;
        break;
    case CapacityAction::DECOMMISSIONING:
    case CapacityAction::RECOMMISSIONING:
        candidateCurrentCapacity = areas.at(areaCandidate.first)
                                     .getDecommissioningCandidate(areaCandidate.second)
                                     .installedCapacity;
        break;
    }
    return candidateCurrentCapacity;
}

/// @brief Update the problems using the balancing algorithm
/// @param simuValues The simulation values to use for the problems modification
/// @return The updated problems
std::shared_ptr<ProblemManager> ProblemGenerationForBalancing::updateProblems(
  const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues)
{
    // For the first iteration, simuValues is empty and no modification should be applied
    if (simuValues.empty())
    {
        return problemManager;
    }

    // updating previous capacity
    for (auto& [areaName, area]: areas)
    {
        for (auto& candidate: area.investmentCandidates | std::views::values)
        {
            candidate.previousInstalledCapacity = candidate.installedCapacity;
        }
        for (auto& candidate: area.decommissioningCandidates | std::views::values)
        {
            candidate.previousInstalledCapacity = candidate.installedCapacity;
        }
    }

    logger->display_message("Find areas candidate to modify",
                            LogUtils::LOGLEVEL::INFO,
                            PROBLEM_GENERATION_LOGGER_CONTEXT);
    const auto& areaCandidatesToModify = findAreaCandidatesToModify(simuValues);
    // If no action available on all areas then the run is stopped
    if (areaCandidatesToModify.empty())
    {
        logger->display_message(
          (std::stringstream() << "No actions found in any area, stop the process").str(),
          LogUtils::LOGLEVEL::INFO,
          PROBLEM_GENERATION_LOGGER_CONTEXT);
        blocked = true;
    }
    logger->display_message("Apply action",
                            LogUtils::LOGLEVEL::INFO,
                            PROBLEM_GENERATION_LOGGER_CONTEXT);
    for (const auto& [areaCandidate, action]: areaCandidatesToModify)
    {
        double previousCandidateCapacity = getCandidateCurrentCapacity(areas,
                                                                       action,
                                                                       areaCandidate);
        applyActionToCluster(areaCandidate, action);
        updateRecords(areaCandidate, action);
        double newCandidateCapacity = getCandidateCurrentCapacity(areas, action, areaCandidate);
        logger->display_message((std::stringstream()
                                 << " Area: " << areaCandidate.first
                                 << " criteria: " << areas.at(areaCandidate.first).avgCriteria
                                 << " [" << lowerThreshold(areas.at(areaCandidate.first)) << "-"
                                 << higherThreshold(areas.at(areaCandidate.first)) << "]"
                                 << " action: " << to_string(action)
                                 << " cluster: " << areaCandidate.second
                                 << " new capacity: " << newCandidateCapacity << " delta: "
                                 << (newCandidateCapacity - previousCandidateCapacity))
                                  .str(),
                                LogUtils::LOGLEVEL::INFO,
                                PROBLEM_GENERATION_LOGGER_CONTEXT);
    }
    return problemManager;
}

/// @brief Check if the system is balanced from the simulation values
/// @param simuValues The simulation values to use for the computation
/// @return true if the system is balanced, false otherwise
bool ProblemGenerationForBalancing::isBalanced() const
{
    return std::ranges::all_of(areas | std::views::values,
                               [](const Area& area)
                               { return area.criterionState == CriterionState::VALID; });
}

/// @brief Check if the system can perform any action
/// @return true if the system is blocked, false otherwise
bool ProblemGenerationForBalancing::isBlocked() const
{
    return blocked;
}

/// @brief Intialize oscillation records
void ProblemGenerationForBalancing::initializeOscillationRecords()
{
    for (const auto& [areaName, areaSetting]: areas)
    {
        for (const auto& [candidateName, investmentCandidate]: areaSetting.investmentCandidates)
        {
            oscillationRecords[{areaName, candidateName}] = {0, std::nullopt};
        }
        for (const auto& [candidateName, decommissioningCandidate]:
             areaSetting.decommissioningCandidates)
        {
            oscillationRecords[{areaName, candidateName}] = {0, std::nullopt};
        }
    }
}

/// @brief Update records for an area candidate
/// @param areaCandidate Area candidate name to update
/// @param action Action apply to the area candidate
void ProblemGenerationForBalancing::updateRecords(const AreaCandidate& areaCandidate,
                                                  CapacityAction action)
{
    auto& oscillationStatus = oscillationRecords[areaCandidate];
    if (oscillationStatus.second.has_value() && action != oscillationStatus.second)
    {
        oscillationStatus.first += 1;
    }
    oscillationStatus.second = action;
}

/// @brief Check if an area reachs max oscillation through one of their candidate
/// @param areaName The name of area to check
/// @return true if the area has reached mas oscillation, false otherwise
bool ProblemGenerationForBalancing::maxOscillationReached(const std::string& areaName) const
{
    bool maxOscillationReached = false;
    for (const auto& [areaCandidate, oscillationStatus]: oscillationRecords)
    {
        if (areaCandidate.first == areaName
            && oscillationStatus.first >= areas[areaName].maxOscillation)
        {
            maxOscillationReached = true;
            continue;
        }
    }
    return maxOscillationReached;
}

Candidate<InvestmentCandidateType>& ProblemGenerationForBalancing::getInvestmentCandidate(
  const std::string& areaName,
  const std::string& candidateName)
{
    return areas.at(areaName).getInvestmentCandidate(candidateName);
}

Candidate<DecommissioningCandidateType>& ProblemGenerationForBalancing::getDecommissioningCandidate(
  const std::string& areaName,
  const std::string& candidateName)
{
    return areas.at(areaName).getDecommissioningCandidate(candidateName);
}
