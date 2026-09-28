#pragma once

#include <optional>
#include <string>

#include "ConfigurationManager.h"
#include "antares-xpansion/balancing/BalancingParser.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationOptimSimu.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

using AreaCandidate = std::pair<std::string, std::string>;

enum class CapacityAction
{
    INVESTMENT,
    DISINVESTMENT,
    DECOMMISSIONING,
    RECOMMISSIONING
};

using OscillationStatus = std::pair<int, std::optional<CapacityAction>>;

constexpr std::string_view to_string(CapacityAction action)
{
    switch (action)
    {
    case CapacityAction::INVESTMENT:
        return "INVESTMENT";
    case CapacityAction::DISINVESTMENT:
        return "DISINVESTMENT";
    case CapacityAction::DECOMMISSIONING:
        return "DECOMMISSIONING";
    case CapacityAction::RECOMMISSIONING:
        return "RECOMMISSIONING";
    }
}

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
    void logCriterionAndAreaSettings(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues) const;
    void saveClusterResultsToCSV(const std::filesystem::path& outputPath) const;
    void saveCriterionAndAreaSettingsToCSV(const std::filesystem::path& outputPath) const;
    void saveCriterionAndAreaSettingsToIterativeLogCSV(int iteration) const;
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
    std::map<AreaCandidate, OscillationStatus> oscillationRecords;
    std::map<std::string, CapacityAction> lastActionForArea;
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
    template<typename T>
    void setCapacityDataForOneCandidate(const std::string& areaName,
                                        const std::string& candidateName,
                                        Candidate<T>& candidate);
    void setCapacitiesDataForCandidates();
    void initializeOscillationRecords();
    bool maxOscillationReached(const std::string& areaName) const;
    void updateRecords(const AreaCandidate& areaCandidate, CapacityAction action);
    std::map<AreaCandidate, CapacityAction> findAreaCandidatesToModify(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues);
    CriterionState computeCriterionState(const Area& area, double value) const;
    void updateAreasIncrement();
    void applyActionToCluster(const AreaCandidate& areaCandidate, CapacityAction action);
    std::string getBestCandidate(
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues,
      const std::string& areaName,
      const Area& area,
      CapacityAction action) const;
    void updateOldCriterionState();
    std::optional<CapacityAction> determineCapacityAction(const std::string& areaName,
                                                          CriterionState currentState,
                                                          const Area& area) const;
    template<typename T>
    std::map<std::string, double> computeRentabilityForCandidates(
      const std::string& areaName,
      const std::map<std::string, Candidate<T>>& candidates,
      const std::map<Antares::Solver::WeeklyProblemId, PbOutput>& simuValues,
      CapacityAction action) const;
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
