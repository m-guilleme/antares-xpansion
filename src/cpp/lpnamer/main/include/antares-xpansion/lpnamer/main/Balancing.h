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

/// @brief Class to generate and modify problems in memory
class Balancing: public ProblemGenerationOptimSimu
{
public:
    /// @param directories The directories to use for the problems generation
    /// @param areas The area investments to use for the problems modification
    /// @param logger The logger to use
    /// @param problemManager The manager holding the problems to modify
    explicit Balancing(ConfigurationManager::ConfigDirectories directories,
                       std::map<std::string, Area> areas,
                       Logger logger,
                       std::shared_ptr<ProblemManager> problemManager);
    virtual ~Balancing() = default;

    void run(int maxIterations, GreedyBalancingFinder& gbf);

private:
    std::map<std::string, Area> areas;
    std::map<std::string, CapacityAction> areasAction;

    // --- Initial setup -------------------------------------------------------
    void fillDispProdVarIndicesAndMarginalCosts();
    void setCapacitiesDataForCandidates();
    void prepareCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);
    void computeCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);
    void updateCandidatesCapacities();

    // --- Candidates access ---------------------------------------------------
    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& areaName,
                                                               const std::string& candidateName);
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& areaName,
      const std::string& candidateName);
    double getCandidateCurrentCapacity(CapacityAction action, const AreaCandidate& areaCandidate);

    // --- Criterion state -----------------------------------------------------
    bool isBalanced() const;
    CriterionState computeCriterionState(const Area& area, double value) const;
    void updateAreaCriteriaData(const SimuValues& simuValues);
    void updateAreasIncrement();
    void updateOldCriterionState();

    // --- Choosing and applying actions ---------------------------------------
    std::optional<CapacityAction> determineCapacityAction(const Area& area) const;
    AreaActions findAreaCandidatesToModify(const SimuValues& simuValues);
    void computeCandidateInstalledCapacity(CapacityAction action,
                                           Area& area,
                                           const std::string& candidateName);
    void initializeCandidatesToModify(const AreaActions& areaCandidatesToModify);
    void applyCapacityActions(const AreaActions& areaCandidatesToModify);
    void applyActionToCandidate(Antares::Solver::WeeklyProblemId pbId,
                                const std::shared_ptr<Problem>& problem,
                                const AreaActions& areaCandidatesToModify);
    SimuValues solveAndComputeCriterionAndPrice(const AreaActions& areaCandidatesToModify,
                                                GreedyBalancingFinder& gbf);

    // --- Logging and CSV output ----------------------------------------------
    void logMessage(const std::string& message,
                    LogUtils::LOGLEVEL level = LogUtils::LOGLEVEL::INFO) const;
    void logAreasView() const;
    std::string describeAction(const AreaCandidate& areaCandidate,
                               CapacityAction action,
                               double previousCapacity);
    void initializeIterativeLogCSV(const std::filesystem::path& iterationsLogFilePath) const;
    void saveIterativeAreasViewToCSV(int iteration,
                                     const std::filesystem::path& iterationsLogFilePath);
    void saveCandidatesResultsToCSV(const std::filesystem::path& outputPath) const;
    void saveAreasViewToCSV(const std::filesystem::path& outputPath) const;

    friend class BalancingTest;
};
