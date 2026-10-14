#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tbb/global_control.h>

#include "antares-xpansion/balancing/InputBalancingParser.h"
#include "antares-xpansion/balancing/SettingsConfigReader.h"
#include "antares-xpansion/balancing/utils.h"
#include "antares-xpansion/bellman_values/ProblemManager.h"
#include "antares-xpansion/benders/logger/MultithreadTBBLogger.h"
#include "antares-xpansion/evaluator/GreedyBalancingFinder.h"
#include "antares-xpansion/exe_options/CommonExeOptions.h"
#include "antares-xpansion/lpnamer/main/Balancing.h"

namespace
{
constexpr const char* SETTINGS_FILE = "user/balancing/settings.yml";
constexpr const char* INPUT_BALANCING_FILE = "user/balancing/input_balancing.yml";
constexpr const char* LOG_SUB_FOLDER = "balancing_logs";
constexpr const char* LOG_FILENAME = "balancing_log.txt";
constexpr const char* INITIAL_PROBLEMS_FOLDER = "initial_problems";

using LoggerPtr = std::shared_ptr<MultithreadTBBLogger>;

std::string formatTime(const std::chrono::system_clock::time_point& timePoint)
{
    // the <format> STL seems to not be available on all used compilers yet
    const std::time_t tt = std::chrono::system_clock::to_time_t(timePoint);
    const std::tm tm = *std::localtime(&tt); // Local time zone
    std::ostringstream oss;
    oss << std::put_time(&tm, "%T");
    return oss.str();
}

int clampThreadCount(int requested)
{
    const int hardwareMax = static_cast<int>(
      tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism));
    return std::clamp(requested, 1, std::max(1, hardwareMax));
}

void logInfo(const LoggerPtr& logger, const std::string& message)
{
    logger->display_message(message, LogUtils::LOGLEVEL::INFO, logger->CONTEXT);
}

LoggerPtr createLogger(const std::filesystem::path& simulationDir,
                       int nbThreads,
                       const std::string& verbosity)
{
    const auto logDir = simulationDir / LOG_SUB_FOLDER;
    std::filesystem::create_directories(logDir);
    return std::make_shared<MultithreadTBBLogger>(logDir,
                                                  LOG_FILENAME,
                                                  nbThreads,
                                                  LogUtils::StrToLogLevel(verbosity));
}

int runBalancing(int argc, char** argv)
{
    CommonExeOptions optionsParser;
    optionsParser.Parse(argc, argv);
    const auto studyPath = optionsParser.StudyPath();

    // Clamp BEFORE creating the limiter, otherwise TBB would be limited with the raw value.
    const int nbThreads = clampThreadCount(optionsParser.NbThreads());
    // Limits TBB threads as long as this instance is alive
    tbb::global_control threadLimiter(tbb::global_control::max_allowed_parallelism, nbThreads);

    // SettingsConfigReader checks whether the settings file exists
    // and returns default values if needed
    SettingsConfigReader settings(studyPath / SETTINGS_FILE);
    const std::string solverName = settings.getSolver();
    const std::string problemFormat = settings.getProblemFormat();
    const std::string verbosity = settings.getVerbosity();
    const bool keepMps = settings.getKeepMps();
    const bool cacheProblems = settings.getCacheProblems();
    const int maxIterations = settings.getMaxIterations();

    InputBalancingParser inputBalancingParser(studyPath / INPUT_BALANCING_FILE);
    const std::map<std::string, Area> areas = inputBalancingParser.parse();
    Benders::Criterion::Type relibilityStandardIndicator = inputBalancingParser
                                                             .getReliabilityStandardIndicator();

    const ConfigurationManager::ConfigDirectories directories{
      .study_dir = studyPath,
      .simulation_dir = ConfigurationManager::generateOutputName(studyPath),
    };
    std::filesystem::create_directories(directories.simulation_dir);

    const auto logger = createLogger(directories.simulation_dir, nbThreads, verbosity);
    const auto startProblemGeneration = std::chrono::system_clock::now();
    logInfo(logger,
            "Generating problems (starting time: " + formatTime(startProblemGeneration) + ")");

    auto problemManager = std::make_shared<ProblemManager>(solverName,
                                                           problemFormat,
                                                           keepMps,
                                                           cacheProblems,
                                                           directories.simulation_dir
                                                             / INITIAL_PROBLEMS_FOLDER);
    Balancing balancing(directories, areas, logger, problemManager);
    GreedyBalancingFinder balancingFinder(logger,
                                          areas,
                                          relibilityStandardIndicator,
                                          problemManager,
                                          solverName,
                                          directories.simulation_dir,
                                          nbThreads);

    const std::chrono::duration<double> elapsed = std::chrono::system_clock::now()
                                                  - startProblemGeneration;
    logInfo(logger, "Problems generated");
    logInfo(logger, "Elapsed time for problem generation: " + formatDuration(elapsed));

    balancing.run(maxIterations, balancingFinder);
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        return runBalancing(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!" << std::endl;
    }
    return 1;
}
