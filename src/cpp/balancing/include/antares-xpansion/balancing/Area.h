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
    int maxOscillation = 5;
    std::map<std::string, Candidate<DecommissioningCandidateType>> decommissioningCandidates;
    std::map<std::string, Candidate<InvestmentCandidateType>> investmentCandidates;
    CriterionState oldCriterionState{CriterionState::UNINITIALIZED};
    CriterionState criterionState{CriterionState::UNINITIALIZED};
    double avgCriteria;

    /// @brief Get an investment candidate of the area from its name
    /// @param candidateName The name of the investment candidate
    /// @return A reference to the investment candidate
    Candidate<InvestmentCandidateType>& getInvestmentCandidate(const std::string& candidateName);

    /// @brief Get a decommissioning candidate of the area from its name
    /// @param candidateName The name of the decommissioning candidate
    /// @return A reference to the decommissioning candidate
    Candidate<DecommissioningCandidateType>& getDecommissioningCandidate(
      const std::string& candidateName);

    /// @brief Check whether the maximum number of oscillations allowed for the area has been
    /// reached
    /// @return true if the maximum oscillation has been reached, false otherwise
    bool maxOscillationReached() const;

    /// @brief Get the current rentability of the candidates
    /// @return A map associating each candidate name to its rentability
    std::map<std::string, double> getCandidatesRentability() const;

    /// @brief Check whether no candidate rentability has been computed
    /// @return true if there is no candidate rentability, false otherwise
    bool isCandidatesRentabilityEmpty() const;

    /// @brief Check whether an investment action can be applied in the area
    /// @return true if an investment is possible, false otherwise
    bool isInvestmentPossible() const;

    /// @brief Check whether a decommissioning action can be applied in the area
    /// @return true if a decommissioning is possible, false otherwise
    bool isDecommissioningPossible() const;

    /// @brief Check whether a disinvestment action can be applied in the area
    /// @return true if a disinvestment is possible, false otherwise
    bool isDisinvestmentPossible() const;

    /// @brief Check whether a recommissioning action can be applied in the area
    /// @return true if a recommissioning is possible, false otherwise
    bool isRecommissioningPossible() const;

    /// @brief Initialize the rentability of the candidates concerned by an action
    /// @param action The capacity action to initialize the rentability for
    void initializeRentability(const CapacityAction& action);

    /// @brief Select the candidate with the best rentability for an action
    /// @param action The capacity action to select a candidate for
    /// @return The name of the candidate with the best rentability
    std::string selectCandidateWithBestRentability(CapacityAction action);

    /// @brief Update the candidates rentability with the results of one problem
    /// @param action The capacity action the rentability is computed for
    /// @param pbOutput The criterion values and prices computed for the problem
    /// @param solution The solution of the problem
    void updateRentabilityWithProblem(const CapacityAction action,
                                      const PbOutput pbOutput,
                                      const std::vector<double> solution);

    /// @brief Check decom candidates type against capacities retrieve from problems
    void checkDecomCandidatesTypeAgainstDecomCandidateCapacities() const;

    /// @brief Get the criterion value above which the area is considered not balanced
    /// @return The higher threshold, derived from the reliability standard and its dead band
    double higherThreshold() const;

    /// @brief Get the criterion value below which the area is considered not balanced
    /// @return The lower threshold, derived from the reliability standard and its dead band
    double lowerThreshold() const;

private:
    std::map<std::string, double> candidatesRentability;

    /// @brief Add to the candidates rentability the contribution of one problem
    /// @param candidates The candidates to compute the rentability for
    /// @param action The capacity action the rentability is computed for
    /// @param pbOutput The criterion values and prices computed for the problem
    /// @param solution The solution of the problem
    template<typename T>
    void computeRentabilityForCandidates(const std::map<std::string, Candidate<T>>& candidates,
                                         const CapacityAction action,
                                         const PbOutput pbOutput,
                                         const std::vector<double> solution);

    /// @brief Compute the extra cost of the rentability of a candidate
    /// @param candidate The candidate to compute the extra cost for
    /// @return The extra cost of the rentability of a candidate
    template<typename T>
    double extraCost(const Candidate<T>& candidate);

    /// @brief Initialize the rentability of the investment candidates
    /// @param action The capacity action to initialize the rentability for
    void initializeInvestmentRentability(const CapacityAction& action);

    /// @brief Initialize the rentability of the decommissioning candidates
    /// @param action The capacity action to initialize the rentability for
    void initializeDecommissioningRentability(const CapacityAction& action);

    /// @brief Initialize the rentability of a set of candidates
    /// @param candidates The candidates to initialize the rentability for
    /// @param action The capacity action to initialize the rentability for
    template<typename T>
    void initializeRentabilityImpl(const std::map<std::string, Candidate<T>>& candidates,
                                   const CapacityAction& action);
};
