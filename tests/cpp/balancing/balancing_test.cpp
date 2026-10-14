#include "antares-xpansion/lpnamer/main/Balancing.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "RandomDirGenerator.h"
#include "antares-xpansion/balancing/InputBalancingParser.h"
#include "antares-xpansion/balancing/SettingsConfigReader.h"
#include "antares-xpansion/bellman_values/ProblemManager.h"
#include "antares-xpansion/benders/logger/FilteredLogger.h"
#include "antares-xpansion/benders/logger/Master.h"
#include "antares-xpansion/benders/logger/User.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "gtest/gtest.h"

namespace
{
using HourlyIndices = std::array<size_t, NUMBER_OF_HOURS_PER_WEEK>;

// Size of the dummy solution vector: big enough to cover candidate indices in each study
constexpr size_t SOLUTION_SIZE = 3100;

bool isInvestmentAction(const CapacityAction& action)
{
    return action == CapacityAction::INVESTMENT || action == CapacityAction::DISINVESTMENT;
}

// Calls func on the investment candidate (INVESTMENT / DISINVESTMENT) or on the
// decommissioning candidate (DECOMMISSIONING / RECOMMISSIONING) matching the action.
// func must be a generic lambda returning the same type for both candidate types.
template<typename Func>
auto visitCandidate(Balancing& pbg,
                    const std::string& areaName,
                    const std::string& candidateName,
                    const CapacityAction& action,
                    Func&& func)
{
    if (isInvestmentAction(action))
    {
        return func(pbg.getInvestmentCandidate(areaName, candidateName));
    }
    return func(pbg.getDecommissioningCandidate(areaName, candidateName));
}

HourlyIndices getDispProdVarIndices(Balancing& pbg,
                                    const std::string& areaName,
                                    const std::string& candidateName,
                                    const CapacityAction& action)
{
    return visitCandidate(pbg,
                          areaName,
                          candidateName,
                          action,
                          [](auto& candidate) -> HourlyIndices
                          { return candidate.dispProdVarIndices; });
}

std::shared_ptr<ProblemManager> makeXpressProblemManager()
{
    return std::make_shared<ProblemManager>("xpress", "mps", false, true, "initial_problems");
}

void assertCandidateBounds(const std::shared_ptr<ProblemManager>& problemManager,
                           const std::vector<int>& candidateIndices,
                           double expectedUpperBound,
                           double expectedLowerBound)
{
    for (const auto& pbId: problemManager->getProblemIds())
    {
        auto problem = problemManager->getProblemFromId(pbId);
        for (const int index: candidateIndices)
        {
            double upperBound;
            double lowerBound;
            problem->get_ub(&upperBound, index, index);
            problem->get_lb(&lowerBound, index, index);
            EXPECT_DOUBLE_EQ(upperBound, expectedUpperBound);
            EXPECT_DOUBLE_EQ(lowerBound, expectedLowerBound);
        }
    }
}
} // namespace

class BalancingTest: public ::testing::Test
{
protected:
    Logger logger;
    std::filesystem::path tmpDir;
    std::filesystem::path originalDir;
    const std::filesystem::path dataTestDir = "data_test/balancing";
    std::vector<std::filesystem::path> createdTmpDirs;

    void SetUp() override
    {
        Logger stdOutLogger = std::make_shared<xpansion::logger::User>(std::cerr);
        auto masterLogger = std::make_shared<xpansion::logger::Master>();
        masterLogger->addLogger(stdOutLogger);
        logger = std::make_shared<FilteredLogger>(masterLogger, LogUtils::LOGLEVEL::INFO);
        originalDir = std::filesystem::current_path();
    }

    void TearDown() override
    {
        std::filesystem::current_path(originalDir);
        std::error_code ec;
        for (const auto& dir: createdTmpDirs)
        {
            std::filesystem::remove_all(dir, ec);
        }
    }

    void copyStudyData(const std::string& studyName)
    {
        tmpDir = CreateRandomSubDir(std::filesystem::temp_directory_path());
        createdTmpDirs.push_back(tmpDir);

        std::filesystem::copy(dataTestDir / studyName,
                              tmpDir,
                              std::filesystem::copy_options::recursive
                                | std::filesystem::copy_options::overwrite_existing);
    }

    // Copies the study into a temporary directory and builds a ProblemGenerationForBalancing
    // from it. The balancing config is read from the copied study unless inputBalFilePath is set.
    Balancing makeProblemGeneration(
      const std::string& studyName,
      const std::shared_ptr<ProblemManager>& problemManager = std::make_shared<ProblemManager>(),
      const std::optional<std::filesystem::path>& inputBalFilePath = std::nullopt)
    {
        copyStudyData(studyName);

        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };
        InputBalancingParser balParser(
          inputBalFilePath.value_or(tmpDir / "user" / "balancing" / "input_balancing.yml"));
        std::map<std::string, Area> areas = balParser.parse();
        return Balancing(directories, areas, logger, problemManager);
    }

    void testFindAreaCandidatesToModify()
    {
        const std::vector<std::string> areasName = {"invest_area",
                                                    "desinvest_area",
                                                    "decom_area",
                                                    "recom_area"};
        const std::vector<std::string> candidatesName = {"candidate_1", "candidate_2"};

        // dummy data, areas are replaced by the real test data below
        auto pbg = makeProblemGeneration("one_candidate_per_area", makeXpressProblemManager());

        // set real test data
        InputBalancingParser balParser(dataTestDir / "find_area_cluster_to_modify"
                                       / "input_balancing.yml");
        pbg.areas = balParser.parse();

        constexpr double capacity = 1000;
        for (const auto& candidateName: candidatesName)
        {
            pbg.getInvestmentCandidate("desinvest_area", candidateName).installedCapacity
              = capacity;
            pbg.getDecommissioningCandidate("decom_area", candidateName).installedCapacity
              = capacity;
            pbg.getDecommissioningCandidate("recom_area", candidateName).initInstalledCapacity
              = capacity;
        }

        // set area lastAction
        pbg.areas.at("invest_area").lastAction = CapacityAction::INVESTMENT;
        pbg.areas.at("desinvest_area").lastAction = CapacityAction::INVESTMENT;
        pbg.areas.at("decom_area").lastAction = CapacityAction::DECOMMISSIONING;
        pbg.areas.at("recom_area").lastAction = CapacityAction::DECOMMISSIONING;

        // set problemManager solution and simulation values:
        // candidate_1 produces 1.0 every hour, candidate_2 produces 0.0
        std::vector<double> solution(NUMBER_OF_HOURS_PER_WEEK * areasName.size()
                                     * candidatesName.size());
        PbOutput pbOutput;
        pbOutput.areaCriterionValues = {{"invest_area", 5},
                                        {"desinvest_area", 2},
                                        {"decom_area", 2},
                                        {"recom_area", 5}};
        std::array<double, NUMBER_OF_HOURS_PER_WEEK> areaPrices;
        areaPrices.fill(1.0);

        size_t idx = 0;
        for (const auto& areaName: areasName)
        {
            pbOutput.areaPrices[areaName] = areaPrices;
            const bool isInvestmentArea = areaName == "invest_area" || areaName == "desinvest_area";
            for (const auto& candidateName: candidatesName)
            {
                HourlyIndices candidateIndices;
                for (auto& index: candidateIndices)
                {
                    solution.at(idx) = (candidateName == "candidate_1") ? 1.0 : 0.0;
                    index = idx++;
                }
                if (isInvestmentArea)
                {
                    pbg.getInvestmentCandidate(areaName, candidateName).dispProdVarIndices
                      = candidateIndices;
                }
                else
                {
                    pbg.getDecommissioningCandidate(areaName, candidateName).dispProdVarIndices
                      = candidateIndices;
                }
            }
        }
        Antares::Solver::WeeklyProblemId pbId({1, 1});
        pbg.problemManager->setProblemSolution(pbId, solution);
        std::map<Antares::Solver::WeeklyProblemId, PbOutput> simuValues = {{pbId, pbOutput}};

        // run
        pbg.updateAreaCriteriaData(simuValues);
        const auto areaCandidatesToModify = pbg.findAreaCandidatesToModify(simuValues);

        // we check that the correct candidate and action have been selected
        const std::map<std::string, std::pair<std::string, CapacityAction>>
          expectedModifications = {{"invest_area", {"candidate_1", CapacityAction::INVESTMENT}},
                                   {"desinvest_area",
                                    {"candidate_2", CapacityAction::DISINVESTMENT}},
                                   {"decom_area", {"candidate_2", CapacityAction::DECOMMISSIONING}},
                                   {"recom_area",
                                    {"candidate_1", CapacityAction::RECOMMISSIONING}}};
        for (const auto& [areaCandidate, action]: areaCandidatesToModify)
        {
            const auto& [expectedCandidate, expectedAction] = expectedModifications.at(
              areaCandidate.first);
            EXPECT_EQ(areaCandidate.second, expectedCandidate);
            EXPECT_EQ(action, expectedAction);
        }

        // we check that the criterion state have been correctly set
        const std::map<std::string, CriterionState> expectedCriterionStates = {
          {"invest_area", CriterionState::HIGHER},
          {"recom_area", CriterionState::HIGHER},
          {"desinvest_area", CriterionState::LOWER},
          {"decom_area", CriterionState::LOWER}};
        for (const auto& [areaName, expectedState]: expectedCriterionStates)
        {
            EXPECT_EQ(pbg.areas.at(areaName).oldCriterionState, expectedState);
        }
    }

    void testApplyActionToCandidate(const std::string& areaName,
                                    const std::string& candidateName,
                                    const CapacityAction& action,
                                    const double capacityIncrement,
                                    const double uBoundRatioToInstCap,
                                    const double lBoundRatioToUpBound,
                                    const double expectedUpperCapacity,
                                    const double expectedLowerCapacity)
    {
        auto pbg = makeProblemGeneration("with_decom_candidate");
        const auto& problemManager = pbg.problemManager;
        const bool isInvestment = isInvestmentAction(action);

        // set area increments
        auto& area = pbg.areas.at(areaName);
        area.currentInvestmentIncrement = capacityIncrement;
        if (!isInvestment)
        {
            area.currentDecommissioningIncrement = capacityIncrement;
        }

        // initInstalledCapacity is set lower than installedCapacity to allow disinvestment,
        // and higher to allow recommissioning
        const double installedCapacity = (isInvestment ? 6 : 2) * capacityIncrement;
        const double initInstalledCapacity = 4 * capacityIncrement;
        visitCandidate(pbg,
                       areaName,
                       candidateName,
                       action,
                       [&](auto& candidate)
                       {
                           candidate.installedCapacity = installedCapacity;
                           candidate.initInstalledCapacity = initInstalledCapacity;
                       });

        // set the bounds data of the candidate for each problem
        std::vector<BoundData> oneWeekBoundData(NUMBER_OF_HOURS_PER_WEEK);
        for (auto& boundData: oneWeekBoundData)
        {
            boundData.lowBoundRatioToUpBound = lBoundRatioToUpBound;
            boundData.upBoundRatioToInstalledCap = uBoundRatioToInstCap;
        }
        for (const auto& pbId: problemManager->getProblemIds())
        {
            visitCandidate(pbg,
                           areaName,
                           candidateName,
                           action,
                           [&](auto& candidate)
                           { candidate.setOneWeekBoundsData(pbId, oneWeekBoundData); });
        }

        const HourlyIndices varIndices = getDispProdVarIndices(pbg,
                                                               areaName,
                                                               candidateName,
                                                               action);
        const std::vector<int> vecIndices(varIndices.begin(), varIndices.end());

        // run applyActionToCandidate
        std::map<AreaCandidate, CapacityAction> areaCandidatesToModify = {
          {{areaName, candidateName}, action}};
        pbg.initializeCandidatesToModify(areaCandidatesToModify);
        for (const auto& pbId: problemManager->getProblemIds())
        {
            auto problem = problemManager->getProblemFromId(pbId);
            pbg.applyActionToCandidate(pbId, problem, areaCandidatesToModify);
        }

        // we check that new bound of candidate have been correctly set
        assertCandidateBounds(problemManager,
                              vecIndices,
                              expectedUpperCapacity,
                              expectedLowerCapacity);
    }

    void testComputeRentabilityForCandidates(const double hourlySolutionValue,
                                             const double hourlyAreaPrice,
                                             const double investmentCost,
                                             const double fixedOmCosts,
                                             const double marginalCost,
                                             const double installedCapacity,
                                             const CapacityAction& action,
                                             const double expectedRentability)
    {
        const std::string areaName = "area2";
        const bool isInvestment = isInvestmentAction(action);
        const std::string candidateName = isInvestment ? "invest_peak2" : "unprofitable_peak";

        auto pbg = makeProblemGeneration("with_decom_candidate", makeXpressProblemManager());

        // set test data
        visitCandidate(pbg,
                       areaName,
                       candidateName,
                       action,
                       [&](auto& candidate)
                       {
                           candidate.installedCapacity = installedCapacity;
                           candidate.type->fixedOmCosts = fixedOmCosts;
                           candidate.marginalCost = marginalCost;
                       });
        // the cost is named differently for investment and decommissioning candidates
        if (isInvestment)
        {
            pbg.getInvestmentCandidate(areaName, candidateName).type->investmentCost
              = investmentCost;
        }
        else
        {
            pbg.getDecommissioningCandidate(areaName, candidateName).type->decommissioningCost
              = investmentCost;
        }

        // candidate produces hourlySolutionValue every hour
        std::vector<double> solution(SOLUTION_SIZE, 0.0);
        for (const auto& idx: getDispProdVarIndices(pbg, areaName, candidateName, action))
        {
            solution.at(idx) = hourlySolutionValue;
        }
        PbOutput pbOutput;
        std::array<double, NUMBER_OF_HOURS_PER_WEEK> areaPrices;
        areaPrices.fill(hourlyAreaPrice);
        pbOutput.areaPrices[areaName] = areaPrices;

        // run
        auto& area = pbg.areas.at(areaName);
        area.initializeRentability(action);
        area.updateRentabilityWithProblem(action, pbOutput, solution);

        // we check that the rentability have been correctly computed
        EXPECT_DOUBLE_EQ(area.getCandidatesRentability().at(candidateName), expectedRentability);
    }

    void testGetNullRentabilityForCandidates(const std::string& studyFolderName,
                                             const std::string& areaName,
                                             const std::string& candidateName,
                                             const CapacityAction& action)
    {
        // all capacity data are set at the same value
        constexpr double capacityValue = 1000;

        auto pbg = makeProblemGeneration(studyFolderName);

        visitCandidate(pbg,
                       areaName,
                       candidateName,
                       action,
                       [&](auto& candidate)
                       {
                           candidate.installedCapacity = capacityValue;
                           candidate.initInstalledCapacity = capacityValue;
                       });
        if (isInvestmentAction(action))
        {
            pbg.getInvestmentCandidate(areaName, candidateName).type->expansionPotential
              = capacityValue;
        }
        else
        {
            pbg.getDecommissioningCandidate(areaName, candidateName).type->decommissioningPotential
              = capacityValue;
        }

        auto& area = pbg.areas.at(areaName);
        area.initializeRentability(action);

        // we check that the candidate has been correctly excluded from the selection
        EXPECT_TRUE(area.isCandidatesRentabilityEmpty());
    }

    void testDetermineCapacityAction(const double initInstalledCapacity,
                                     const double expansionPotential,
                                     const double decommissioningPotential,
                                     const CriterionState& criterionState,
                                     const std::optional<CapacityAction>& previousAreaAction,
                                     const std::optional<CapacityAction>& expectedCapacityAction)
    {
        const std::string areaName = "area2";
        auto pbg = makeProblemGeneration("with_decom_candidate",
                                         std::make_shared<ProblemManager>(),
                                         dataTestDir / "determine_capacity_action"
                                           / "input_balancing.yml");

        // set test data
        auto& investCandidate = pbg.getInvestmentCandidate(areaName, "invest_semibase");
        investCandidate.initInstalledCapacity = initInstalledCapacity;
        investCandidate.type->expansionPotential = expansionPotential;
        auto& decomCandidate = pbg.getDecommissioningCandidate(areaName, "unprofitable_peak");
        decomCandidate.initInstalledCapacity = initInstalledCapacity;
        decomCandidate.type->decommissioningPotential = decommissioningPotential;

        auto& area = pbg.areas.at(areaName);
        area.criterionState = criterionState;
        if (previousAreaAction)
        {
            area.lastAction = *previousAreaAction;
        }

        // we check that the correct action has been selected
        EXPECT_EQ(pbg.determineCapacityAction(area), expectedCapacityAction);
    }
};

TEST_F(BalancingTest, findAreaCandidatesToModify)
{
    testFindAreaCandidatesToModify();
}

TEST_F(BalancingTest, applyInvestmentActionToCandidateWithUpperonlyBound)
{
    testApplyActionToCandidate("area2",
                               "invest_semibase",
                               CapacityAction::INVESTMENT,
                               500,
                               1.0,
                               0.0,
                               3500,
                               0.0);
}

TEST_F(BalancingTest, applyInvestmentActionToCandidateWithBothBound)
{
    testApplyActionToCandidate("area2",
                               "invest_semibase",
                               CapacityAction::INVESTMENT,
                               500,
                               1.0,
                               0.9,
                               3500,
                               3150);
}

TEST_F(BalancingTest, applyInvestmentActionToCandidateWithFixedBound)
{
    testApplyActionToCandidate("area2",
                               "invest_semibase",
                               CapacityAction::INVESTMENT,
                               500,
                               0.8,
                               1.0,
                               2800,
                               2800);
}

TEST_F(BalancingTest, applyDisinvestmentActionToCandidate)
{
    testApplyActionToCandidate("area2",
                               "invest_semibase",
                               CapacityAction::DISINVESTMENT,
                               500,
                               1.0,
                               0.9,
                               2500,
                               2250);
}

TEST_F(BalancingTest, applyDecomActionToCandidate)
{
    testApplyActionToCandidate("area2",
                               "unprofitable_peak",
                               CapacityAction::DECOMMISSIONING,
                               500,
                               1.0,
                               0.9,
                               500,
                               450);
}

TEST_F(BalancingTest, applyRecomActionToCandidate)
{
    testApplyActionToCandidate("area2",
                               "unprofitable_peak",
                               CapacityAction::RECOMMISSIONING,
                               500,
                               1.0,
                               0.9,
                               1500,
                               1350);
}

TEST_F(BalancingTest, computeNonNullRentabilityForCandidates)
{
    // base
    testComputeRentabilityForCandidates(1, 1, 0, 0, 0, 1000, CapacityAction::INVESTMENT, 168);
    // change hourly solution value
    testComputeRentabilityForCandidates(2, 1, 0, 0, 0, 1000, CapacityAction::INVESTMENT, 336);
    // change hourly area price
    testComputeRentabilityForCandidates(1, 2, 0, 0, 0, 1000, CapacityAction::INVESTMENT, 336);
    // change investment/decom cost
    testComputeRentabilityForCandidates(1, 1, 1, 0, 0, 1000, CapacityAction::INVESTMENT, -832);
    // change fixed om cost
    testComputeRentabilityForCandidates(1, 1, 0, 1, 0, 1000, CapacityAction::INVESTMENT, -832);
    // change marginal cost
    testComputeRentabilityForCandidates(1, 1, 0, 0, 2, 1000, CapacityAction::INVESTMENT, -168);
    // change investment cost, fixed om cost and current capacity
    testComputeRentabilityForCandidates(1, 1, 0.5, 0.5, 0, 2000, CapacityAction::INVESTMENT, -1832);
    // change decommission cost, fixed om cost and capacity action
    testComputeRentabilityForCandidates(1,
                                        1,
                                        0.5,
                                        0.5,
                                        0,
                                        1000,
                                        CapacityAction::DECOMMISSIONING,
                                        -832);
}

TEST_F(BalancingTest, computeNullRentabilityForCandidate)
{
    // test with INVESTMENT action
    testGetNullRentabilityForCandidates("one_candidate_per_area",
                                        "area1",
                                        "invest_peak*1",
                                        CapacityAction::INVESTMENT);
    // test with DISINVESTMENT action
    testGetNullRentabilityForCandidates("one_candidate_per_area",
                                        "area1",
                                        "invest_peak*1",
                                        CapacityAction::DISINVESTMENT);
    // test with DECOM action
    testGetNullRentabilityForCandidates("with_decom_candidate",
                                        "area2",
                                        "unprofitable_peak",
                                        CapacityAction::DECOMMISSIONING);
    // test with RECOM action
    testGetNullRentabilityForCandidates("with_decom_candidate",
                                        "area2",
                                        "unprofitable_peak",
                                        CapacityAction::RECOMMISSIONING);
}

TEST_F(BalancingTest, determineCapacityAction)
{
    // arguments: initInstalledCapacity, expansionPotential, decommissioningPotential,
    //            criterionState, previous area action (nullopt: not set), expected action

    // isInvestmentCycle TRUE && isHigher TRUE -> Investment
    testDetermineCapacityAction(0,
                                3000,
                                0,
                                CriterionState::HIGHER,
                                CapacityAction::INVESTMENT,
                                CapacityAction::INVESTMENT);
    // isInvestmentCycle TRUE && isHigher TRUE && no invest possible -> Recommissioning
    testDetermineCapacityAction(5500,
                                2800,
                                0,
                                CriterionState::HIGHER,
                                std::nullopt,
                                CapacityAction::RECOMMISSIONING);
    // isInvestmentCycle TRUE && isHigher FALSE -> Disinvestment
    testDetermineCapacityAction(2500,
                                0,
                                0,
                                CriterionState::LOWER,
                                CapacityAction::INVESTMENT,
                                CapacityAction::DISINVESTMENT);
    // isInvestmentCycle TRUE && isHigher FALSE && no disinvest possible -> Decommissioning
    testDetermineCapacityAction(2800,
                                0,
                                4500,
                                CriterionState::LOWER,
                                CapacityAction::DISINVESTMENT,
                                CapacityAction::DECOMMISSIONING);
    // isInvestmentCycle FALSE && isHigher TRUE -> Recommissioning
    testDetermineCapacityAction(5500,
                                0,
                                0,
                                CriterionState::HIGHER,
                                CapacityAction::DECOMMISSIONING,
                                CapacityAction::RECOMMISSIONING);
    // isInvestmentCycle FALSE && isHigher TRUE && no recom possible -> Investment
    testDetermineCapacityAction(5000,
                                3000,
                                0,
                                CriterionState::HIGHER,
                                CapacityAction::RECOMMISSIONING,
                                CapacityAction::INVESTMENT);
    // isInvestmentCycle FALSE && isHigher FALSE -> Decommissioning
    testDetermineCapacityAction(0,
                                0,
                                4500,
                                CriterionState::LOWER,
                                CapacityAction::DECOMMISSIONING,
                                CapacityAction::DECOMMISSIONING);
    // isInvestmentCycle FALSE && isHigher TRUE && no decom possible -> DISINVESTMENT
    testDetermineCapacityAction(2500,
                                0,
                                5000,
                                CriterionState::LOWER,
                                std::nullopt,
                                CapacityAction::DISINVESTMENT);
    // no action available
    testDetermineCapacityAction(5000,
                                2800,
                                0,
                                CriterionState::HIGHER,
                                CapacityAction::INVESTMENT,
                                std::nullopt);
}
