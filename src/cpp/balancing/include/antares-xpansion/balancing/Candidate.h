#pragma once
#include <map>
#include <memory>
#include <string>

#include <antares/api/solver.h>

#include "antares-xpansion/benders/benders_core/CriterionInputDataReader.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

constexpr int NUMBER_OF_HOURS_PER_WEEK = 168;

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

struct InvestmentCandidateType
{
    double derating, expansionPotential, investmentCost, fixedOmCosts;

    double maxCapacity() const
    {
        return expansionPotential;
    }

    double unitCost() const
    {
        return investmentCost + fixedOmCosts;
    }
};

struct DecommissioningCandidateType
{
    double decommissioningPotential, decommissioningCost, fixedOmCosts;

    double maxCapacity() const
    {
        return decommissioningPotential;
    }

    double unitCost() const
    {
        return decommissioningCost + fixedOmCosts;
    }
};

struct BoundData
{
    double upBoundRatioToInstalledCap, lowBoundRatioToUpBound;
};

struct CandidateBase
{
    double installedCapacity;
    double previousInstalledCapacity;
    double initInstalledCapacity;
    double marginalCost;
    std::optional<CapacityAction> lastAction;
    int oscillationCounter = 0;
    std::array<size_t, NUMBER_OF_HOURS_PER_WEEK> dispProdVarIndices;
    std::map<Antares::Solver::WeeklyProblemId, std::vector<BoundData>> boundsData;

    void setOneWeekBoundsData(Antares::Solver::WeeklyProblemId pbId,
                              std::vector<BoundData> oneWeekBoundsData);
    void updateOscillationStatus(const CapacityAction& action);
    void prepareBoundsData(const std::set<Antares::Solver::WeeklyProblemId>& pbIds);
    void computeAndSetOneWeekCapacityData(Antares::Solver::WeeklyProblemId pbId,
                                          const std::shared_ptr<Problem>& problem);
    void updateCapacitiesFromBounds();
};

template<typename T>
struct Candidate: CandidateBase
{
    std::shared_ptr<T> type;
    Candidate() = default;

    explicit Candidate(std::shared_ptr<T> t):
        CandidateBase{},
        type(std::move(t))
    {
    }
};
