#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ConfigurationManager.h"
#include "antares-xpansion/balancing/InputBalancingParser.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationOptimSimu.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

using AreaCandidate = std::pair<std::string, std::string>;
using AreaActions = std::map<AreaCandidate, CapacityAction>;
using SimuValues = std::map<Antares::Solver::WeeklyProblemId, PbOutput>;

class Balancing: public ProblemGenerationOptimSimu
{
public:
    /// @brief Build the balancing and prepare the candidates data from the first problem
    /// @param directories The directories to use for the problems generation
    /// @param areas The area investments to use for the problems modification
    /// @param logger The logger to use
    /// @param problemManager The manager holding the problems to modify
    explicit Balancing(ConfigurationManager::ConfigDirectories directories,
                       std::map<std::string, Area> areas,
                       Logger logger,
                       std::shared_ptr<ProblemManager> problemManager);
    virtual ~Balancing() = default;

    /// @brief Run the balancing process: modify the candidates capacities iteratively until every
    /// area is balanced, no action is possible anymore or the maximum number of iterations is
    /// reached
    /// @param maxIterations The maximum number of iterations
    /// @param gbf The finder used to compute the criterion and prices of the problems
    void run(int maxIterations, GreedyBalancingFinder& gbf);

private:
    std::map<std::string, Area> areas;
    std::map<std::string, CapacityAction> areasAction;

    // --- Initial setup -------------------------------------------------------

    /// @brief Find the DispatchableProduction variable indices and compute the marginal cost of
    /// every candidate from the first problem
    void fillDispProdVarIndicesAndMarginalCosts();

    /// @brief Compute the bounds data of every candidate and set their capacities from it
    void setCapacitiesDataForCandidates();

    /// @brief Prepare the bounds data storage of every candidate
    /// @param pbIds The ids of the problems to prepare the bounds data for
    void prepareCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);

    /// @brief Compute, in parallel over the problems, the bounds data of every candidate
    /// @param pbIds The ids of the problems to compute the bounds data from
    void computeCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);

    /// @brief Update the capacities of every candidate from its bounds data
    void updateCandidatesCapacities();

    // --- Candidates access ---------------------------------------------------

    /// @brief Get an investment candidate of an area
    /// @param areaName The name of the area
    /// @param candidateName The name of the investment candidate
    /// @return A reference to the investment candidate
    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& areaName,
                                                               const std::string& candidateName);

    /// @brief Get a decommissioning candidate of an area
    /// @param areaName The name of the area
    /// @param candidateName The name of the decommissioning candidate
    /// @return A reference to the decommissioning candidate
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& areaName,
      const std::string& candidateName);

    /// @brief Get the current installed capacity of a candidate
    /// @param action The action that determines which kind of candidate is looked up
    /// @param areaCandidate The area name and the candidate name
    /// @return The installed capacity of the candidate
    double getCandidateCurrentCapacity(CapacityAction action, const AreaCandidate& areaCandidate);

    // --- Criterion state -----------------------------------------------------

    /// @brief Check whether the system is balanced
    /// @return true if every area has a valid criterion state, false otherwise
    bool isBalanced() const;

    /// @brief Compute the criterion state from the area thresholds and a criterion value
    /// @param area The area whose thresholds are used
    /// @param value The criterion value
    /// @return LOWER, HIGHER or VALID depending on the value position relative to the thresholds
    CriterionState computeCriterionState(const Area& area, double value) const;

    /// @brief Update the average criteria and the criterion state of every area
    /// @param simuValues The simulation values to compute the average criteria from
    void updateAreaCriteriaData(const SimuValues& simuValues);

    /// @brief Reduce the current increments of the areas whose criterion state has changed
    void updateAreasIncrement();

    /// @brief Set the old criterion state of every area to its current criterion state
    void updateOldCriterionState();

    // --- Choosing and applying actions ---------------------------------------

    /// @brief Find the action to apply to an area from its criterion state and previous action
    /// @param area The area to find the action for
    /// @return The action to apply, or nothing if no action is possible
    std::optional<CapacityAction> determineCapacityAction(const Area& area) const;

    /// @brief Find the action to apply for each area candidate
    /// @param simuValues The simulation values to use to select the candidates
    /// @return A map associating each selected area candidate to its action
    AreaActions findAreaCandidatesToModify(const SimuValues& simuValues);

    /// @brief Compute the new installed capacity of a candidate
    /// @param action The action to apply
    /// @param area The area of the candidate, which provides the current increments
    /// @param candidateName The name of the candidate to update
    void computeCandidateInstalledCapacity(CapacityAction action,
                                           Area& area,
                                           const std::string& candidateName);

    /// @brief Update the last action, the capacity and the oscillation status of the selected
    /// candidates
    /// @param areaCandidatesToModify The selected area candidates and their action
    void prepareCandidatesToModify(const AreaActions& areaCandidatesToModify);

    /// @brief Update the capacities of the selected candidates and log what changed
    /// @param areaCandidatesToModify The selected area candidates and their action
    void applyCapacityActions(const AreaActions& areaCandidatesToModify);

    /// @brief Set, in a problem, the new bounds of every selected candidate
    /// @param pbId The id of the problem
    /// @param problem The problem to modify
    /// @param areaCandidatesToModify The selected area candidates and their action
    void applyActionToCandidate(Antares::Solver::WeeklyProblemId pbId,
                                const std::shared_ptr<Problem>& problem,
                                const AreaActions& areaCandidatesToModify);

    /// @brief Apply the actions to every problem, then compute their criterion and prices
    /// @param areaCandidatesToModify The selected area candidates and their action, which may be
    /// empty
    /// @param gbf The finder used to compute the criterion and prices
    /// @return The balancing results of every problem
    SimuValues solveAndComputeCriterionAndPrice(const AreaActions& areaCandidatesToModify,
                                                GreedyBalancingFinder& gbf);

    // --- Logging and CSV output ----------------------------------------------

    /// @brief Log a message in the problem generation context
    /// @param message The message to log
    /// @param level The log level
    void logMessage(const std::string& message,
                    LogUtils::LOGLEVEL level = LogUtils::LOGLEVEL::INFO) const;

    /// @brief Log, for every area, its criterion state and its candidates capacities
    void logAreasView() const;

    /// @brief Describe the action applied to a candidate
    /// @param areaCandidate The area name and the candidate name
    /// @param action The action applied
    /// @param previousCapacity The capacity of the candidate before the action
    /// @return A description of the area criteria, the action and the capacity change
    std::string describeAction(const AreaCandidate& areaCandidate,
                               CapacityAction action,
                               double previousCapacity);

    /// @brief Create the iterations log file and write its header
    /// @param iterationsLogFilePath The path of the file to create
    void initializeIterativeLogCSV(const std::filesystem::path& iterationsLogFilePath) const;

    /// @brief Append to the iterations log file the candidates whose capacity changed
    /// @param iteration The current iteration
    /// @param iterationsLogFilePath The path of the iterations log file
    void saveIterativeAreasViewToCSV(int iteration,
                                     const std::filesystem::path& iterationsLogFilePath);

    /// @brief Write the final capacity and capacity change of every candidate
    /// @param outputPath The path of the file to write
    void saveCandidatesResultsToCSV(const std::filesystem::path& outputPath) const;

    /// @brief Write the final criterion, status and total capacity change of every area
    /// @param outputPath The path of the file to write
    void saveAreasViewToCSV(const std::filesystem::path& outputPath) const;

    friend class BalancingTest;
};
