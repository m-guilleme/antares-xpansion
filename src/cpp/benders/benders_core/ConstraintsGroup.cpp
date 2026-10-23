#include "include/antares-xpansion/benders/benders_core/ConstraintsGroup.h"

using namespace Benders::Criterion;

/**
 * @file ConstraintsGroup.cpp
 * @brief Implementation of the ConstraintsGroup class.
 *
 * This file contains the implementation of the ConstraintsGroup class,
 * which is responsible for grouping constraints based on provided input patterns.
 */

ConstraintsGroup::ConstraintsGroup(
  const std::vector<std::string>& all_constraints,
  const std::vector<CriterionSingleInputData>& criterion_single_input_data):
    all_constraints_(all_constraints),
    criterion_single_input_data_(criterion_single_input_data)
{
    Search();
}

std::vector<std::vector<int>> ConstraintsGroup::Indices() const
{
    return indices_;
}

void ConstraintsGroup::Search()
{
    indices_.assign(criterion_single_input_data_.size(), {});
    int pattern_index(0);
    for (const auto& single_input_data: criterion_single_input_data_)
    {
        auto pattern = single_input_data.Pattern().Value();
        int cst_index(0);
        for (const auto& constraint: all_constraints_)
        {
            if (constraint.starts_with(pattern))
            {
                indices_[pattern_index].push_back(cst_index);
            }
            ++cst_index;
        }
        ++pattern_index;
    }
}
