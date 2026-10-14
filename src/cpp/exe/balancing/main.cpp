
#include <chrono>
#include <iostream>
#include <tbb/global_control.h>

#include "antares-xpansion/balancing/InputBalancingParser.h"
#include "antares-xpansion/balancing/SettingsConfigReader.h"
#include "antares-xpansion/balancing/utils.h"
#include "antares-xpansion/bellman_values/ProblemManager.h"
#include "antares-xpansion/benders/logger/MultithreadTBBLogger.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/exe_options/CommonExeOptions.h"
#include "antares-xpansion/lpnamer/main/Balancing.h"

std::string formatTime(const std::chrono::system_clock::time_point& timePoint)
{
    // the <format> STL seems to not be available on all used compilers yet
    std::time_t tt = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm = *std::localtime(&tt); // Locale time-zone, usually UTC by default.
    return (std::stringstream() << std::put_time(&tm, "%T")).str();
}

int clampThreadCount(int requested)
{
    const int hardwareMax = static_cast<int>(
      tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism));
    return std::clamp(requested, 1, std::max(1, hardwareMax));
}

int main(int argc, char** argv)
{
    try
    {
        auto optionsParser = CommonExeOptions();
        optionsParser.Parse(argc, argv);
        auto studyPath = optionsParser.StudyPath();

        // Clamp BEFORE creating the limiter, otherwise TBB would be limited with the raw value.
        const int nbThreads = clampThreadCount(optionsParser.NbThreads());
        // Limits TBB threads as long as this instance is alive
        tbb::global_control threadLimiter(tbb::global_control::max_allowed_parallelism, nbThreads);

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
        const int maxIterations = scr.getMaxIterations();
        const auto areaFile = studyPath / "area.txt";

        ConfigurationManager::ConfigDirectories directories{
          .study_dir = studyPath,
          .simulation_dir = ConfigurationManager::generateOutputName(studyPath),
        };

        const std::filesystem::path balancingConfigFilePath(studyPath
                                                            / "user/balancing/input_balancing.yml");

        InputBalancingParser inputBalancingParser(balancingConfigFilePath);

        std::map<std::string, Area> areas = inputBalancingParser.parse();

        if (!std::filesystem::exists(directories.simulation_dir))
        {
            std::filesystem::create_directories(directories.simulation_dir);
        }

        const std::string logSubFolder = "balancing_logs";
        const std::string logFilename = "balancing_log.txt";
        std::filesystem::create_directories(directories.simulation_dir / logSubFolder);

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
        Balancing pbg = Balancing(directories, areas, logger, problemManager);
        GreedyBalancingFinder gbf = GreedyBalancingFinder(
          logger,
          areas,
          inputBalancingParser.getReliabilityStandardIndicator(),
          problemManager,
          solverName,
          directories.simulation_dir,
          nbThreads);

        auto endProblemGeneration = std::chrono::system_clock::now();
        logger->display_message("Problems generated", LogUtils::LOGLEVEL::INFO, logger->CONTEXT);
        std::chrono::duration<double> elapsed_seconds = endProblemGeneration
                                                        - startProblemGeneration;
        logger->display_message("Elapsed time for problem generation: "
                                  + formatDuration(elapsed_seconds),
                                LogUtils::LOGLEVEL::INFO,
                                logger->CONTEXT);
        pbg.run(maxIterations, gbf);
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
