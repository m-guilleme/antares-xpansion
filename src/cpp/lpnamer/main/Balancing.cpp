#include "antares-xpansion/lpnamer/main/Balancing.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tbb/parallel_for_each.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include <antares/api/solver.h>
#include <antares/solver/lps/LpsFromAntares.h>

#include "antares-xpansion/balancing/utils.h"

namespace
{
constexpr const char* ITERATIONS_LOG_FILE = "iterations_values_log.csv";
constexpr const char* FINAL_CRITERIA_FILE = "final_criteria.csv";
constexpr const char* FINAL_CAPACITIES_FILE = "final_capacities.csv";
constexpr const char* WHITESPACES = " \t\n\r\f\v";

// An increment never goes below 10% of its initial value, and shrinks by 10% each time the
// criterion state changes
constexpr double INCREMENT_REDUCTION_RATIO = 0.1;

// ---------------------------------------------------------------------------
// Iteration over candidates
// ---------------------------------------------------------------------------

template<typename AreaT, typename Func>
void forEachCandidateOnArea(AreaT& area, Func&& f)
{
    for (auto& [candidateName, candidate]: area.investmentCandidates)
    {
        f(candidateName, candidate);
    }
    for (auto& [candidateName, candidate]: area.decommissioningCandidates)
    {
        f(candidateName, candidate);
    }
}

// Calls f(areaName, candidateName, candidate) on every candidate of every area
template<typename AreasT, typename Func>
void forEachCandidate(AreasT& areas, Func&& f)
{
    for (auto& [areaName, area]: areas)
    {
        forEachCandidateOnArea(area,
                               [&](const std::string& candidateName, auto& candidate)
                               { f(areaName, candidateName, candidate); });
    }
}

// ---------------------------------------------------------------------------
// Dispatchable production variables
// ---------------------------------------------------------------------------

std::string toLower(std::string_view text)
{
    std::string lower(text);
    std::ranges::transform(lower,
                           lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

void trimTrailingWhitespaces(std::string& text)
{
    text.erase(text.find_last_not_of(WHITESPACES) + 1);
}

std::unordered_map<std::string, size_t> buildVarToIndex(const std::vector<std::string>& vars)
{
    std::unordered_map<std::string, size_t> varToIndex;
    varToIndex.reserve(vars.size());
    for (size_t i = 0; i < vars.size(); ++i)
    {
        varToIndex.emplace(toLower(vars[i]), i);
    }
    return varToIndex;
}

std::string dispatchableProductionVarName(const std::string& areaName,
                                          const std::string& candidateName,
                                          size_t hour)
{
    return "dispatchableproduction::area<" + areaName + ">::thermalcluster<" + candidateName
           + ">::hour<" + std::to_string(hour) + ">";
}

template<typename T>
void setDispProdVarIndicesAndMarginalCost(const std::string& areaName,
                                          const std::string& candidateName,
                                          Candidate<T>& candidate,
                                          const std::unordered_map<std::string, size_t>& varToIndex,
                                          const std::vector<double>& objCoeffs)
{
    for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
    {
        const auto it = varToIndex.find(
          dispatchableProductionVarName(areaName, candidateName, hour));
        if (it == varToIndex.end())
        {
            throw std::runtime_error("Failed to find for candidate " + candidateName + " of area "
                                     + areaName + " the dispProdVarIndices of hour "
                                     + std::to_string(hour));
        }
        candidate.dispProdVarIndices[hour] = it->second;
        if (hour == 0)
        {
            candidate.marginalCost = objCoeffs[it->second];
        }
    }
}

// ---------------------------------------------------------------------------
// Capacity actions
// ---------------------------------------------------------------------------

bool isInvestmentAction(CapacityAction action)
{
    return action == CapacityAction::INVESTMENT || action == CapacityAction::DISINVESTMENT;
}

bool isActionPossible(const Area& area, CapacityAction action)
{
    switch (action)
    {
    case CapacityAction::INVESTMENT:
        return area.isInvestmentPossible();
    case CapacityAction::DISINVESTMENT:
        return area.isDisinvestmentPossible();
    case CapacityAction::DECOMMISSIONING:
        return area.isDecommissioningPossible();
    case CapacityAction::RECOMMISSIONING:
        return area.isRecommissioningPossible();
    }
    return false;
}

bool isInvestmentCycle(const Area& area)
{
    if (!area.lastAction.has_value())
    {
        return area.criterionState == CriterionState::HIGHER;
    }
    return isInvestmentAction(area.lastAction.value());
}

std::array<CapacityAction, 2> filterActions(bool investmentCycle, bool criterionIsHigher)
{
    if (investmentCycle)
    {
        if (criterionIsHigher)
        {
            return {CapacityAction::INVESTMENT, CapacityAction::RECOMMISSIONING};
        }
        return {CapacityAction::DISINVESTMENT, CapacityAction::DECOMMISSIONING};
    }
    if (criterionIsHigher)
    {
        return {CapacityAction::RECOMMISSIONING, CapacityAction::INVESTMENT};
    }
    return {CapacityAction::DECOMMISSIONING, CapacityAction::DISINVESTMENT};
}

// ---------------------------------------------------------------------------
// Increment Cost
// ---------------------------------------------------------------------------

bool shouldReduceIncrements(const Area& area)
{
    return area.oldCriterionState != area.criterionState
           && area.oldCriterionState != CriterionState::UNINITIALIZED
           && area.criterionState != CriterionState::VALID;
}

double reducedIncrement(double currentIncrement, double initialIncrement)
{
    return std::max(initialIncrement * INCREMENT_REDUCTION_RATIO,
                    currentIncrement - INCREMENT_REDUCTION_RATIO * initialIncrement);
}

// ---------------------------------------------------------------------------
// Criteria
// ---------------------------------------------------------------------------

std::map<std::string, double> computeAverageAreaCriteriaValues(const SimuValues& simuValues)
{
    std::set<unsigned int> years;
    std::map<std::string, double> averageCriteria;

    for (const auto& [id, output]: simuValues)
    {
        years.insert(id.year);
        for (const auto& [area, value]: output.areaCriterionValues)
        {
            averageCriteria[area] += value;
        }
    }

    const double numYears = static_cast<double>(years.size());
    for (auto& sum: averageCriteria | std::views::values)
    {
        sum /= numYears;
    }
    return averageCriteria;
}

// ---------------------------------------------------------------------------
// Problem bounds
// ---------------------------------------------------------------------------

// Bounds to change for each candidate in a problem
struct CandidatesBoundsChange
{
    std::vector<int> varIndices;
    std::vector<char> boundTypes;
    std::vector<double> values;

    explicit CandidatesBoundsChange(size_t nbCandidates)
    {
        const size_t size = nbCandidates * 2 * NUMBER_OF_HOURS_PER_WEEK;
        varIndices.reserve(size);
        boundTypes.reserve(size);
        values.reserve(size);
    }

    template<typename T>
    void addBoundsChange(const Candidate<T>& candidate, Antares::Solver::WeeklyProblemId pbId)
    {
        const auto& hourlyBounds = candidate.boundsData.at(pbId);
        std::array<double, NUMBER_OF_HOURS_PER_WEEK> upperBounds;
        std::array<double, NUMBER_OF_HOURS_PER_WEEK> lowerBounds;
        for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
        {
            const int index = static_cast<int>(candidate.dispProdVarIndices[hour]);
            // add upper candidate bounds change
            double upperBoundValue = hourlyBounds[hour].upBoundRatioToInstalledCap
                                     * candidate.installedCapacity;
            appendBoundsChange('U', index, upperBoundValue);
            // add lower candidate bounds change
            double lowerBoundValue = upperBoundValue * hourlyBounds[hour].lowBoundRatioToUpBound;
            appendBoundsChange('L', index, lowerBoundValue);
        }
    }

private:
    void appendBoundsChange(char boundType, int index, double value)
    {
        boundTypes.push_back(boundType);
        varIndices.push_back(index);
        values.push_back(value);
    }
};

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

std::ofstream openForWriting(const std::filesystem::path& path,
                             std::ios_base::openmode mode = std::ios_base::out)
{
    std::ofstream file(path, mode);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    return file;
}

template<typename CandidateMap>
void describeCandidates(std::ostream& out, std::string_view label, const CandidateMap& candidates)
{
    for (const auto& [candidateName, candidate]: candidates)
    {
        out << "  " << label << " capacity for cluster candidate " << candidateName << ": "
            << candidate.installedCapacity << " | oscillation : " << candidate.oscillationCounter
            << "\n";
    }
}

template<typename T>
void writeCapacityChange(std::ostream& file,
                         int iteration,
                         const std::string& areaName,
                         const Area& area,
                         const std::string& candidateName,
                         Candidate<T>& candidate)
{
    if (candidate.installedCapacity != candidate.previousInstalledCapacity)
    {
        std::string_view lastAction = area.lastAction.has_value()
                                        ? to_string(area.lastAction.value())
                                        : "NOACTION";
        file << iteration << "," << areaName << "," << to_string(area.criterionState) << ","
             << lastAction << "," << candidateName << ","
             << candidate.installedCapacity - candidate.previousInstalledCapacity << "\n";
    }
    candidate.previousInstalledCapacity = candidate.installedCapacity;
}

double totalCapacityChange(const Area& area)
{
    double total = 0.0;
    forEachCandidateOnArea(area,
                           [&total](const std::string&, const auto& candidate) {
                               total += candidate.installedCapacity
                                        - candidate.initInstalledCapacity;
                           });
    return total;
}

std::string_view criteriaStatus(const Area& area)
{
    if (area.avgCriteria < area.lowerThreshold())
    {
        return "LOWER";
    }
    if (area.avgCriteria > area.higherThreshold())
    {
        return "HIGHER";
    }
    return "NO ACTION";
}
} // namespace

// ---------------------------------------------------------------------------
// Construction and initial setup
// ---------------------------------------------------------------------------

Balancing::Balancing(ConfigurationManager::ConfigDirectories directories,
                     std::map<std::string, Area> areas,
                     Logger logger,
                     std::shared_ptr<ProblemManager> problemManager):
    ProblemGenerationOptimSimu(directories, logger, problemManager),
    areas(std::move(areas))
{
    logMessage("Get candidates indices and marginal cost");
    fillDispProdVarIndicesAndMarginalCosts();
    logMessage("Get candidate bounds data");
    setCapacitiesDataForCandidates();
}

void Balancing::fillDispProdVarIndicesAndMarginalCosts()
{
    const auto& firstProblem = problemManager->getFirstProblem();
    auto vars = firstProblem->get_col_names();
    for (auto& var: vars)
    {
        trimTrailingWhitespaces(var);
    }

    std::vector<double> objCoeffs(vars.size());
    firstProblem->get_obj(objCoeffs.data(), 0, static_cast<int>(vars.size()) - 1);

    const auto varToIndex = buildVarToIndex(vars);
    forEachCandidate(areas,
                     [&](const std::string& areaName,
                         const std::string& candidateName,
                         auto& candidate)
                     {
                         setDispProdVarIndicesAndMarginalCost(areaName,
                                                              candidateName,
                                                              candidate,
                                                              varToIndex,
                                                              objCoeffs);
                     });
}

void Balancing::setCapacitiesDataForCandidates()
{
    const auto& pbIds = problemManager->getProblemIds();
    prepareCandidatesBoundsData(pbIds);
    computeCandidatesBoundsData(pbIds);
    updateCandidatesCapacities();
}

void Balancing::prepareCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds)
{
    forEachCandidate(areas,
                     [&](const std::string&, const std::string&, CandidateBase& c)
                     { c.prepareBoundsData(pbIds); });
}

void Balancing::computeCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds)
{
    tbb::parallel_for_each(pbIds,
                           [&](const auto& pbId)
                           {
                               const auto problem = problemManager->getProblemFromId(pbId);
                               forEachCandidate(
                                 areas,
                                 [&](const std::string&, const std::string&, CandidateBase& c)
                                 { c.computeAndSetOneWeekCapacityData(pbId, problem); });
                           });
}

void Balancing::updateCandidatesCapacities()
{
    forEachCandidate(areas,
                     [](const std::string&, const std::string&, CandidateBase& c)
                     { c.updateCapacitiesFromBounds(); });
}

// ---------------------------------------------------------------------------
// Candidates access
// ---------------------------------------------------------------------------

Candidate<InvestmentCandidateType>& Balancing::getInvestmentCandidate(
  const std::string& areaName,
  const std::string& candidateName)
{
    return areas.at(areaName).getInvestmentCandidate(candidateName);
}

Candidate<DecommissioningCandidateType>& Balancing::getDecommissioningCandidate(
  const std::string& areaName,
  const std::string& candidateName)
{
    return areas.at(areaName).getDecommissioningCandidate(candidateName);
}

double Balancing::getCandidateCurrentCapacity(CapacityAction action,
                                              const AreaCandidate& areaCandidate)
{
    auto& area = areas.at(areaCandidate.first);
    return isInvestmentAction(action)
             ? area.getInvestmentCandidate(areaCandidate.second).installedCapacity
             : area.getDecommissioningCandidate(areaCandidate.second).installedCapacity;
}

// ---------------------------------------------------------------------------
// Criterion state
// ---------------------------------------------------------------------------

bool Balancing::isBalanced() const
{
    return std::ranges::all_of(areas | std::views::values,
                               [](const Area& area)
                               { return area.criterionState == CriterionState::VALID; });
}

CriterionState Balancing::computeCriterionState(const Area& area, double value) const
{
    if (value < area.lowerThreshold())
    {
        return CriterionState::LOWER;
    }
    if (value > area.higherThreshold())
    {
        return CriterionState::HIGHER;
    }
    return CriterionState::VALID;
}

void Balancing::updateAreaCriteriaData(const SimuValues& simuValues)
{
    const auto averageCriteria = computeAverageAreaCriteriaValues(simuValues);
    for (auto& [areaName, area]: areas)
    {
        const double average = averageCriteria.at(areaName);
        area.avgCriteria = average;
        area.criterionState = computeCriterionState(area, average);
    }
}

void Balancing::updateAreasIncrement()
{
    for (auto& area: areas | std::views::values)
    {
        if (shouldReduceIncrements(area))
        {
            area.currentInvestmentIncrement = reducedIncrement(area.currentInvestmentIncrement,
                                                               area.investmentIncrement);
            area.currentDecommissioningIncrement = reducedIncrement(
              area.currentDecommissioningIncrement,
              area.decommissioningIncrement);
        }
    }
}

void Balancing::updateOldCriterionState()
{
    for (auto& area: areas | std::views::values)
    {
        area.oldCriterionState = area.criterionState;
    }
}

// ---------------------------------------------------------------------------
// Choosing and applying actions on candidates
// ---------------------------------------------------------------------------

std::optional<CapacityAction> Balancing::determineCapacityAction(const Area& area) const
{
    if (area.maxOscillationReached())
    {
        std::ostringstream oss;
        oss << "No action in area " << area.name << " because max oscillation has been reached";
        logMessage(oss.str());
        return std::nullopt;
    }

    const bool criterionIsHigher = area.criterionState == CriterionState::HIGHER;
    for (const CapacityAction action: filterActions(isInvestmentCycle(area), criterionIsHigher))
    {
        if (isActionPossible(area, action))
        {
            return action;
        }
    }

    // if no action is possible: logging a warning and carrying on
    std::ostringstream oss;
    oss << "Area " << area.name << " is not balanced but no modification is possible\n"
        << " Current criterion state: " << to_string(area.criterionState) << "\n"
        << " Previous action: "
        << (area.lastAction.has_value() ? to_string(area.lastAction.value()) : "None") << "\n";
    logMessage(oss.str(), LogUtils::LOGLEVEL::WARNING);
    return std::nullopt;
}

AreaActions Balancing::findAreaCandidatesToModify(const SimuValues& simuValues)
{
    logMessage("Find areas candidate to modify");
    areasAction.clear();
    updateAreasIncrement();

    for (auto& [areaName, area]: areas)
    {
        if (area.criterionState == CriterionState::VALID)
        {
            continue;
        }
        // if no action is possible, no candidate will be modified
        if (const auto action = determineCapacityAction(area))
        {
            areasAction[areaName] = action.value();
            area.initializeRentability(action.value());
        }
    }

    for (const auto& [pbId, pbOutput]: simuValues)
    {
        const std::vector<double> solution = problemManager->getProblemSolution(pbId);
        for (const auto& [areaName, action]: areasAction)
        {
            areas.at(areaName).updateRentabilityWithProblem(action, pbOutput, solution);
        }
    }

    AreaActions areaCandidatesToModify;
    for (const auto& [areaName, action]: areasAction)
    {
        const std::string candidateName = areas.at(areaName).selectCandidateWithBestRentability(
          action);
        areaCandidatesToModify[{areaName, candidateName}] = action;
    }

    updateOldCriterionState();
    return areaCandidatesToModify;
}

void Balancing::computeCandidateInstalledCapacity(CapacityAction action,
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

void Balancing::initializeCandidatesToModify(const AreaActions& areaCandidatesToModify)
{
    for (const auto& [areaCandidate, action]: areaCandidatesToModify)
    {
        auto& area = areas.at(areaCandidate.first);
        const auto& candidateName = areaCandidate.second;

        area.lastAction = action;
        computeCandidateInstalledCapacity(action, area, candidateName);
        if (isInvestmentAction(action))
        {
            area.getInvestmentCandidate(candidateName).updateOscillationStatus(action);
        }
        else
        {
            area.getDecommissioningCandidate(candidateName).updateOscillationStatus(action);
        }
    }
}

void Balancing::applyCapacityActions(const AreaActions& areaCandidatesToModify)
{
    if (areaCandidatesToModify.empty())
    {
        return;
    }
    logMessage("Apply action");

    std::map<AreaCandidate, double> previousCapacities;
    for (const auto& [areaCandidate, action]: areaCandidatesToModify)
    {
        previousCapacities[areaCandidate] = getCandidateCurrentCapacity(action, areaCandidate);
    }

    initializeCandidatesToModify(areaCandidatesToModify);

    for (const auto& [areaCandidate, action]: areaCandidatesToModify)
    {
        logMessage(describeAction(areaCandidate, action, previousCapacities.at(areaCandidate)));
    }
}

void Balancing::applyActionToCandidate(Antares::Solver::WeeklyProblemId pbId,
                                       const std::shared_ptr<Problem>& problem,
                                       const AreaActions& areaCandidatesToModify)
{
    CandidatesBoundsChange cBoundsChange(areaCandidatesToModify.size());
    for (const auto& [areaCandidate, action]: areaCandidatesToModify)
    {
        auto& area = areas.at(areaCandidate.first);
        const auto& candidateName = areaCandidate.second;
        if (isInvestmentAction(action))
        {
            cBoundsChange.addBoundsChange(area.getInvestmentCandidate(candidateName), pbId);
        }
        else
        {
            cBoundsChange.addBoundsChange(area.getDecommissioningCandidate(candidateName), pbId);
        }
    }
    problem->chg_bounds(cBoundsChange.varIndices, cBoundsChange.boundTypes, cBoundsChange.values);
    problemManager->setProblem(pbId, problem);
}

SimuValues Balancing::solveAndComputeCriterionAndPrice(const AreaActions& areaCandidatesToModify,
                                                       GreedyBalancingFinder& gbf)
{
    tbb::parallel_for_each(problemManager->getProblemIds(),
                           [&](const auto& pbId)
                           {
                               std::shared_ptr<Problem> problem = problemManager->getProblemFromId(
                                 pbId);
                               if (!areaCandidatesToModify.empty())
                               {
                                   applyActionToCandidate(pbId, problem, areaCandidatesToModify);
                               }
                               gbf.computeCriterionAndPrice(pbId, problem);
                           });
    return gbf.getBalancingResults();
}

// ---------------------------------------------------------------------------
// Logging and CSV output
// ---------------------------------------------------------------------------

void Balancing::logMessage(const std::string& message, LogUtils::LOGLEVEL level) const
{
    logger->display_message(message, level, PROBLEM_GENERATION_LOGGER_CONTEXT);
}

void Balancing::logAreasView() const
{
    for (const auto& [areaName, area]: areas)
    {
        std::ostringstream ss;
        ss << "\n  Criterion state for area " << areaName << ": " << to_string(area.criterionState)
           << " | max oscillation reached : " << std::boolalpha << area.maxOscillationReached()
           << "\n"
           << "  Average criteria value: " << area.avgCriteria;
        if (area.criterionState == CriterionState::HIGHER)
        {
            ss << " (which is above the threshold of " << area.higherThreshold() << ")";
        }
        else if (area.criterionState != CriterionState::VALID)
        {
            ss << " (which is below the threshold of " << area.lowerThreshold() << ")";
        }
        ss << "\n";

        describeCandidates(ss, "Invested", area.investmentCandidates);
        describeCandidates(ss, "Decommissioned", area.decommissioningCandidates);

        logMessage(ss.str());
    }
}

std::string Balancing::describeAction(const AreaCandidate& areaCandidate,
                                      CapacityAction action,
                                      double previousCapacity)
{
    const auto& area = areas.at(areaCandidate.first);
    const double newCapacity = getCandidateCurrentCapacity(action, areaCandidate);

    std::ostringstream os;
    os << " Area: " << areaCandidate.first << " criteria: " << area.avgCriteria << " ["
       << area.lowerThreshold() << "-" << area.higherThreshold() << "]"
       << " action: " << to_string(action) << " cluster candidate: " << areaCandidate.second
       << " new capacity: " << newCapacity << " delta: " << (newCapacity - previousCapacity);
    return os.str();
}

void Balancing::initializeIterativeLogCSV(const std::filesystem::path& iterationsLogFilePath) const
{
    auto file = openForWriting(iterationsLogFilePath);
    file << "iteration,zone,criteria,action,cluster candidate,capacity change\n";
}

void Balancing::saveIterativeAreasViewToCSV(int iteration,
                                            const std::filesystem::path& iterationsLogFilePath)
{
    auto file = openForWriting(iterationsLogFilePath, std::ios_base::app);
    for (auto& [areaName, area]: areas)
    {
        for (auto& [candidateName, candidate]: area.investmentCandidates)
        {
            writeCapacityChange(file, iteration, areaName, area, candidateName, candidate);
        }
        for (auto& [candidateName, candidate]: area.decommissioningCandidates)
        {
            writeCapacityChange(file, iteration, areaName, area, candidateName, candidate);
        }
    }
}

void Balancing::saveCandidatesResultsToCSV(const std::filesystem::path& outputPath) const
{
    auto file = openForWriting(outputPath);
    file << "area name,candidate name,capacity,capacity change\n";
    forEachCandidate(
      areas,
      [&file](const std::string& areaName, const std::string& candidateName, const auto& candidate)
      {
          file << areaName << "," << candidateName << "," << candidate.installedCapacity << ","
               << candidate.installedCapacity - candidate.initInstalledCapacity << "\n";
      });
}

void Balancing::saveAreasViewToCSV(const std::filesystem::path& outputPath) const
{
    auto file = openForWriting(outputPath);
    file << "area,criteria value,criteria target,criteria lower bound,criteria upper bound,status,"
            "total capacity change\n";
    for (const auto& [areaName, area]: areas)
    {
        file << areaName << "," << area.avgCriteria << "," << area.reliabilityStandard << ","
             << area.lowerThreshold() << "," << area.higherThreshold() << ","
             << criteriaStatus(area) << "," << totalCapacityChange(area) << "\n";
    }
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void Balancing::run(int maxIterations, GreedyBalancingFinder& gbf)
{
    const auto iterationsLogFilePath = directories.simulation_dir / ITERATIONS_LOG_FILE;
    const auto finalCriteriaFilePath = directories.simulation_dir / FINAL_CRITERIA_FILE;
    const auto finalCapacitiesFilePath = directories.simulation_dir / FINAL_CAPACITIES_FILE;

    const auto logRun = [this](const std::string& message)
    { logger->display_message(message, LogUtils::LOGLEVEL::INFO, logger->CONTEXT); };

    logMessage("Initialize balancing logs");
    initializeIterativeLogCSV(iterationsLogFilePath);

    SimuValues simuValues;
    // First iteration is iteration 0: no modification is applied to the problems yet
    int iteration = -1;
    const auto startBalancingProcess = std::chrono::steady_clock::now();
    logMessage("Starting balancing process");
    logAreasView();

    while (!isBalanced() && iteration < maxIterations)
    {
        ++iteration;
        const auto startIteration = std::chrono::steady_clock::now();
        logRun("Iteration " + std::to_string(iteration));

        // simuValues is empty on the first iteration: nothing to modify yet
        const AreaActions areaCandidatesToModify = simuValues.empty()
                                                     ? AreaActions{}
                                                     : findAreaCandidatesToModify(simuValues);
        if (iteration > 0 && areaCandidatesToModify.empty())
        {
            logMessage("No actions found in any area, stop the run");
            break;
        }

        applyCapacityActions(areaCandidatesToModify);
        simuValues = solveAndComputeCriterionAndPrice(areaCandidatesToModify, gbf);
        const auto endIteration = std::chrono::steady_clock::now();

        saveIterativeAreasViewToCSV(iteration, iterationsLogFilePath);
        updateAreaCriteriaData(simuValues);
        logAreasView();

        const std::chrono::duration<double> iterationDuration = endIteration - startIteration;
        logRun("Elapsed time for iteration " + std::to_string(iteration) + ": "
               + formatDuration(iterationDuration));
    }

    logMessage("Final iteration " + std::to_string(iteration));
    logAreasView();
    saveCandidatesResultsToCSV(finalCapacitiesFilePath);
    saveAreasViewToCSV(finalCriteriaFilePath);

    const std::chrono::duration<double> totalDuration = std::chrono::steady_clock::now()
                                                        - startBalancingProcess;
    logMessage("Balancing process ended after " + std::to_string(iteration) + " iterations. In "
               + formatDuration(totalDuration));
    logMessage(isBalanced() ? "The system is balanced." : "The system is not balanced.");
}
