
#include <chrono>
#include <iostream>
#include <tbb/global_control.h>

#include "antares-xpansion/balancing/BalancingParser.h"
#include "antares-xpansion/balancing/SettingsConfigReader.h"
#include "antares-xpansion/bellman_values/ProblemManager.h"
#include "antares-xpansion/benders/logger/MultithreadTBBLogger.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/exe_options/CommonExeOptions.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationForBalancing.h"

using namespace PlainData;

std::string formatTime(const std::chrono::system_clock::time_point& timePoint)
{
    // the <format> STL seems to not be available on all used compilers yet
    std::time_t tt = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm = *std::localtime(&tt); // Locale time-zone, usually UTC by default.
    return (std::stringstream() << std::put_time(&tm, "%T")).str();
}

template<typename T>
std::string formatDuration(std::chrono::duration<T> duration)
{
    // the <format> STL seems to not be available on all used compilers yet
    auto h = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= h;
    auto m = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= m;
    auto s = std::chrono::duration_cast<std::chrono::seconds>(duration);
    return (std::stringstream() << h.count() << "h, " << m.count() << "m, " << s.count() << "s")
      .str();
}

int main(int argc, char** argv)
{
    try
    {
        auto optionsParser = CommonExeOptions();
        optionsParser.Parse(argc, argv);
        auto studyPath = optionsParser.StudyPath();
        int nbThreads = optionsParser.NbThreads();

        // getting the maximum hardware concurrency (default)
        int max_thread_concurrency = tbb::global_control::active_value(
          tbb::global_control::max_allowed_parallelism);
        // limiting the number of TBB threads, as long as this instance is alive
        tbb::global_control thread_limiter(tbb::global_control::max_allowed_parallelism, nbThreads);
        // nbThreads shouldn't be larger than the maximum hardware concurrency
        nbThreads = std::min(nbThreads, max_thread_concurrency);

        const std::filesystem::path settingsConfigFilePath(studyPath
                                                           / "user/balancing/settings.yml");

        // SettingsConfigReader will check whether the settings.yaml file exists and
        // return default values if needed
        SettingsConfigReader scr(settingsConfigFilePath);
        std::string solverName = scr.getSolver();
        const std::string verbosity = scr.getVerbosity();
        const bool keepMps = scr.getKeepMps();
        const std::string problemFormat = scr.getProblemFormat();
        const bool cacheProblems = scr.getCacheProblems();
        const int max_iterations = scr.getMaxIterations();
        const auto areaFile = studyPath / "area.txt";

        ConfigurationManager::ConfigDirectories directories{
          .study_dir = studyPath,
          .simulation_dir = ConfigurationManager::generateOutputName(studyPath),
        };

        const std::filesystem::path balancingConfigFilePath(studyPath
                                                            / "user/balancing/input_balancing.yml");

        BalancingParser balParser(balancingConfigFilePath);

        if (!std::filesystem::exists(directories.simulation_dir))
        {
            std::filesystem::create_directories(directories.simulation_dir);
        }

        const std::string logSubFolder = "balancing_logs";
        const std::string logFilename = "balancing_log.txt";
        std::filesystem::create_directories(directories.simulation_dir / logSubFolder);
        // logs containing all values for every iteration
        const std::filesystem::path iterationsLogFilePath = directories.simulation_dir
                                                            / "iterations_values_log.csv";
        // logs containing all values reached at the end of the simulation
        const std::filesystem::path finalCriteriaFilePath = directories.simulation_dir
                                                            / "final_criteria.csv";

        std::shared_ptr<MultithreadTBBLogger> logger = std::make_shared<MultithreadTBBLogger>(
          directories.simulation_dir / logSubFolder,
          logFilename,
          nbThreads,
          LogUtils::StrToLogLevel(verbosity));

        auto startProblemGeneration = std::chrono::system_clock::now();
        logger->display_message("Generating problems (starting time: "
                                  + formatTime(startProblemGeneration) + ")",
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);
        auto problemManager = std::make_shared<ProblemManager>(solverName,
                                                               problemFormat,
                                                               keepMps,
                                                               cacheProblems,
                                                               directories.simulation_dir
                                                                 / "initial_problems");
        ProblemGenerationForBalancing pbg(directories,
                                          balParser.areaSettings,
                                          logger,
                                          problemManager,
                                          iterationsLogFilePath);
        auto endProblemGeneration = std::chrono::system_clock::now();
        logger->display_message("Problems generated", LogUtils::LOGLEVEL::INFO, logger->CONTEXT);
        std::chrono::duration<double> elapsed_seconds = endProblemGeneration
                                                        - startProblemGeneration;
        logger->display_message("Elapsed time for problem generation: "
                                  + formatDuration(elapsed_seconds),
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);

        std::map<Antares::Solver::WeeklyProblemId, PbOutput> res;
        // First iteration will be iteration 0 (the iteration before any modification is applied to
        // the problems)
        int iteration = -1;
        auto startBalancingProcess = std::chrono::system_clock::now();
        logger->display_message("Starting balancing process",
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);
        pbg.logCriterionAndAreaSettings(res);
        while (!pbg.isBalanced() && !pbg.isBlocked() && iteration < max_iterations)
        {
            iteration++;
            auto startIteration = std::chrono::system_clock::now();
            logger->display_message("Iteration " + std::to_string(iteration),
                                    LogUtils::LOGLEVEL::INFO,
                                    logger->CONTEXT);
            auto updatedProblemsManager = pbg.updateProblems(res);

            res = GreedyBalancingFinder(logger,
                                        balParser.areaSettings,
                                        balParser.getReliabilityStandardIndicator(),
                                        updatedProblemsManager,
                                        solverName,
                                        directories.simulation_dir,
                                        nbThreads)
                    .ComputeCriterionAndPrice();
            auto endIteration = std::chrono::system_clock::now();
            pbg.logCriterionAndAreaSettings(res);
            std::chrono::duration<double> elapsed_iteration_seconds = endIteration - startIteration;
            logger->display_message("Elapsed time for iteration " + std::to_string(iteration) + ": "
                                      + formatDuration(elapsed_iteration_seconds),
                                    LogUtils::LOGLEVEL::INFO,
                                    logger->CONTEXT);
            pbg.saveCriterionAndAreaSettingsToIterativeLogCSV(iteration);
            pbg.updateAreaCriteriaData(res);
        };
        logger->display_message("Final iteration " + std::to_string(++iteration),
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);
        pbg.logCriterionAndAreaSettings(res);
        pbg.saveClusterResultsToCSV(directories.simulation_dir / "final_capacities.csv");
        pbg.saveCriterionAndAreaSettingsToCSV(finalCriteriaFilePath);
        auto endProblemUpdate = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_update_seconds = endProblemUpdate
                                                               - startBalancingProcess;
        logger->display_message("Balancing process ended after " + std::to_string(iteration)
                                  + " iterations. In " + formatDuration(elapsed_update_seconds),
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);
        logger->display_message(pbg.isBalanced() ? "The system is balanced."
                                                 : "The system is not balanced.",
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);

        return 0;
    }
    catch (std::exception& e)
    {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!" << std::endl;
        return 1;
    }

    return 0;
}
