#pragma once

#include "CriterionComputation.h"

namespace Benders::Criterion
{

class CriterionLOLE final: public CriterionComputation
{
public:
    explicit CriterionLOLE() = default;

    explicit CriterionLOLE(const CriterionInputData& criterion_input_data);

    // this constructor allows for one-time initialisation of var_indices_
    explicit CriterionLOLE(const CriterionInputData& criterion_input_data,
                           std::shared_ptr<SolverAbstract> problem);
    ~CriterionLOLE() override = default;
};
} // namespace Benders::Criterion
