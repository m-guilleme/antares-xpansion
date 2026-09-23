#pragma once

#include "CriterionComputation.h"

namespace Benders::Criterion
{

class CriterionUNSPENERG final: public CriterionComputation
{
public:
    explicit CriterionUNSPENERG() = default;

    explicit CriterionUNSPENERG(const CriterionInputData& criterion_input_data);

    // this constructor allows for one-time initialisation of indices_
    explicit CriterionUNSPENERG(const CriterionInputData& criterion_input_data,
                                std::shared_ptr<SolverAbstract> problem);

    void ComputeCriterion(std::shared_ptr<SolverAbstract> problem,
                          double subproblem_weight,
                          std::vector<double>& criteria,
                          std::vector<double>& patterns_values) override;
    ~CriterionUNSPENERG() override = default;
};
} // namespace Benders::Criterion
