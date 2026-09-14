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

class BalancingTestEndToEnd: public ::testing::Test
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

    std::ifstream openFileWithChecks(const std::filesystem::path& fileName)
    {
        std::ifstream file(tmpDir / fileName);
        EXPECT_TRUE(file);
        if (!file)
        {
            throw std::runtime_error("File does not exist at " + (tmpDir / fileName).string());
        }
        return file;
    }

    void compareOutputFileToRef(const std::filesystem::path& outputFileName,
                                const std::filesystem::path& refFileName)
    {
        logger->display_message("\nComparing files " + outputFileName.string() + " and "
                                + refFileName.string() + "...");
        std::string errorPrefix = "File " + (tmpDir / outputFileName).string() + ": ";
        // open result file
        std::ifstream outputFile = openFileWithChecks(outputFileName);

        // open reference file
        std::ifstream refFile = openFileWithChecks(refFileName);

        std::string refLine, outputLine;
        unsigned int lineCount = 1;

        while (std::getline(refFile, refLine))
        {
            logger->display_message("Comparing line " + std::to_string(lineCount));
            EXPECT_TRUE(std::getline(outputFile, outputLine));
            EXPECT_STREQ(refLine.c_str(), outputLine.c_str());
            if (refLine != outputLine)
            {
                std::stringstream refStringStream(refLine);
                std::stringstream outputStringStream(outputLine);
                std::string refToken, outputToken;
                unsigned int col = 1;
                while (std::getline(refStringStream, refToken, ','))
                {
                    EXPECT_TRUE(std::getline(outputStringStream, outputToken, ','));

                    if (refToken != outputToken)
                    {
                        logger->display_message((std::stringstream()
                                                 << errorPrefix << "Line " << lineCount << " col "
                                                 << col << ": expected value '" << refToken
                                                 << "', got '" << outputToken << "'\n")
                                                  .str());
                    }
                    col++;
                }
                if (std::getline(outputStringStream, outputToken, ','))
                {
                    logger->display_message(
                      (std::stringstream() << errorPrefix << "Output file has extra value at line "
                                           << lineCount << " col " << col << "\n")
                        .str());
                }
            }
            ++lineCount;
        }
        EXPECT_FALSE(std::getline(outputFile, outputLine));
        logger->display_message("Comparison done.");
    }

    void runStudyAndCompare(const std::string& studyFolderName)
    {
        logger->display_message("Testing of study " + studyFolderName + "...");
        copyStudyData(studyFolderName);

        // directories
        ConfigurationManager::ConfigDirectories directories{
          .study_dir = tmpDir,
          .simulation_dir = ConfigurationManager::generateOutputName(tmpDir),
        };

        // parsing .yml files
        const std::filesystem::path balancingConfigFilePath(tmpDir
                                                            / "user/balancing/input_balancing.yml");
        const std::filesystem::path settingsConfigFilePath(tmpDir / "user/balancing/settings.yml");

        BalancingParser balParser(balancingConfigFilePath);
        SettingsConfigReader scr(settingsConfigFilePath);
        const int max_iterations = scr.getMaxIterations();

        if (!std::filesystem::exists(directories.simulation_dir))
        {
            std::filesystem::create_directories(directories.simulation_dir);
        }

        // logs and result files
        const std::string logSubFolder = "balancing_logs";
        const std::string logFilename = "balancing_log.txt";
        std::filesystem::create_directories(directories.simulation_dir / logSubFolder);
        // logs containing all values for every iteration
        const std::filesystem::path iterationsLogFilePath = directories.simulation_dir
                                                            / "iterations_values_log.csv";
        // logs containing all values reached at the end of the simulation
        const std::filesystem::path finalCriteriaFilePath = directories.simulation_dir
                                                            / "final_criteria.csv";

        // generating problems
        logger->display_message("Generating problems...");
        auto problemManager = std::make_shared<ProblemManager>(solverName);
        ProblemGenerationForBalancing pbg(directories,
                                          balParser.areaSettings,
                                          logger,
                                          problemManager,
                                          iterationsLogFilePath);
        logger->display_message("Problems generated.");

        // balancing
        std::map<Antares::Solver::WeeklyProblemId, PbOutput> res;
        // First iteration will be iteration 0 (the iteration before any modification is applied to
        // the problems)
        int iteration = -1;
        logger->display_message("Starting balancing process");
        pbg.logCriterionAndAreaSettings(res);
        while (!pbg.isBalanced() && !pbg.isBlocked() && iteration < max_iterations)
        {
            iteration++;
            logger->display_message("Iteration " + std::to_string(iteration));
            auto updatedProblemsManager = pbg.updateProblems(res);

            res = GreedyBalancingFinder(logger,
                                        balParser.areaSettings,
                                        balParser.getReliabilityStandardIndicator(),
                                        updatedProblemsManager,
                                        solverName,
                                        directories.simulation_dir,
                                        8)
                    .ComputeCriterionAndPrice();
            pbg.logCriterionAndAreaSettings(res);
            logger->display_message("Iteration " + std::to_string(iteration) + " done.");
            pbg.saveCriterionAndAreaSettingsToIterativeLogCSV(iteration);
            pbg.updateAreaCriteriaData(res);
        };
        // saving final results
        pbg.saveClusterResultsToCSV(directories.simulation_dir / "final_capacities.csv");
        pbg.saveCriterionAndAreaSettingsToCSV(finalCriteriaFilePath);

        // compare results to ref
        logger->display_message("\nComparing results files...");
        // cluster results
        compareOutputFileToRef(directories.simulation_dir / "final_capacities.csv",
                               tmpDir / "final_capacities_ref.csv");
        // criterion and area results
        compareOutputFileToRef(finalCriteriaFilePath, tmpDir / "final_criteria_ref.csv");
        // iterative logs
        compareOutputFileToRef(iterationsLogFilePath, tmpDir / "iterations_values_log_ref.csv");

        logger->display_message("Test of study " + studyFolderName + " done!");
    }
};

TEST_F(BalancingTestEndToEnd, OneCandidatePerArea)

{
    runStudyAndCompare("one_candidate_per_area");
}

TEST_F(BalancingTestEndToEnd, TwoCandidatesPerArea)
{
    runStudyAndCompare("two_candidates_per_area");
}

TEST_F(BalancingTestEndToEnd, WithDecomCandidate)
{
    runStudyAndCompare("with_decom_candidate");
}
