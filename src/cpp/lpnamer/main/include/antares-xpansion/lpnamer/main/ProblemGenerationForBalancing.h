#pragma once

#include <optional>
#include <string>

#include "ConfigurationManager.h"
#include "antares-xpansion/balancing/BalancingParser.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationOptimSimu.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

using AreaCandidate = std::pair<std::string, std::string>;

/// @brief Class to generate and modify problems in memory
class ProblemGenerationForBalancing: public ProblemGenerationOptimSimu
{
public:
    explicit ProblemGenerationForBalancing(ConfigurationManager::ConfigDirectories directories,
                                           std::map<std::string, Area>& areas,
                                           Logger logger,
                                           std::shared_ptr<ProblemManager> problemManager,
                                           std::filesystem::path iterationsLogFileName);
    virtual ~ProblemGenerationForBalancing() = default;
    std::shared_ptr<ProblemManager> updateProblems(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues);
    bool isBalanced() const;
    bool isBlocked() const;
    void logAreasView(const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues) const;
    void saveCandidatesResultsToCSV(const std::filesystem::path& outputPath) const;
    void saveAreasViewToCSV(const std::filesystem::path& outputPath) const;
    void saveIterativeAreasViewToCSV(int iteration) const;
    void updateAreaCriteriaData(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues);

    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& areaName,
                                                               const std::string& candidateName);
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& areaName,
      const std::string& candidateName);

private:
    bool blocked = false;
    std::map<std::string, Area>& areas;
    std::map<std::string, CapacityAction> areasAction;
    std::filesystem::path iterationsLogFileName;

    double lowerThreshold(const Area& area) const
    {
        return area.reliabilityStandard - area.reliabilityStandardDeadBandDown;
    }

    double higherThreshold(const Area& area) const
    {
        return area.reliabilityStandard + area.reliabilityStandardDeadBandUp;
    }

    void initializeIterativeLogCSV() const;
    void fillDispProdVarIndicesAndMarginalCosts();
    void setCapacitiesDataForCandidates();
    std::map<AreaCandidate, CapacityAction> findAreaCandidatesToModify(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues);
    CriterionState computeCriterionState(const Area& area, double value) const;
    void updateAreasIncrement();
    void applyActionToCluster(const AreaCandidate& areaCandidate, CapacityAction action);
    void updateOldCriterionState();
    std::optional<CapacityAction> determineCapacityAction(const Area& area) const;
    template<typename T>
    void fillDispProdVarIndicesAndMarginalCostsForArea(
      const std::string& areaName,
      const std::string& candidateName,
      Candidate<T>& candidate,
      const std::unordered_map<std::string, size_t>& varToIndex,
      const std::vector<double>& objCoeffs);

    void computeCandidateInstalledCapacity(CapacityAction action,
                                           Area& area,
                                           const std::string& candidateName);
    friend class BalancingTest;
};
