#pragma once
#include "antares-xpansion/balancing/Candidate.h"

struct PbOutput
{
    std::map<std::string, int> areaCriterionValues{};
    std::map<std::string, std::array<double, NUMBER_OF_HOURS_PER_WEEK>>
      areaPrices{}; // Dual value of AreaBalance Constraint
};

struct Area
{
public:
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
    CriterionState oldCriterionState{CriterionState::UNINITIALIZED};
    CriterionState criterionState{CriterionState::UNINITIALIZED};
    double avgCriteria;
    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& candidateName);
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& candidateName);
    bool maxOscillationReached() const;
    std::map<std::string, double> getCandidatesRentability() const;
    bool isCandidatesRentabilityEmpty() const;
    bool isInvestmentPossible() const;
    bool isDecommissioningPossible() const;
    bool isDisinvestmentPossible() const;
    bool isRecommissioningPossible() const;
    void initializeRentability(const CapacityAction& action);
    std::string selectCandidateWithBestRentability(CapacityAction action);
    void updateRentabilityWithProblem(const CapacityAction action,
                                      const PbOutput pbOutput,
                                      const std::vector<double> solution);

private:
    std::map<std::string, double> candidatesRentability;

    template<typename T>
    void computeRentabilityForCandidates(const std::map<std::string, Candidate<T>>& candidates,
                                         const CapacityAction action,
                                         const PbOutput pbOutput,
                                         const std::vector<double> solution);

    template<typename T>
    double extraCost(const Candidate<T>& candidate);
    void initializeInvestmentRentability(const CapacityAction& action);
    void initializeDecommissioningRentability(const CapacityAction& action);
    template<typename T>
    void initializeRentabilityImpl(const std::map<std::string, Candidate<T>>& candidates,
                                   const CapacityAction& action);
};
