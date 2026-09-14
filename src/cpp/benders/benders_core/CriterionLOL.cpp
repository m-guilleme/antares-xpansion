#include "antares-xpansion/benders/benders_core/CriterionLOL.h"

namespace Benders::Criterion
{
CriterionLOL::CriterionLOL(const CriterionInputData& criterion_input_data):
    CriterionComputation(criterion_input_data)
{
}

CriterionLOL::CriterionLOL(const CriterionInputData& criterion_input_data,
                           std::shared_ptr<SolverAbstract> problem):
    CriterionComputation(criterion_input_data)
{
    const auto col_names = problem->get_col_names();
    SearchVariables(col_names);
}

void CriterionLOL::ComputeCriterion(std::shared_ptr<SolverAbstract> problem,
                                    double subproblem_weight,
                                    std::vector<double>& criteria,
                                    std::vector<double>& patterns_values)
{
    // this check still exists in case a problem hasn't been passed to the constructor
    if (indices_.empty())
    {
        const auto col_names = problem->get_col_names();
        SearchVariables(col_names);
    }

    std::vector<double> varValues(problem->get_ncols());
    problem->get_lp_sol(varValues.data(), NULL, NULL);

    auto criteria_input_size = static_cast<int>(indices_.size()); // num of patterns
    criteria.resize(criteria_input_size, 0.);
    patterns_values.resize(criteria_input_size, 0.);

    double criterion_count_threshold = criterion_input_data_.CriterionCountThreshold();

    for (int pattern_index(0); pattern_index < criteria_input_size; ++pattern_index)
    {
        auto pattern_indices = indices_[pattern_index];
        double pattern_value = patterns_values[pattern_index];
        double criteria_value = criteria[pattern_index];
        for (auto index: pattern_indices)
        {
            const auto solution = varValues[index];
            pattern_value += solution;
            if (solution > criterion_count_threshold)
            {
                // 1h were criterion is satisfied
                criteria_value += subproblem_weight;
            }
        }
        patterns_values[pattern_index] = pattern_value;
        criteria[pattern_index] = criteria_value;
    }
}
} // namespace Benders::Criterion
