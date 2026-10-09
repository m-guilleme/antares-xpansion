#pragma once

#include "CriterionComputation.h"

namespace Benders::Criterion
{

class CriterionNPCAP final: public CriterionComputation
{
public:
    explicit CriterionNPCAP() = default;

    explicit CriterionNPCAP(const CriterionInputData& criterion_input_data,
                            std::shared_ptr<SolverAbstract> problem);

    void ComputeCriterion(std::shared_ptr<SolverAbstract> problem,
                          double subproblem_weight,
                          std::vector<double>& criteria,
                          std::vector<double>& patterns_values) override;

    ~CriterionNPCAP() override = default;

    std::map<std::string, double> criterionThreasholdByArea;
};
} // namespace Benders::Criterion
