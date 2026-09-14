
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"

#include <fmt/core.h>
#include <sstream>
#include <tbb/global_control.h>
#include <tbb/parallel_for_each.h>
#include <unordered_set>

#include "antares-xpansion/benders/benders_core/CriterionLOL.h"
#include "antares-xpansion/benders/benders_core/CriterionNPCAP.h"
#include "antares-xpansion/helpers/Timer.h"

using namespace PlainData;

/// @brief Constructor of the GreedyBalancingFinder class
/// @param logger The logger to use for the evaluation
/// @param areaSettings The area investments to use for the evaluation
/// @param criterion The criterion to evaluate
/// @param problemManager The problemManager holding all problems to evaluate on
/// @param solverName The name of the solver to use for the evaluation
/// @param nbThreads The number of threads to use for the evaluation
GreedyBalancingFinder::GreedyBalancingFinder(
  Logger logger,
  const std::map<std::string, AreaSettings>& areaSettings,
  Benders::Criterion::Type criterion,
  std::shared_ptr<ProblemManager> problemManager,
  std::string solverName,
  std::filesystem::path studyDir,
  int nbThreads):
    Evaluator(logger, problemManager, studyDir, solverName, nbThreads),
    areaSettings(areaSettings)
{
    auto criterionInputData = buildPatterns(criterion, areaSettings);
    setCriterionComputationInputs(criterionInputData);
}

/// @brief Build the patterns to use for the criterion computation
/// @param criterion The criterion to evaluate
/// @param areaSettings The area investments to use for the evaluation
/// @return The criterion input data containing the patterns to use for the criterion computation
Benders::Criterion::CriterionInputData GreedyBalancingFinder::buildPatterns(
  Benders::Criterion::Type criterion,
  const std::map<std::string, AreaSettings>& areaSettings)
{
    Benders::Criterion::CriterionInputData ret{criterion};
    for (const auto& area: areaSettings | std::views::keys)
    {
        Benders::Criterion::CriterionSingleInputData singleInputData(getPrefix(criterion), area, 1);
        ret.AddSingleData(singleInputData);
    }

    return ret;
}

/// @brief Get the indices of the area balance constraints in the problem
/// @param subProblem The problem to get the indices from
/// @return The indices of the area balance constraints in the problem
std::vector<size_t> GreedyBalancingFinder::getAreaBalanceIndices(
  std::shared_ptr<Problem> subProblem)
{
    const auto& constraints = subProblem->get_row_names();

    constexpr std::string_view prefix = "AreaBalance::area";
    constexpr std::string_view hourTag = "::hour";

    std::unordered_set<std::string_view> areas;
    for (const auto& [name, _]: areaSettings)
    {
        areas.insert(name);
    }

    std::vector<size_t> indices;

    for (std::size_t i = 0; i < constraints.size(); ++i)
    {
        std::string_view v = constraints[i];
        if (!v.starts_with(prefix))
        {
            continue;
        }

        v.remove_prefix(prefix.size());
        auto area = v.substr(1, v.find(hourTag) - 2); // remove also the angled brackets from <area>

        if (areas.contains(area))
        {
            indices.push_back(i);
        }
    }

    return indices;
}

/// @brief Fill the area criterion values and prices from the subproblem results
/// @param res The subproblem results containing criteria values
/// @param dualValuesCst The dual values of the constraints
/// @param cstIndices The indices of the area balance constraints
/// @param output The output to fill
void GreedyBalancingFinder::fillAreaCriterionValuesAndPrices(
  const std::vector<double>& criteria,
  const std::vector<double>& dualValuesCst,
  const std::vector<size_t>& cstIndices,
  PbOutput& output)
{
    for (std::size_t i = 0; i < criteria.size(); ++i)
    {
        double value = criteria[i];
        std::string area = criterion_computation_->getCriterionInputData().PatternBodies()[i];
        output.areaCriterionValues[area] += value;

        for (size_t j = 0; j < NUMBER_OF_HOURS_PER_WEEK; j++)
        {
            output.areaPrices[area][j] = dualValuesCst[cstIndices[i * NUMBER_OF_HOURS_PER_WEEK
                                                                  + j]];
        }
    }
}

/// @brief Process a single subproblem
/// @param subProblemId the id of the problem to treat
/// @param subProblem the problem to treat
void GreedyBalancingFinder::ProcessSubproblem(const Antares::Solver::WeeklyProblemId subProblemId,
                                              std::shared_ptr<Problem> subProblem)
{
    Timer timer;
    totalPbModifTimer += timer.elapsed();
    SubProblemData res = SolveSubproblem(subProblem);

    criterion_computation_->ComputeCriterion(subProblem, 1, res.criteria, res.patterns_values);

    std::vector<double> dualValuesCst(subProblem->get_nrows());
    subProblem->get_lp_sol(NULL, dualValuesCst.data(), NULL);
    const auto cstIndices = getAreaBalanceIndices(subProblem);

    PbOutput output{};
    fillAreaCriterionValuesAndPrices(res.criteria, dualValuesCst, cstIndices, output);

    balancingResults.insert(subProblemId, output);
    logger->display_message((std::stringstream() << "Cost: " << res.subproblem_cost).str(),
                            LogUtils::LOGLEVEL::DEBUG,
                            BALANCING_EVALUATOR_LOGGER_CONTEXT);
}

/// @brief Compute the criterion and the price for each subproblem
/// @return A map associating each subproblem id to the computed criterion and price
std::map<Antares::Solver::WeeklyProblemId, PbOutput>
GreedyBalancingFinder::ComputeCriterionAndPrice()
{
    logger->display_message(
      (std::stringstream() << "Launching criterion and price evaluation").str(),
      LogUtils::LOGLEVEL::DEBUG,
      BALANCING_EVALUATOR_LOGGER_CONTEXT);

    Timer run_timer;

    Run();

    auto run_time = run_timer.elapsed();
    logger->display_message(
      (std::stringstream() << "Evaluation done in " << run_time << " seconds").str(),
      LogUtils::LOGLEVEL::DEBUG,
      BALANCING_EVALUATOR_LOGGER_CONTEXT);
    logger->display_message((std::stringstream()
                             << "Time solving subproblems (accumulated by each thread) : "
                             << totalSubPbTimer << " seconds")
                              .str(),
                            LogUtils::LOGLEVEL::DEBUG,
                            BALANCING_EVALUATOR_LOGGER_CONTEXT);

    return balancingResults.get();
}

void GreedyBalancingFinder::setCriterionComputationInputs(
  const Benders::Criterion::CriterionInputData& criterion_input_data)
{
    using enum Benders::Criterion::Type;
    if (problemManager->getProblemIds().empty())
    {
        throw std::runtime_error("No problems available");
    }
    auto problem = problemManager->getFirstProblem();

    switch (criterion_input_data.criterion)
    {
    case UnsuppliedEnergy:
        criterion_computation_ = std::make_unique<Benders::Criterion::CriterionLOL>(
          criterion_input_data,
          problem);
        break;
    case NearPriceCapHours:
        criterion_computation_ = std::make_unique<Benders::Criterion::CriterionNPCAP>(
          criterion_input_data,
          problem);
        break;
    default:
        criterion_computation_.reset();
        break;
    }
}
