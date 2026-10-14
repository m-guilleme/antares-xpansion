#pragma once

#include <optional>
#include <string>

#include "ConfigurationManager.h"
#include "antares-xpansion/balancing/InputBalancingParser.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationOptimSimu.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

using AreaCandidate = std::pair<std::string, std::string>;

/// @brief Class to generate and modify problems in memory
class Balancing: public ProblemGenerationOptimSimu
{
public:
    explicit Balancing(ConfigurationManager::ConfigDirectories directories,
                       std::map<std::string, Area> areas,
                       Logger logger,
                       std::shared_ptr<ProblemManager> problemManager);
    virtual ~Balancing() = default;
    bool isBalanced() const;
    bool isBlocked() const;
    void logAreasView(const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues) const;
    void saveCandidatesResultsToCSV(const std::filesystem::path& outputPath) const;
    void saveAreasViewToCSV(const std::filesystem::path& outputPath) const;
    void saveIterativeAreasViewToCSV(int iteration,
                                     const std::filesystem::path& iterationsLogFileName);
    void updateAreaCriteriaData(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues);
    std::map<AreaCandidate, CapacityAction> findAreaCandidatesToModify(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues);

    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& areaName,
                                                               const std::string& candidateName);
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& areaName,
      const std::string& candidateName);
    double getCandidateCurrentCapacity(const CapacityAction& action,
                                       const AreaCandidate& areaCandidate);
    void initializeCandidatesToModify(
      std::map<AreaCandidate, CapacityAction>& areaCandidatesToModify);
    void applyActionToCandidate(Antares::Solver::WeeklyProblemId pbId,
                                std::shared_ptr<Problem>& problem,
                                std::map<AreaCandidate, CapacityAction> areaCandidatesToModify);

    double lowerThreshold(const Area& area) const
    {
        return area.reliabilityStandard - area.reliabilityStandardDeadBandDown;
    }

    double higherThreshold(const Area& area) const
    {
        return area.reliabilityStandard + area.reliabilityStandardDeadBandUp;
    }

    std::map<std::string, Area> areas;
    void run(const int maxIterations, GreedyBalancingFinder& gbf);

private:
    std::map<std::string, CapacityAction> areasAction;
    std::filesystem::path iterationsLogFileName;

    void initializeIterativeLogCSV(const std::filesystem::path iterationsLogFileName) const;
    void fillDispProdVarIndicesAndMarginalCosts();
    void setCapacitiesDataForCandidates();
    CriterionState computeCriterionState(const Area& area, double value) const;
    void updateAreasIncrement();
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
    void prepareCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);
    void computeCandidatesBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);
    void updateCandidatesCapacities();
    std::string describeAction(const AreaCandidate& areaCandidate,
                               CapacityAction action,
                               double previousCapacity);

    friend class BalancingTest;
};
