#pragma once

#include "CriterionComputation.h"

namespace Benders::Criterion
{

class CriterionLOLE final: public CriterionComputation
{
public:
    explicit CriterionLOLE() = default;

    explicit CriterionLOLE(const CriterionInputData& criterion_input_data);

    // this constructor allows for one-time initialisation of indices_
    explicit CriterionLOLE(const CriterionInputData& criterion_input_data,
                           std::shared_ptr<SolverAbstract> problem);

    void ComputeCriterion(std::shared_ptr<SolverAbstract> problem,
                          double subproblem_weight,
                          std::vector<double>& criteria,
                          std::vector<double>& patterns_values) override;
    ~CriterionLOLE() override = default;
};
} // namespace Benders::Criterion
