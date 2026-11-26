#include "antares-xpansion/benders/benders_core/CriterionLOLE.h"

namespace Benders::Criterion
{
CriterionLOLE::CriterionLOLE(const CriterionInputData& criterion_input_data):
    CriterionComputation(criterion_input_data)
{
}

CriterionLOLE::CriterionLOLE(const CriterionInputData& criterion_input_data,
                             std::shared_ptr<SolverAbstract> problem):
    CriterionComputation(criterion_input_data)
{
    const auto col_names = problem->get_col_names();
    SearchVariables(col_names);
}
} // namespace Benders::Criterion
