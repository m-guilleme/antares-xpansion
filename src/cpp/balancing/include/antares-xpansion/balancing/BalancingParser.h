#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include <antares/api/solver.h>

#include "antares-xpansion/benders/benders_core/CriterionInputDataReader.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

constexpr int NUMBER_OF_HOURS_PER_WEEK = 168;

struct PbOutput
{
    std::map<std::string, int> areaCriterionValues{};
    std::map<std::string, std::array<double, NUMBER_OF_HOURS_PER_WEEK>>
      areaPrices{}; // Dual value of AreaBalance Constraint
};

enum class CriterionState
{
    LOWER,
    VALID,
    HIGHER,
    UNINITIALIZED,
};

enum class CapacityAction
{
    INVESTMENT,
    DISINVESTMENT,
    DECOMMISSIONING,
    RECOMMISSIONING
};

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

using AreaCriterionData = std::pair<double, CriterionState>;
using OscillationStatus = std::pair<int, std::optional<CapacityAction>>;

constexpr std::string_view to_string(CriterionState state)
{
    switch (state)
    {
    case CriterionState::LOWER:
        return "LOWER";
    case CriterionState::VALID:
        return "VALID";
    case CriterionState::HIGHER:
        return "HIGHER";
    case CriterionState::UNINITIALIZED:
        return "UNINITIALIZED";
    }
}

struct InvestmentCandidateType
{
    double derating;
    double expansionPotential;
    double investmentCost;
    double fixedOmCosts;
};

struct DecommissioningCandidateType
{
    double decommissioningPotential;
    double decommissioningCost;
    double fixedOmCosts;
};

struct BoundData
{
    double upBoundRatioToInstalledCap, lowBoundRatioToUpBound;
};

template<typename T>
struct Candidate
{
    std::shared_ptr<T> type;
    double installedCapacity;
    double previousInstalledCapacity;
    double initInstalledCapacity;
    double marginalCost;
    std::optional<CapacityAction> lastAction;
    int oscillationCounter = 0;
    std::array<size_t, NUMBER_OF_HOURS_PER_WEEK> dispProdVarIndices;
    std::map<Antares::Solver::WeeklyProblemId, std::vector<BoundData>> boundsData;

    void setOneWeekBoundsData(Antares::Solver::WeeklyProblemId pbId,
                              std::vector<BoundData> oneWeekBoundsData)
    {
        boundsData[pbId] = oneWeekBoundsData;
    }

    void updateOscillationStatus(const CapacityAction& action)
    {
        if (lastAction.has_value() && action != lastAction)
        {
            oscillationCounter += 1;
        }
        lastAction = action;
    }
};

struct Area
{
    double reliabilityStandard;
    double reliabilityStandardDeadBandUp;
    double reliabilityStandardDeadBandDown;
    double decommissioningIncrement;
    double currentDecommissioningIncrement;
    double investmentIncrement;
    double currentInvestmentIncrement;
    std::optional<CapacityAction> lastAction;
    std::string name;
    int maxOscillation;
    std::map<std::string, Candidate<DecommissioningCandidateType>> decommissioningCandidates;
    std::map<std::string, Candidate<InvestmentCandidateType>> investmentCandidates;
    std::map<std::string, double> candidatesRentability;
    CriterionState oldCriterionState{CriterionState::UNINITIALIZED};
    CriterionState criterionState{CriterionState::UNINITIALIZED};
    double avgCriteria;

    bool isInvestmentPossible() const;
    bool isDecommissioningPossible() const;
    bool isDisinvestmentPossible() const;
    bool isRecommissioningPossible() const;
    bool maxOscillationReached() const;
    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& candidateName);
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& candidateName);
    void updateRentabilityWithProblem(const CapacityAction action,
                                      const std::shared_ptr<Problem> problem,
                                      const PbOutput pbOutput,
                                      const std::vector<double> solution);
    template<typename T>
    void computeRentabilityForCandidates(const std::map<std::string, Candidate<T>>& candidates,
                                         const CapacityAction action,
                                         const std::shared_ptr<Problem> problem,
                                         const PbOutput pbOutput,
                                         const std::vector<double> solution);
    std::string selectBestClusterFromRentability(CapacityAction action);

    template<typename T>
    double extraCost(const Candidate<T>& candidate)
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
    void initializeRentability(const std::map<std::string, Candidate<T>>& candidates,
                               const CapacityAction& action)
    {
        candidatesRentability.clear();
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
            candidatesRentability[candidateName] -= extraCost(candidate);
        }
    }
};

class BalancingParser
{
public:
    BalancingParser(const std::filesystem::path& pathToYamlConfigFile = "");

    void parse();

    Benders::Criterion::Type getReliabilityStandardIndicator() const;

    std::map<std::string, Area> areas;

private:
    std::filesystem::path pathToYamlConfigFile;
    YAML::Node config;

    double defaultReliabilityStandardDeadBandUp;
    double defaultReliabilityStandardDeadBandDown;
    Benders::Criterion::Type reliabilityStandardIndicator;

    std::map<std::string, std::shared_ptr<DecommissioningCandidateType>>
      decommissioningCandidatesTypes;
    std::map<std::string, std::shared_ptr<InvestmentCandidateType>> investmentCandidatesTypes;

    void parseGlobalSettings();
    void parseAreasSettings();
    void parseDecommissioningCandidatesTypes();
    void parseInvestmentCandidatesTypes();
};
