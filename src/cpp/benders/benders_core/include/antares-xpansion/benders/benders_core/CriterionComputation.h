#pragma once

#include "ConstraintsGroup.h"
#include "CriterionInputDataReader.h"
#include "VariablesGroup.h"
#include "antares-xpansion/multisolver_interface/SolverAbstract.h"

namespace Benders::Criterion
{

class CriterionComputation
{
public:
    /**
     * @brief  default constructor
     */
    explicit CriterionComputation() = default;

    /**
     * @brief Constructs a CriterionComputation object.
     *
     * This constructor initializes the CriterionComputation instance with the
     * provided criterion input data.
     *
     * @param criterion_input_data The input data to be used for criterion
     * computation.
     */
    explicit CriterionComputation(const CriterionInputData& criterion_input_data);

    /**
     * @brief Searches for relevant variables based on the provided variable
     * names.
     *
     * This method initializes a VariablesGroup with the provided variable names
     * and retrieves the indices of these variables for later computation.
     *
     * @param variables A vector of strings representing the variable names to
     * search for.
     */
    void SearchVariables(const std::vector<std::string>& variables);

    /**
     * @brief Searches for relevant constraints based on the provided constraint
     * names.
     *
     * This method initializes a VariablesGroup with the provided constraint names
     * and retrieves the indices of these constraints for later computation.
     *
     * @param constraints A vector of strings representing the constraint names to
     * search for.
     */
    void SearchConstraints(const std::vector<std::string>& constraints);

    /**
     * @brief Computes the  criteria based on subproblem solutions.
     *
     * This method calculates the criterion criteria and pattern values
     * based on the provided subproblem weight and solution. It updates the
     * criteria and patterns values vectors accordingly.
     *
     * @param subproblem_weight The weight of the subproblem affecting the
     * criteria.
     * @param sub_problem_solution A vector containing the solution values of
     * the subproblem.
     * @param criteria A reference to a vector where the computed
     * criteria will be stored.
     * @param patterns_values A reference to a vector where the computed
     * pattern values will be stored.
     */
    void ComputeCriterion(double subproblem_weight,
                          const std::vector<double>& sub_problem_solution,
                          std::vector<double>& criteria,
                          std::vector<double>& patterns_values);

    /**
     * @brief Computes the criteria directly from an already solved problem.
     *
     * Default behaviour: retrieves the primal solution of the problem and
     * calls ComputeCriterion. Criterion-specific classes (LOLE, UNSPENERG,
     * NPCAP) override it.
     *
     * @param problem The already solved problem
     * @param subproblem_weight The weight of the subproblem affecting the
     * criteria.
     * @param criteria A reference to a vector where the computed
     * criteria will be stored.
     * @param patterns_values A reference to a vector where the computed
     * pattern values will be stored.
     */
    virtual void ComputeCriterionFromProblem(std::shared_ptr<SolverAbstract> problem,
                                             double subproblem_weight,
                                             std::vector<double>& criteria,
                                             std::vector<double>& patterns_values);

    /**
     * @brief Retrieves the variable indices.
     *
     * This method returns a reference to the vector containing the indices of
     * the variables associated with this CriterionComputation instance.
     *
     * @return A reference to the vector of variable indices.
     */
    std::vector<std::vector<int>>& getVarIndices();

    /**
     * @brief Retrieves the criterion input data.
     *
     * This method returns a constant reference to the criterion input data
     * associated with this CriterionComputation instance.
     *
     * @return A constant reference to the CriterionInputData object.
     */
    const CriterionInputData& getCriterionInputData() const;

    bool IsEmpty() const
    {
        return criterion_input_data_.Criteria().empty();
    }

    void SetCriterionCountThreshold(double count_threshold);

    virtual ~CriterionComputation() = default;

protected:
    std::vector<std::vector<int>> var_indices_ = {};
    CriterionInputData criterion_input_data_;
};
} // namespace Benders::Criterion
