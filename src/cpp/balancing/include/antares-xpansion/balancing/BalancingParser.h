#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include <antares/api/solver.h>

#include "antares-xpansion/benders/benders_core/CriterionInputDataReader.h"

enum class CriterionState
{
    LOWER,
    VALID,
    HIGHER,
    UNINITIALIZED,
};

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
    std::map<Antares::Solver::WeeklyProblemId, std::vector<BoundData>> boundsData;

    void setOneWeekBoundsData(Antares::Solver::WeeklyProblemId pbId,
                              std::vector<BoundData> oneWeekBoundsData)
    {
        boundsData[pbId] = oneWeekBoundsData;
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
    int maxOscillation;
    std::map<std::string, Candidate<DecommissioningCandidateType>> decommissioningCandidates;
    std::map<std::string, Candidate<InvestmentCandidateType>> investmentCandidates;
    CriterionState oldCriterionState{CriterionState::UNINITIALIZED};

    bool isInvestmentPossible() const;
    bool isDecommissioningPossible() const;
    bool isDisinvestmentPossible() const;
    bool isRecommissioningPossible() const;
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
