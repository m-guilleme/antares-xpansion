#include "antares-xpansion/benders/benders_core/CriterionComputation.h"

namespace Benders::Criterion
{
void CriterionComputation::SearchVariables(const std::vector<std::string>& variables)
{
    Benders::Criterion::VariablesGroup variablesGroup(variables, criterion_input_data_.Criteria());
    indices_ = variablesGroup.Indices();
}

void CriterionComputation::SearchConstraints(const std::vector<std::string>& constraints)
{
    Benders::Criterion::ConstraintsGroup constraintsGroup(constraints,
                                                          criterion_input_data_.Criteria());
    indices_ = constraintsGroup.Indices();
}

const CriterionInputData& CriterionComputation::getCriterionInputData() const
{
    return criterion_input_data_;
}

void CriterionComputation::SetCriterionCountThreshold(double count_threshold)
{
    criterion_input_data_.SetCriterionCountThreshold(count_threshold);
}

std::vector<std::vector<int>>& CriterionComputation::getIndices()
{
    return indices_;
}

CriterionComputation::CriterionComputation(const CriterionInputData& criterion_input_data):
    criterion_input_data_(criterion_input_data)
{
}
} // namespace Benders::Criterion
