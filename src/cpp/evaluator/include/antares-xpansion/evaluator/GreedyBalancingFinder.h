#pragma once

#include <antares/solver/lps/LpsFromAntares.h>

#include "antares-xpansion/balancing/BalancingParser.h"
#include "antares-xpansion/benders/benders_core/CriterionComputation.h"
#include "antares-xpansion/evaluator/Evaluator.h"
#include "antares-xpansion/xpansion_interfaces/ILogger.h"

constexpr char BALANCING_EVALUATOR_LOGGER_CONTEXT[] = "GreedyBalancingFinder";

using namespace PlainData;

class GreedyBalancingFinder: public Evaluator
{
public:
    GreedyBalancingFinder(Logger logger,
                          const std::map<std::string, Area>& areas,
                          Benders::Criterion::Type criterion,
                          std::shared_ptr<ProblemManager> problemManager,
                          std::string solverName,
                          std::filesystem::path studyDir,
                          int nbThreads = 1);

    std::map<Antares::Solver::WeeklyProblemId, PbOutput> computeCriterionAndPrice(
      const Antares::Solver::WeeklyProblemId problemId,
      std::shared_ptr<Problem> problem);
    void setCriterionComputationInputs(
      const Benders::Criterion::CriterionInputData& criterion_input_data);
    std::map<Antares::Solver::WeeklyProblemId, PbOutput> getBalancingResults();

private:
    std::unique_ptr<Benders::Criterion::CriterionComputation> criterion_computation_;
    Output::ConcurrentInsertionMap<Antares::Solver::WeeklyProblemId, PbOutput> balancingResults;

    Benders::Criterion::CriterionInputData buildPatterns(Benders::Criterion::Type criterion,
                                                         const std::map<std::string, Area>& areas);
    std::vector<size_t> getAreaBalanceIndices(std::shared_ptr<Problem> subProblem);
    const std::map<std::string, Area>& areas;
    void fillAreaCriterionValuesAndPrices(const std::vector<double>& criteria,
                                          const std::vector<double>& dualValuesCst,
                                          const std::vector<size_t>& cstIndices,
                                          PbOutput& output);

protected:
    void ProcessSubproblem(const Antares::Solver::WeeklyProblemId,
                           std::shared_ptr<Problem> subProblem) override;
};
