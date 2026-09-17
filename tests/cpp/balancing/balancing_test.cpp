#include "RandomDirGenerator.h"
#include "antares-xpansion/balancing/BalancingParser.h"
#include "antares-xpansion/balancing/SettingsConfigReader.h"
#include "antares-xpansion/bellman_values/ProblemManager.h"
#include "antares-xpansion/benders/logger/FilteredLogger.h"
#include "antares-xpansion/benders/logger/Master.h"
#include "antares-xpansion/benders/logger/User.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationForBalancing.h"
#include "gtest/gtest.h"

class BalancingTest: public ::testing::Test
{
public:
    Logger logger;
    std::filesystem::path tmpDir;
    const std::filesystem::path data_test_dir = "data_test/balancing";
    const std::string solverName = "xpress";

protected:
    std::filesystem::path original_dir;

    void SetUp() override
    {
        Logger std_out_logger;
        std_out_logger = std::make_shared<xpansion::logger::User>(std::cerr);
        auto master_logger = std::make_shared<xpansion::logger::Master>();
        master_logger->addLogger(std_out_logger);
        logger = std::make_shared<FilteredLogger>(master_logger, LogUtils::LOGLEVEL::INFO);
        original_dir = std::filesystem::current_path();
    }

    void TearDown() override
    {
        std::filesystem::current_path(original_dir);
    }

    void copyStudyData(const std::string& studyName)
    {
        std::filesystem::path data_dir = data_test_dir / studyName;
        tmpDir = CreateRandomSubDir(std::filesystem::temp_directory_path());

        std::filesystem::copy(data_dir,
                              tmpDir,
                              std::filesystem::copy_options::recursive
                                | std::filesystem::copy_options::overwrite_existing);
    }

    void testFindAreaClustersToModify()
    {
        logger->display_message("Testing of findAreaClusterToModify");

        std::vector<std::string> areasName = {"invest_area",
                                              "desinvest_area",
                                              "decom_area",
                                              "recom_area"};
        std::vector<std::string> candidatesName = {"candidate_1", "candidate_2"};

        // copy dummy data
        copyStudyData("one_candidate_per_area");
        // directories and path
        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };
        const std::filesystem::path iterLogFilePath = directories.simulation_dir
                                                      / "iterations_values_log.csv";
        const std::filesystem::path inputBalFilePath(tmpDir / "user/balancing/input_balancing.yml");
        // instantiation of ProblemGenerationForBalancing with dummy data
        BalancingParser dummyBalParser(inputBalFilePath);
        auto problemManager = std::make_shared<ProblemManager>("xpress",
                                                               "mps",
                                                               false,
                                                               true,
                                                               "initial_problems");
        ProblemGenerationForBalancing pbg = ProblemGenerationForBalancing(directories,
                                                                          dummyBalParser.areas,
                                                                          logger,
                                                                          problemManager,
                                                                          iterLogFilePath);

        // set real test data
        // set pbg.areas
        const std::filesystem::path balancingConfigFilePath(
          "data_test/balancing/find_area_cluster_to_modify/input_balancing.yml");
        BalancingParser balParser(balancingConfigFilePath);
        pbg.areas = balParser.areas;
        pbg.areas["desinvest_area"].investmentCandidates["candidate_1"].installedCapacity = 1000;
        pbg.areas["desinvest_area"].investmentCandidates["candidate_2"].installedCapacity = 1000;
        pbg.areas["decom_area"].decommissioningCandidates["candidate_1"].installedCapacity = 1000;
        pbg.areas["decom_area"].decommissioningCandidates["candidate_2"].installedCapacity = 1000;
        pbg.areas["recom_area"].decommissioningCandidates["candidate_1"].initInstalledCapacity
          = 1000;
        pbg.areas["recom_area"].decommissioningCandidates["candidate_2"].initInstalledCapacity
          = 1000;
        // set pbg.lastActionForArea
        std::map<std::string, CapacityAction> lastActionForArea = {
          {"invest_area", CapacityAction::INVESTMENT},
          {"desinvest_area", CapacityAction::INVESTMENT},
          {"decom_area", CapacityAction::DECOMMISSIONING},
          {"recom_area", CapacityAction::DECOMMISSIONING}};
        pbg.lastActionForArea = lastActionForArea;

        // set pbg.balancingData, probleManager.solutions_ and simuValues
        std::map<AreaCluster, BalancingData> balancingData;
        std::vector<double> solution(168 * 8);
        PbOutput pbOutput;
        std::map<std::string, int> areaCriterionValues = {{"invest_area", 5},
                                                          {"desinvest_area", 2},
                                                          {"decom_area", 2},
                                                          {"recom_area", 5}};
        pbOutput.areaCriterionValues = areaCriterionValues;
        std::array<double, NUMBER_OF_HOURS_PER_WEEK> areaPrices;
        for (int hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
        {
            areaPrices[hour] = 1;
        }

        int idx(0);
        for (std::string areaName: areasName)
        {
            pbOutput.areaPrices[areaName] = areaPrices;
            for (std::string candidateName: candidatesName)
            {
                std::array<size_t, NUMBER_OF_HOURS_PER_WEEK> areaClusterIndices;
                for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
                {
                    solution[idx] = (candidateName == "candidate_1") ? 1.0 : 0.0;
                    areaClusterIndices[hour] = idx;
                    idx += 1;
                }
                balancingData[{areaName, candidateName}].dispProdVarIndices = areaClusterIndices;
            }
        }
        pbg.balancingData = balancingData;
        Antares::Solver::WeeklyProblemId pbId({1, 1});
        pbg.problemManager->setProblemSolution(pbId, solution);
        std::map<Antares::Solver::WeeklyProblemId, PbOutput> simuValues = {{pbId, pbOutput}};
        // compute areaCriteriaData
        pbg.updateAreaCriteriaData(simuValues);
        // run findAreaClustersToModify
        std::map<AreaCluster, CapacityAction> areaClusterToModify = pbg.findAreaClustersToModify(
          simuValues);
        // assert results
        // we check that the correct candidate and action have been selected
        for (const auto& [areaCluster, action]: areaClusterToModify)
        {
            if (areaCluster.first == "invest_area")
            {
                EXPECT_TRUE(areaCluster.second == "candidate_1");
                EXPECT_TRUE(action == CapacityAction::INVESTMENT);
            }
            if (areaCluster.first == "desinvest_area")
            {
                EXPECT_TRUE(areaCluster.second == "candidate_2");
                EXPECT_TRUE(action == CapacityAction::DISINVESTMENT);
            }
            if (areaCluster.first == "decom_area")
            {
                EXPECT_TRUE(areaCluster.second == "candidate_2");
                EXPECT_TRUE(action == CapacityAction::DECOMMISSIONING);
            }
            if (areaCluster.first == "recom_area")
            {
                EXPECT_TRUE(areaCluster.second == "candidate_1");
                EXPECT_TRUE(action == CapacityAction::RECOMMISSIONING);
            }
        }
        // we check that the criterion state have been correctly set
        for (const auto& [areaName, area]: pbg.areas)
        {
            if (areaName == "invest_area" || areaName == "recom_area")
            {
                EXPECT_TRUE(area.oldCriterionState == CriterionState::HIGHER);
            }
            if (areaName == "desinvest_area" || areaName == "decom_area")
            {
                EXPECT_TRUE(area.oldCriterionState == CriterionState::LOWER);
            }
        }
        logger->display_message("Test of findAreaClusterToModify done!");
    }

    void assertCandidateBounds(std::shared_ptr<ProblemManager> problemManager,
                               std::vector<int> candidateIndices,
                               double expectedUpperBound,
                               double expectedLowerBound)
    {
        double upperBound;
        double lowerBound;
        for (const auto& pbId: problemManager->getProblemIds())
        {
            std::shared_ptr<Problem> problem = problemManager->getProblemFromId(pbId);
            for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
            {
                problem->get_ub(&upperBound, candidateIndices[hour], candidateIndices[hour]);
                EXPECT_TRUE(upperBound == expectedUpperBound);
                problem->get_lb(&lowerBound, candidateIndices[hour], candidateIndices[hour]);
                EXPECT_TRUE(lowerBound == expectedLowerBound);
            }
        }
    }

    void testApplyActionToCluster(const std::string& areaName,
                                  const std::string& candidateName,
                                  const CapacityAction& action,
                                  const double capacityIncrement,
                                  const double uBoundRatioToInstCap,
                                  const double lBoundRatioToUpBound,
                                  const double expectedUpperCapacity,
                                  const double expectedLowerCapacity)
    {
        // copy study data
        copyStudyData("with_decom_candidate");
        // directories and path
        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };
        const std::filesystem::path iterLogFilePath = directories.simulation_dir
                                                      / "iterations_values_log.csv";
        const std::filesystem::path inputBalFilePath(tmpDir / "user/balancing/input_balancing.yml");
        // instantiation of ProblemGenerationForBalancing
        BalancingParser balParser(inputBalFilePath);
        auto problemManager = std::make_shared<ProblemManager>();
        ProblemGenerationForBalancing pbg = ProblemGenerationForBalancing(directories,
                                                                          balParser.areas,
                                                                          logger,
                                                                          problemManager,
                                                                          iterLogFilePath);

        // set pbg.areas
        double newBoundRef;
        int multForUpperBoundLocation;
        pbg.areas[areaName].currentInvestmentIncrement = capacityIncrement;
        if (action == CapacityAction::INVESTMENT || action == CapacityAction::DISINVESTMENT)
        {
            pbg.areas[areaName].investmentCandidates[candidateName].initInstalledCapacity
              = 4 * capacityIncrement; // we set initInstalledCapacity lower to allow disinvestment
            pbg.areas[areaName].currentInvestmentIncrement = capacityIncrement;
            pbg.areas[areaName].investmentCandidates[candidateName].installedCapacity
              = 6 * capacityIncrement;
            for (const auto& pbId: problemManager->getProblemIds())
            {
                std::vector<BoundData> oneWeekBoundData(NUMBER_OF_HOURS_PER_WEEK);
                for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
                {
                    oneWeekBoundData[hour].lowBoundRatioToUpBound = lBoundRatioToUpBound;
                    oneWeekBoundData[hour].upBoundRatioToInstalledCap = uBoundRatioToInstCap;
                }
                pbg.areas[areaName].investmentCandidates[candidateName].setOneWeekBoundsData(
                  pbId,
                  oneWeekBoundData);
            }
        }
        else
        {
            pbg.areas[areaName].decommissioningCandidates[candidateName].initInstalledCapacity
              = 4 * capacityIncrement; // we set initInstalledCapacity higher to allow recom
            pbg.areas[areaName].currentDecommissioningIncrement = capacityIncrement;
            pbg.areas[areaName].decommissioningCandidates[candidateName].installedCapacity
              = 2 * capacityIncrement;
            for (const auto& pbId: problemManager->getProblemIds())
            {
                std::vector<BoundData> oneWeekBoundsData(NUMBER_OF_HOURS_PER_WEEK);
                for (size_t hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
                {
                    oneWeekBoundsData[hour].lowBoundRatioToUpBound = lBoundRatioToUpBound;
                    oneWeekBoundsData[hour].upBoundRatioToInstalledCap = uBoundRatioToInstCap;
                }
                pbg.areas[areaName].decommissioningCandidates[candidateName].setOneWeekBoundsData(
                  pbId,
                  oneWeekBoundsData);
            }
        }

        const auto& varIndices = pbg.balancingData.at({areaName, candidateName}).dispProdVarIndices;
        auto& area = pbg.areas.at(areaName);
        std::vector<int> vecIndices(varIndices.begin(), varIndices.end());

        // run applyActionToCluster
        pbg.applyActionToCluster({areaName, candidateName}, action);
        // assert results
        // we check that new bound of candidate have been correctly set
        assertCandidateBounds(pbg.problemManager,
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
        std::string studyName = "with_decom_candidate";
        std::string areaName = "area2";
        std::string candidateName = action == CapacityAction::INVESTMENT ? "invest_peak2"
                                                                         : "unprofitable_peak";
        // copy dummy data
        copyStudyData(studyName);

        // directories and path
        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };
        const std::filesystem::path iterLogFilePath = directories.simulation_dir
                                                      / "iterations_values_log.csv";
        const std::filesystem::path inputBalFilePath(tmpDir / "user/balancing/input_balancing.yml");
        // instantiation of ProblemGenerationForBalancing
        BalancingParser balParser(inputBalFilePath);
        auto problemManager = std::make_shared<ProblemManager>("xpress",
                                                               "mps",
                                                               false,
                                                               true,
                                                               "initial_problems");
        ProblemGenerationForBalancing pbg = ProblemGenerationForBalancing(directories,
                                                                          balParser.areas,
                                                                          logger,
                                                                          problemManager,
                                                                          iterLogFilePath);

        // set test data
        // set investment cost and fixed om cost
        if (action == CapacityAction::INVESTMENT)
        {
            pbg.areas[areaName].investmentCandidates[candidateName].installedCapacity
              = installedCapacity;
            pbg.areas[areaName].investmentCandidates[candidateName].type->investmentCost
              = investmentCost;
            pbg.areas[areaName].investmentCandidates[candidateName].type->fixedOmCosts
              = fixedOmCosts;
        }
        else
        {
            pbg.areas[areaName].decommissioningCandidates[candidateName].installedCapacity
              = installedCapacity;
            pbg.areas[areaName].decommissioningCandidates[candidateName].type->decommissioningCost
              = investmentCost;
            pbg.areas[areaName].decommissioningCandidates[candidateName].type->fixedOmCosts
              = fixedOmCosts;
        }
        // set marginalCost
        pbg.balancingData.at({areaName, candidateName}).marginalCost = marginalCost;
        // set probleManager.solutions_
        Antares::Solver::WeeklyProblemId pbId({1, 1});
        // set arbitrary size of solution big enough to cover candidate indices in each study
        std::vector<double> solution(3100, 0);
        for (const auto& idx: pbg.balancingData[{areaName, candidateName}].dispProdVarIndices)
        {
            solution[idx] = hourlySolutionValue;
        }
        pbg.problemManager->setProblemSolution(pbId, solution);
        // set simuValues
        PbOutput pbOutput;
        std::array<double, NUMBER_OF_HOURS_PER_WEEK> areaPrices;
        for (int hour = 0; hour < NUMBER_OF_HOURS_PER_WEEK; ++hour)
        {
            areaPrices[hour] = hourlyAreaPrice;
        }
        pbOutput.areaPrices[areaName] = areaPrices;
        std::map<Antares::Solver::WeeklyProblemId, PbOutput> simuValues = {{pbId, pbOutput}};
        // run computeRentabilityForCandidates
        std::map<std::string, double> rentability;
        if (action == CapacityAction::INVESTMENT)
        {
            rentability = pbg.computeRentabilityForCandidates(
              areaName,
              pbg.areas[areaName].investmentCandidates,
              simuValues,
              action);
        }
        else
        {
            rentability = pbg.computeRentabilityForCandidates(
              areaName,
              pbg.areas[areaName].decommissioningCandidates,
              simuValues,
              action);
        }
        // assert results
        // we check that the rentability have been correctly computed
        EXPECT_TRUE(rentability[candidateName] == expectedRentability);
    }

    void testGetNullRentabilityForCandidates(const std::string& studyFolderName,
                                             const std::string& areaName,
                                             const std::string& candidateName,
                                             const CapacityAction& action)

    {
        // constant used to set capacity data at the same values
        double capacityValue = 1000;

        // copy study data
        copyStudyData(studyFolderName);
        // directories and path
        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };
        const std::filesystem::path iterLogFilePath = directories.simulation_dir
                                                      / "iterations_values_log.csv";
        const std::filesystem::path inputBalFilePath(tmpDir / "user/balancing/input_balancing.yml");
        // instantiation of ProblemGenerationForBalancing
        BalancingParser balParser(inputBalFilePath);
        auto problemManager = std::make_shared<ProblemManager>();
        ProblemGenerationForBalancing pbg = ProblemGenerationForBalancing(directories,
                                                                          balParser.areas,
                                                                          logger,
                                                                          problemManager,
                                                                          iterLogFilePath);

        // set simuValues
        PbOutput pbOutput = {};
        Antares::Solver::WeeklyProblemId pbId({1, 1});
        std::map<Antares::Solver::WeeklyProblemId, PbOutput> simuValues = {{pbId, pbOutput}};
        // set candidate capacity data and run computeRentabilityForCandidates
        std::map<std::string, double> rentability;
        if (action == CapacityAction::INVESTMENT || action == CapacityAction::DISINVESTMENT)
        {
            pbg.areas[areaName].investmentCandidates[candidateName].installedCapacity
              = capacityValue;
            pbg.areas[areaName].investmentCandidates[candidateName].initInstalledCapacity
              = capacityValue;
            pbg.areas[areaName].investmentCandidates[candidateName].type->expansionPotential
              = capacityValue;
            rentability = pbg.computeRentabilityForCandidates(
              areaName,
              pbg.areas[areaName].investmentCandidates,
              simuValues,
              action);
        }
        else
        {
            pbg.areas[areaName].decommissioningCandidates[candidateName].installedCapacity
              = capacityValue;
            pbg.areas[areaName].decommissioningCandidates[candidateName].initInstalledCapacity
              = capacityValue;
            pbg.areas[areaName]
              .decommissioningCandidates[candidateName]
              .type->decommissioningPotential
              = capacityValue;
            rentability = pbg.computeRentabilityForCandidates(
              areaName,
              pbg.areas[areaName].decommissioningCandidates,
              simuValues,
              action);
        }
        // assert results
        // we check that the candidate has been correctly excluded from the selection
        EXPECT_TRUE(rentability.empty());
    }

    void testDetermineCapacityAction(const double initInstalledCapacity,
                                     const double expansionPotential,
                                     const double decommissioningPotential,
                                     const CriterionState& criterionState,
                                     const bool setLastAction,
                                     const CapacityAction previousAreaAction,
                                     const std::optional<CapacityAction> expectedCapacityAction)
    {
        // copy study data
        copyStudyData("with_decom_candidate");
        // directories and path
        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };
        const std::filesystem::path iterLogFilePath = directories.simulation_dir
                                                      / "iterations_values_log.csv";
        const std::filesystem::path inputBalFilePath(
          "data_test/balancing/determine_capacity_action/input_balancing.yml");
        // instantiation of ProblemGenerationForBalancing
        BalancingParser balParser(inputBalFilePath);
        auto problemManager = std::make_shared<ProblemManager>();
        ProblemGenerationForBalancing pbg = ProblemGenerationForBalancing(directories,
                                                                          balParser.areas,
                                                                          logger,
                                                                          problemManager,
                                                                          iterLogFilePath);
        // set pbg.areas
        pbg.areas["area2"].investmentCandidates["invest_semibase"].initInstalledCapacity
          = initInstalledCapacity;
        pbg.areas["area2"].investmentCandidates["invest_semibase"].type->expansionPotential
          = expansionPotential;
        pbg.areas["area2"].decommissioningCandidates["unprofitable_peak"].initInstalledCapacity
          = initInstalledCapacity;
        pbg.areas["area2"]
          .decommissioningCandidates["unprofitable_peak"]
          .type->decommissioningPotential
          = decommissioningPotential;
        //  isInvestmentCycle
        if (setLastAction)
        {
            pbg.lastActionForArea["area2"] = previousAreaAction;
        }
        // run determineCapacityAction
        std::optional<CapacityAction> resCapacityAction = pbg.determineCapacityAction(
          "area2",
          criterionState,
          pbg.areas["area2"]);
        // assert results
        // we check that the correct action has been selected
        EXPECT_TRUE(resCapacityAction == expectedCapacityAction);
    }
};

TEST_F(BalancingTest, findAreaClustersToModify)
{
    testFindAreaClustersToModify();
}

TEST_F(BalancingTest, applyInvestmentActionToClusterWithUpperonlyBound)
{
    logger->display_message("Testing of applyActionToCluster with INVESTMENT and UpperOnly bound");
    testApplyActionToCluster("area2",
                             "invest_semibase",
                             CapacityAction::INVESTMENT,
                             500,
                             1.0,
                             0.0,
                             3500,
                             0.0);
    logger->display_message(
      "Test of applyActionToCluster with INVESTMENT and UpperOnly bound done!");
}

TEST_F(BalancingTest, applyInvestmentActionToClusterWithBothBound)
{
    logger->display_message("Testing of applyActionToCluster with INVESTMENT and Both bound");
    testApplyActionToCluster("area2",
                             "invest_semibase",
                             CapacityAction::INVESTMENT,
                             500,
                             1.0,
                             0.9,
                             3500,
                             3150);
    logger->display_message("Test of applyActionToCluster with INVESTMENT and Both bound done!");
}

TEST_F(BalancingTest, applyInvestmentActionToClusterWithFixedBound)
{
    logger->display_message("Testing of applyActionToCluster with INVESTMENT and Fixed bound");
    testApplyActionToCluster("area2",
                             "invest_semibase",
                             CapacityAction::INVESTMENT,
                             500,
                             0.8,
                             1.0,
                             2800,
                             2800);
    logger->display_message("Test of applyActionToCluster with INVESTMENT and Fixed bound done!");
}

TEST_F(BalancingTest, applyDisinvestmentActionToCluster)
{
    logger->display_message("Testing of applyActionToCluster with DISINVESTMENT");
    testApplyActionToCluster("area2",
                             "invest_semibase",
                             CapacityAction::DISINVESTMENT,
                             500,
                             1.0,
                             0.9,
                             2500,
                             2250);
    logger->display_message("Test of applyActionToCluster with DISINVESTMENT done!");
}

TEST_F(BalancingTest, applyDecomActionToCluster)
{
    logger->display_message("Testing of applyActionToCluster with DECOM");
    testApplyActionToCluster("area2",
                             "unprofitable_peak",
                             CapacityAction::DECOMMISSIONING,
                             500,
                             1.0,
                             0.9,
                             500,
                             450);
    logger->display_message("Test of applyActionToCluster with DECOM done!");
}

TEST_F(BalancingTest, applyRecomActionToCluster)
{
    logger->display_message("Testing of applyActionToCluster with RECOM");
    testApplyActionToCluster("area2",
                             "unprofitable_peak",
                             CapacityAction::RECOMMISSIONING,
                             500,
                             1.0,
                             0.9,
                             1500,
                             1350);
    logger->display_message("Test of applyActionToCluster with RECOM done!");
}

TEST_F(BalancingTest, computeNonNullRentabilityForCandidates)
{
    logger->display_message("Testing compute of non null rentability for candidates");
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
    logger->display_message("Test of compute of non null rentability for candidates done!");
}

TEST_F(BalancingTest, computeNullRentabilityForCandidate)
{
    logger->display_message("Testing compute of null rentability for candidates");
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
    logger->display_message("Test of compute null rentability for candidates done!");
}

TEST_F(BalancingTest, determineCapacityAction)
{
    logger->display_message("Testing of determineCapacityAction");
    // isInvestmentCycle TRUE && isHigher TRUE -> Investment
    testDetermineCapacityAction(0,
                                3000,
                                0,
                                CriterionState::HIGHER,
                                true,
                                CapacityAction::INVESTMENT,
                                CapacityAction::INVESTMENT);
    // isInvestmentCycle TRUE && isHigher TRUE && no invest possible -> Recommissioning
    testDetermineCapacityAction(5500,
                                2800,
                                0,
                                CriterionState::HIGHER,
                                false,
                                CapacityAction::RECOMMISSIONING,
                                CapacityAction::RECOMMISSIONING);
    // isInvestmentCycle TRUE && isHigher FALSE -> Disinvestment
    testDetermineCapacityAction(2500,
                                0,
                                0,
                                CriterionState::LOWER,
                                true,
                                CapacityAction::INVESTMENT,
                                CapacityAction::DISINVESTMENT);
    // isInvestmentCycle TRUE && isHigher FALSE && no disinvest possible -> Decommissioning
    testDetermineCapacityAction(2800,
                                0,
                                4500,
                                CriterionState::LOWER,
                                true,
                                CapacityAction::DISINVESTMENT,
                                CapacityAction::DECOMMISSIONING);
    // isInvestmentCycle FALSE && isHigher TRUE -> Recommissioning
    testDetermineCapacityAction(5500,
                                0,
                                0,
                                CriterionState::HIGHER,
                                true,
                                CapacityAction::DECOMMISSIONING,
                                CapacityAction::RECOMMISSIONING);
    // isInvestmentCycle FALSE && isHigher TRUE && no recom possible -> Investment
    testDetermineCapacityAction(5000,
                                3000,
                                0,
                                CriterionState::HIGHER,
                                true,
                                CapacityAction::RECOMMISSIONING,
                                CapacityAction::INVESTMENT);
    // isInvestmentCycle FALSE && isHigher FALSE -> Decommissioning
    testDetermineCapacityAction(0,
                                0,
                                4500,
                                CriterionState::LOWER,
                                true,
                                CapacityAction::DECOMMISSIONING,
                                CapacityAction::DECOMMISSIONING);
    // isInvestmentCycle FALSE && isHigher TRUE && no decom possible -> DISINVESTMENT
    testDetermineCapacityAction(2500,
                                0,
                                5000,
                                CriterionState::LOWER,
                                false,
                                CapacityAction::DECOMMISSIONING,
                                CapacityAction::DISINVESTMENT);
    // no action available
    testDetermineCapacityAction(5000,
                                2800,
                                0,
                                CriterionState::HIGHER,
                                true,
                                CapacityAction::INVESTMENT,
                                std::nullopt);
    logger->display_message("Test of determineCapacityAction done!");
}
