#pragma once

#include <antares/solver/lps/LpsFromAntares.h>

#include "antares-xpansion/balancing/BalancingParser.h"
#include "antares-xpansion/benders/benders_core/CriterionComputation.h"
#include "antares-xpansion/evaluator/Evaluator.h"
#include "antares-xpansion/lpnamer/model/Problem.h"
#include "antares-xpansion/xpansion_interfaces/ILogger.h"

constexpr char BALANCING_EVALUATOR_LOGGER_CONTEXT[] = "GreedyBalancingFinder";
constexpr int NUMBER_OF_HOURS_PER_WEEK = 168;

using namespace PlainData;

struct PbOutput
{
    std::map<std::string, int> areaCriterionValues{};
    std::map<std::string, std::array<double, NUMBER_OF_HOURS_PER_WEEK>>
      areaPrices{}; // Dual value of AreaBalance Constraint
};

class GreedyBalancingFinder: public Evaluator
{
public:
    GreedyBalancingFinder(Logger logger,
                          const std::map<std::string, AreaSettings>& areaSettings,
                          Benders::Criterion::Type criterion,
                          std::shared_ptr<ProblemManager> problemManager,
                          std::string solverName,
                          std::filesystem::path studyDir,
                          int nbThreads = 1);

    std::map<Antares::Solver::WeeklyProblemId, PbOutput> ComputeCriterionAndPrice();
    void setCriterionComputationInputs(
      const Benders::Criterion::CriterionInputData& criterion_input_data);

private:
    std::unique_ptr<Benders::Criterion::CriterionComputation> criterion_computation_;
    Output::ConcurrentInsertionMap<Antares::Solver::WeeklyProblemId, PbOutput> balancingResults;

    Benders::Criterion::CriterionInputData buildPatterns(
      Benders::Criterion::Type criterion,
      const std::map<std::string, AreaSettings>& areaSettings);
    std::vector<size_t> getAreaBalanceIndices(std::shared_ptr<Problem> subProblem);
    const std::map<std::string, AreaSettings>& areaSettings;
    void fillAreaCriterionValuesAndPrices(const std::vector<double>& criteria,
                                          const std::vector<double>& dualValuesCst,
                                          const std::vector<size_t>& cstIndices,
                                          PbOutput& output);

protected:
    void ProcessSubproblem(const Antares::Solver::WeeklyProblemId,
                           std::shared_ptr<Problem> subProblem) override;
};
