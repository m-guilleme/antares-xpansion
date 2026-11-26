#pragma once
#include <regex>
#include <string>
#include <vector>

#include "CriterionInputDataReader.h"

namespace Benders::Criterion
{
class ConstraintsGroup
{
public:
    explicit ConstraintsGroup(
      const std::vector<std::string>& all_constraints,
      const std::vector<CriterionSingleInputData>& criterion_single_input_data);

    [[nodiscard]] std::vector<std::vector<int>> Indices() const;

private:
    void Search();
    const std::vector<std::string>& all_constraints_;
    const std::vector<CriterionSingleInputData>& criterion_single_input_data_;
    std::vector<std::vector<int>> indices_;
};
} // namespace Benders::Criterion
